#!/usr/bin/env python3
"""@file benchmark_split_runtime.py
@brief Compare split-runtime startup cycles using a temporary 16 KiB emulator.

Builds an instrumented copy, counts emulated Z80 T-states to main(), checks a
rendered fixture page, verifies unpacked RAM bytes, and samples clock formatting.
Reported milliseconds assume 2.5 MHz and exclude real hardware wait states.

SPDX-License-Identifier: GPL-3.0-only
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

GLOBALS = """unsigned bench_main_pc = 0, bench_function_pc = 0;
unsigned bench_min_sp = 0xffff, bench_function_return = 0;
unsigned long long bench_tstates = 0, bench_main_tstates = 0;
unsigned long long bench_function_started = 0;
unsigned long long bench_function_cycles = 0, bench_function_calls = 0;

"""
PROBE = """    if (R.SP.D >= 0x7000 && R.SP.D < bench_min_sp)
      bench_min_sp = R.SP.D;
    if (R.PC.D == bench_main_pc && !bench_main_tstates)
      bench_main_tstates = bench_tstates;
    if (bench_function_return && R.PC.D == bench_function_return) {
      bench_function_cycles += bench_tstates - bench_function_started;
      ++bench_function_calls;
      bench_function_return = 0;
    }
    if (R.PC.D == bench_function_pc && !bench_function_return) {
      bench_function_started = bench_tstates;
      bench_function_return = M_RDMEM_WORD(R.SP.D);
    }
    int bench_before = Z80_ICount;
"""
SETUP = """  extern unsigned bench_main_pc, bench_function_pc;
  bench_main_pc = (unsigned)strtoul(getenv("BENCH_MAIN"), NULL, 16);
  bench_function_pc = (unsigned)strtoul(getenv("BENCH_FUNCTION"), NULL, 16);
"""
REPORT = """  extern unsigned bench_min_sp;
  extern unsigned long long bench_main_tstates, bench_function_cycles;
  extern unsigned long long bench_function_calls;
  fprintf(stderr, "bench: main=%llu min_sp=%u calls=%llu cycles=%llu\\n",
          bench_main_tstates, bench_min_sp, bench_function_calls,
          bench_function_cycles);
  FILE *bench_file = fopen(getenv("BENCH_RAM"), "wb");
  if (!bench_file) return 1;
  size_t bench_written = fwrite(RAM + 0x2000, 1, 0x2000, bench_file);
  fclose(bench_file);
  if (bench_written != 0x2000) return 1;
"""


def instrument(repository: Path, temporary: Path) -> Path:
    """@brief Compile an instrumented copy without touching emulator sources.

    @param[in] repository Project root containing emulator and firmware sources.
    @param[in] temporary Workspace for the copied sources and build.
    @return Path to the instrumented emulator executable.
    """
    emulator = temporary / "emulator"
    shutil.copytree(
        repository / "emulator", emulator,
        ignore=shutil.ignore_patterns("build", "__pycache__"),
    )
    shutil.copytree(repository / "firmware/src", temporary / "firmware/src")
    cpu = emulator / "vendor/m2000/Z80.c"
    text = cpu.read_text().replace(
        "int Z80_Execute (void)", GLOBALS + "int Z80_Execute (void)"
    )
    text = text.replace("    ++R.R;", PROBE + "    ++R.R;")
    text = text.replace(
        "    (*(opcode_main[opcode]))();\n  } while (Z80_ICount>0);",
        "    (*(opcode_main[opcode]))();\n"
        "    bench_tstates += (unsigned)(bench_before - Z80_ICount);\n"
        "  } while (Z80_ICount>0);",
    )
    if "bench_tstates +=" not in text:
        raise RuntimeError("CPU instrumentation point changed")
    cpu.write_text(text)
    main = emulator / "src/main.c"
    text = main.read_text().replace(
        "  if (!InitP2000(m, c))", "  RAMSizeKb = 16;\n  if (!InitP2000(m, c))"
    )
    text = text.replace("  Z80_SetRegs(&regs);", "  Z80_SetRegs(&regs);\n" + SETUP)
    text = text.replace("  TrashP2000();", REPORT + "  TrashP2000();")
    if "RAMSizeKb = 16;" not in text or "bench_written" not in text:
        raise RuntimeError("Emulator instrumentation point changed")
    main.write_text(text)
    build = emulator / "build"
    subprocess.run(["cmake", "-S", str(emulator), "-B", str(build), "-G", "Ninja"], check=True)
    subprocess.run(["cmake", "--build", str(build)], check=True)
    return build / "p2000t-emulator"


def measure(repository: Path, emulator: Path, monitor: Path, build: Path,
            temporary: Path, baseline: bool) -> dict:
    """@brief Execute one fixture scenario and verify its RAM image.

    @param[in] repository Project root providing the fixture and font.
    @param[in] emulator Instrumented emulator binary.
    @param[in] monitor Synthetic monitor ROM used by the cartridge tests.
    @param[in] build Linked cartridge output directory.
    @param[in] temporary Workspace for screen and RAM dumps.
    @param[in] baseline Whether this is the production cartridge build.
    @return CPU timing, stack and image-size measurements.
    """
    mapfile = build / ("p2wp-cartridge.map" if baseline else "split.map")
    symbols = {
        name: int(value, 16)
        for name, value in re.findall(
            r"^(\w+)\s+= \$([0-9A-Fa-f]+)", mapfile.read_text(), re.M
        )
    }
    rom = repository / "build/p2wp-cartridge.bin" if baseline else build / "p2wp-cartridge.bin"
    if not rom.exists() and not baseline:
        rom = build / "split-runtime.bin"
    environment = dict(
        os.environ, BENCH_MAIN=hex(symbols["_main"]),
        BENCH_FUNCTION=hex(symbols["_clock_text"]),
        BENCH_RAM=str(temporary / "ram.bin"),
    )
    command = [
        str(emulator), "--monitor", str(monitor), "--cartridge", str(rom),
        "--fixture", str(repository / "emulator/tests/fixtures/nos-100.json"),
        "--font", str(repository / "emulator/assets/Default.fnt"),
        "--headless", "--auto", "--frames", "650",
        "--dump-screen", str(temporary / "screen.bin"),
    ]
    result = subprocess.run(command, env=environment, check=True, capture_output=True, text=True)
    (temporary / f"{build.parent.name}-benchmark.log").write_text(result.stderr)
    match = re.search(r"bench: main=(\d+) min_sp=(\d+) calls=(\d+) cycles=(\d+)", result.stderr)
    if not match or b"Meer bevoegdheden" not in (temporary / "screen.bin").read_bytes():
        raise RuntimeError("Missing measurements or fixture page failed to render")
    cycles, minimum_sp, calls, clock_cycles = map(int, match.groups())
    if not cycles or not calls or minimum_sp < 0x9BF0:
        raise RuntimeError("Missing execution or protected stack reserve exceeded")
    if baseline:
        report = json.loads((build / "measurements.json").read_text())
        report["variant"] = "production"
    else:
        report_file = build / "measurements.json"
        if not report_file.exists():
            report_file = build / "result.json"
        report = json.loads(report_file.read_text())
        if report.get("required_ram_kib", 16) != 16:
            raise RuntimeError("This benchmark is restricted to 16 KiB RAM layouts")
        # Normalize measurements from the first LZ4 exploration script.
        report.setdefault("codec", "lz4")
        for old, new in (
            ("free", "rom_free"), ("ram_code_bytes", "runtime_bytes"),
            ("packed_bytes", "compressed_bytes"),
        ):
            if old in report:
                report.setdefault(new, report.pop(old))
        report.setdefault("stack_reserve", 1024)
        report.setdefault("ram_growth_budget", report["stack_gap"] - 1024)
    runtime_file = "p2wp-cartridge_runtime_code.bin" if baseline else "split_runtime_code.bin"
    runtime = (build / runtime_file).read_bytes()
    if (temporary / "ram.bin").read_bytes()[:len(runtime)] != runtime:
        raise RuntimeError("Decoded runtime was corrupted during execution")
    report.update(
        startup_tstates=cycles, startup_ms=cycles / 2500,
        minimum_sp=hex(minimum_sp), clock_text_calls=calls,
        clock_text_total_tstates=clock_cycles,
        clock_text_average_tstates=clock_cycles / calls,
        experiment_directory=str(build.parent),
    )
    return report


def main() -> int:
    """@brief Benchmark production and one or more compiled experiment directories.

    @return Zero after writing all measurements to a new JSON file.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiments", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output must be a new file")
    repository = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="p2000-runtime-benchmark-") as directory:
        temporary = Path(directory)
        emulator = instrument(repository, temporary)
        monitor = temporary / "monitor.bin"
        subprocess.run(["python3", str(repository / "emulator/tests/make_monitor.py"), str(monitor)], check=True)
        rows = [measure(repository, emulator, monitor, repository / "build/cartridge", temporary, True)]
        for experiment in args.experiments:
            row = measure(repository, emulator, monitor, experiment.resolve() / "build", temporary, False)
            rows.append(row)
            print(json.dumps(row), flush=True)
    baseline_cycles = rows[0]["startup_tstates"]
    for row in rows:
        row["startup_difference_to_production_ms"] = (row["startup_tstates"] - baseline_cycles) / 2500
    args.output.write_text(json.dumps(rows, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
