#!/usr/bin/env python3
"""@file experiment_split_runtime.py
@brief Build experimental ROM/RAM splits without changing production sources.

Requires Docker and, for LZ4, the host liblz4 shared library. Outputs are confined
to a new experiment directory, separate from the release build.

SPDX-License-Identifier: GPL-3.0-only
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from prepare_runtime import select_functions

IMAGE = "z88dk/z88dk:2.4@sha256:7da21d721a76bf25db518f2415b859b2dfe215ccd059f117cb6ec052251ac2ae"
RAM_ORIGIN = 0x8000
STACK_RESERVE = 1024
LOADER = """; Experimental loader; the packer appends a size word and raw LZ4 block.
EXTERN _lz4_decompress
EXTERN __ROMABLE_END_tail, __DATA_head, __DATA_END_tail
unpack_runtime:
    ld hl,__ROMABLE_END_tail + (__DATA_END_tail - __DATA_head)
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    push bc
    ld de,$8000
    push de
    push hl
    call _lz4_decompress
    pop hl
    pop hl
    pop hl
    ret

"""
ZX0_LOADER = """; Experimental loader; ZX0 carries its own end marker.
EXTERN asm_dzx0_standard
EXTERN __ROMABLE_END_tail, __DATA_head, __DATA_END_tail
unpack_runtime:
    ld hl,__ROMABLE_END_tail + (__DATA_END_tail - __DATA_head)
    ld de,$8000
    call asm_dzx0_standard
    ret

"""


def compress(raw: bytes) -> bytes:
    """@brief Compress and independently round-trip a raw LZ4 block.

    @param[in] raw Linked bytes at their final RAM addresses.
    @return Compressed bytes without an embedded decoded-size prefix.
    """
    library = ctypes.util.find_library("lz4")
    if library is None:
        raise RuntimeError("The experiment requires the host liblz4 library")
    lib = ctypes.CDLL(library)
    lib.LZ4_compressBound.argtypes = [ctypes.c_int]
    lib.LZ4_compressBound.restype = ctypes.c_int
    signature = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
    lib.LZ4_compress_HC.argtypes = signature + [ctypes.c_int]
    lib.LZ4_compress_HC.restype = ctypes.c_int
    lib.LZ4_decompress_safe.argtypes = signature
    lib.LZ4_decompress_safe.restype = ctypes.c_int
    output = ctypes.create_string_buffer(lib.LZ4_compressBound(len(raw)))
    size = lib.LZ4_compress_HC(raw, output, len(raw), len(output), 12)
    if size <= 0:
        raise RuntimeError("LZ4 compression failed")
    packed = output.raw[:size]
    decoded = ctypes.create_string_buffer(len(raw))
    result = lib.LZ4_decompress_safe(packed, decoded, size, len(raw))
    if result != len(raw) or decoded.raw != raw:
        raise RuntimeError("LZ4 round-trip verification failed")
    return packed


def main() -> int:
    """@brief Compile, validate, compress, package and sign one experimental ROM.

    @return Zero after writing the image, link map and measurement report.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, required=True, help="new experiment directory"
    )
    parser.add_argument(
        "--variant",
        choices=("whole-viewer", "viewer-core", "fetch-viewer", "core-protocol", "core-strings"),
        default="whole-viewer",
    )
    parser.add_argument(
        "--codec", choices=("lz4", "zx0", "zx7", "zx7-turbo"), default="lz4"
    )
    args = parser.parse_args()
    root = args.output.resolve()
    if root.exists():
        parser.error("output must be a new directory to preserve existing experiments")
    repository = Path(__file__).resolve().parents[1]
    source = repository / "src"
    root.mkdir(parents=True)
    shutil.copytree(source, root / "src", ignore=shutil.ignore_patterns("__pycache__"))
    build = root / "build"
    build.mkdir()
    header = repository / "build/cartridge/build_info.h"
    if header.exists():
        shutil.copy(header, build / "build_info.h")
    else:
        result = subprocess.run(
            ["python3", str(source / "build_info.py")],
            check=True,
            capture_output=True,
        )
        (build / "build_info.h").write_bytes(result.stdout)
    stack_top = 0x9FF0
    crt = root / "src/crt0.asm"
    text = crt.read_text()
    loader = LOADER if args.codec == "lz4" else ZX0_LOADER
    if args.codec.startswith("zx7"):
        decoder = "asm_dzx7_turbo" if args.codec == "zx7-turbo" else "asm_dzx7_standard"
        loader = loader.replace("asm_dzx0_standard", decoder)
    (root / "src/runtime_loader.asm").write_text(loader)
    if args.variant == "core-strings":
        text += "SECTION runtime_const\n"
    crt.write_text(text)
    sources = [
        "main.c", "p2wp.c", "wifi.c", "ui.c", "teletekst.c", "platform.c",
        "platform.asm", "lz4_z80.asm",
    ]
    container = [
        "docker", "run", "--rm", "--user", f"{os.getuid()}:{os.getgid()}",
        "-v", f"{root}:/src", "-w", "/src/src", IMAGE,
    ]
    docker = container + ["/opt/z88dk/bin/zcc", "+embedded"]
    options = [
        "-compiler=sdcc", "-clib=default", "--opt-code-size", "-I../build",
        "--max-allocs-per-node10000", "-SO3",
    ]
    selected = {"teletekst_start"}
    if args.variant != "whole-viewer":
        subprocess.run(
            docker + options + ["-a", "-o", "../build/teletekst.asm", "teletekst.c"],
            check=True,
        )
        selected = select_functions(
            build / "teletekst.asm", args.variant
        )
        sources[sources.index("teletekst.c")] = "../build/teletekst.asm"
        if args.variant == "core-protocol":
            path = root / "src/p2wp.c"
            path.write_text("#pragma codeseg runtime_code\n" + path.read_text())
            selected.add("p2wp_request")
    else:
        path = root / "src/teletekst.c"
        path.write_text("#pragma codeseg runtime_code\n" + path.read_text())
    subprocess.run(
        docker + options + ["-crt0=crt0.asm", "-m", "-o", "../build/split"] + sources,
        check=True,
    )
    symbols = {
        name: int(value, 16)
        for name, value in re.findall(
            r"^(\w+)\s+= \$([0-9A-Fa-f]+)", (build / "split.map").read_text(), re.M
        )
    }
    raw = (build / "split_runtime_code.bin").read_bytes()
    runtime_end = symbols[
        "__runtime_const_tail" if args.variant == "core-strings" else "__runtime_code_tail"
    ]
    if symbols["__BSS_END_tail"] > RAM_ORIGIN or runtime_end != RAM_ORIGIN + len(raw):
        raise RuntimeError("RAM code overlaps mutable data or binary layout disagrees with map")
    if runtime_end > stack_top - STACK_RESERVE:
        raise RuntimeError("Less than 1 KiB remains for the runtime stack")
    for name in selected:
        if not RAM_ORIGIN <= symbols["_" + name] < runtime_end:
            raise RuntimeError(f"{name} was not linked into RAM")
    for name in ("_main", "_lz4_decompress", "unpack_runtime"):
        if not 0x1000 <= symbols[name] < 0x5000:
            raise RuntimeError(f"{name} must remain in ROM")
    if args.codec != "lz4":
        tool = "zx7" if args.codec.startswith("zx7") else "zx0"
        subprocess.run(
            container + [f"/opt/z88dk/bin/z88dk-{tool}", "-f",
                         "../build/split_runtime_code.bin", f"../build/runtime.{tool}"],
            check=True,
        )
        subprocess.run(
            container + [f"/opt/z88dk/bin/z88dk-d{tool}", "-f",
                         f"../build/runtime.{tool}", "../build/runtime.roundtrip.bin"],
            check=True,
        )
        if (build / "runtime.roundtrip.bin").read_bytes() != raw:
            raise RuntimeError(f"{tool.upper()} round-trip verification failed")
        packed = (build / f"runtime.{tool}").read_bytes()
        prefix = b""
    else:
        packed = compress(raw)
        prefix = len(packed).to_bytes(2, "little")
    resident = (build / "split").read_bytes() + (build / "split_DATA.bin").read_bytes()
    if symbols["__ROMABLE_END_tail"] != 0x1000 + (build / "split").stat().st_size:
        raise RuntimeError("ROM end does not match the appended payload location")
    data_size = (build / "split_DATA.bin").stat().st_size
    if symbols["__DATA_END_tail"] - symbols["__DATA_head"] != data_size:
        raise RuntimeError("Initialized DATA layout does not match the loader")
    payload = resident + prefix + packed
    if len(payload) > 16384 or payload[0] != 0x5E:
        raise RuntimeError("Invalid cartridge header or ROM payload exceeds 16 KiB")
    rom = build / "p2wp-cartridge.bin"
    rom.write_bytes(payload + b"\xff" * (16384 - len(payload)))
    for flags in ([], ["--verify"]):
        subprocess.run(
            ["python3", str(source / "sign_cartridge.py"), *flags, str(rom)],
            check=True,
        )
    report = {
        "variant": args.variant,
        "codec": args.codec,
        "required_ram_kib": 16,
        "resident_bytes": len(resident),
        "runtime_bytes": len(raw),
        "compressed_bytes": len(packed),
        "rom_used": len(payload),
        "rom_free": 16384 - len(payload),
        "runtime_start": hex(RAM_ORIGIN),
        "runtime_end": hex(runtime_end),
        "stack_top": hex(stack_top),
        "stack_gap": stack_top - runtime_end,
        "stack_reserve": STACK_RESERVE,
        "ram_growth_budget": stack_top - runtime_end - STACK_RESERVE,
    }
    (build / "measurements.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
