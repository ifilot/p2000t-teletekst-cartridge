#!/usr/bin/env python3
"""@file make_cartridge_rom.py
@brief Validate and package ROM code/data plus the ZX0-compressed viewer core.

The Z88DK ROM memory model emits initialized RAM separately from code and
read-only data. RAM code is linked separately at 0x8000, then compressed and
appended after initialized DATA. Layout checks protect a 1 KiB stack reserve
within 16 KiB total RAM before padding unused EPROM space with erased bytes.

SPDX-License-Identifier: GPL-3.0-only
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

ROM_SIZE = 16 * 1024
ERASED_BYTE = 0xFF
RAM_START = 0x7000
RUNTIME_START = 0x8000
STACK_TOP = 0x9FF0
STACK_RESERVE = 1024


def read_symbols(path: Path) -> dict[str, int]:
    """@brief Read hexadecimal symbol addresses from a Z88DK link map.

    @param[in] path Map emitted by the final ROM/RAM link.
    @return Symbol names mapped to absolute addresses.
    """
    return {
        name: int(value, 16)
        for name, value in re.findall(
            r"^(\w+)\s+= \$([0-9A-Fa-f]+)", path.read_text(), re.M
        )
    }


def validate_runtime(code: bytes, data: bytes, runtime: bytes, packed: bytes,
                     decoded: bytes, symbols: dict[str, int],
                     functions: list[str]) -> dict:
    """@brief Check the final link, independent ZX0 round-trip and RAM budget.

    @param[in] code Resident ROM code and constants, including the header.
    @param[in] data Initialized DATA appended after the resident ROM bytes.
    @param[in] runtime Instructions linked at their final RAM addresses.
    @param[in] packed Compressed ZX0 block to store in ROM.
    @param[in] decoded Independently decompressed output from z88dk-dzx0.
    @param[in] symbols Final link-map symbols.
    @param[in] functions Manifest of functions selected for RAM execution.
    @return Measurements after all layout and image checks pass.
    """
    required = {
        "RAM_Start", "Runtime_Start", "Stack_Top", "__BSS_head", "__BSS_END_tail",
        "__DATA_head", "__DATA_END_tail", "__ROMABLE_END_tail",
        "__runtime_code_head", "__runtime_code_tail", "__runtime_code_size",
        "_main", "_lz4_decompress", "asm_dzx0_standard", "unpack_runtime",
        "_choose_source", "_p2wp_request",
    }
    missing = required - symbols.keys()
    if missing:
        raise ValueError(f"Missing layout symbols: {sorted(missing)}")
    if (symbols["RAM_Start"], symbols["Runtime_Start"], symbols["Stack_Top"]) != (
        RAM_START, RUNTIME_START, STACK_TOP
    ):
        raise ValueError("Memory map must fit the fixed 16 KiB RAM layout")
    bss_start, bss_end = symbols["__BSS_head"], symbols["__BSS_END_tail"]
    if bss_start != RAM_START or not bss_start <= bss_end <= RUNTIME_START:
        raise ValueError("Mutable RAM overlaps the viewer runtime")
    data_start, data_end = symbols["__DATA_head"], symbols["__DATA_END_tail"]
    if not bss_start <= data_start <= data_end == bss_end:
        raise ValueError("Initialized DATA is outside the mutable RAM region")
    if data_end - data_start != len(data):
        raise ValueError("Initialized DATA length disagrees with the link map")
    rom_end = symbols["__ROMABLE_END_tail"]
    if rom_end != 0x1000 + len(code):
        raise ValueError("Resident ROM length disagrees with the loader address")
    runtime_start = symbols["__runtime_code_head"]
    runtime_end = symbols["__runtime_code_tail"]
    if runtime_start != RUNTIME_START or runtime_end != RUNTIME_START + len(runtime):
        raise ValueError("RAM runtime length or origin disagrees with the link map")
    if symbols["__runtime_code_size"] != len(runtime) or not runtime or not packed:
        raise ValueError("Empty runtime or inconsistent RAM section size")
    if runtime_end > STACK_TOP - STACK_RESERVE:
        raise ValueError("RAM runtime consumes the protected 1 KiB stack reserve")
    if decoded != runtime:
        raise ValueError("ZX0 round-trip does not match the RAM-linked instructions")
    if not functions or not {"fetch_page", "viewer_loop", "teletekst_start"} <= set(functions):
        raise ValueError("Incomplete viewer-core function manifest")
    for name in functions:
        address = symbols.get("_" + name, -1)
        if not runtime_start <= address < runtime_end:
            raise ValueError(f"{name} was not linked into the RAM runtime")
    for name in ("_main", "_lz4_decompress", "asm_dzx0_standard", "unpack_runtime",
                 "_choose_source", "_p2wp_request"):
        if not 0x1000 <= symbols[name] < rom_end:
            raise ValueError(f"{name} must remain resident in ROM")
    used = len(code) + len(data) + len(packed)
    if used > ROM_SIZE:
        raise ValueError(f"Packed image is {used} bytes; cartridge limit is {ROM_SIZE}")
    return {
        "variant": "viewer-core", "codec": "zx0", "required_ram_kib": 16,
        "resident_bytes": len(code) + len(data), "runtime_bytes": len(runtime),
        "compressed_bytes": len(packed), "rom_used": used, "rom_free": ROM_SIZE - used,
        "mutable_ram_bytes": bss_end - bss_start,
        "runtime_start": hex(runtime_start), "runtime_end": hex(runtime_end),
        "stack_top": hex(STACK_TOP), "stack_reserve": STACK_RESERVE,
        "stack_gap": STACK_TOP - runtime_end,
        "ram_growth_budget": STACK_TOP - runtime_end - STACK_RESERVE,
    }


def main() -> int:
    """@brief Build one complete cartridge image from command-line inputs.

    @return Zero after writing a valid image; argparse exits on invalid input.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data",
        type=Path,
        help="initialized RAM image emitted by Z88DK's ROM memory model",
    )
    parser.add_argument("input", type=Path, help="linked Z88DK binary")
    parser.add_argument("output", type=Path, help="complete cartridge image")
    parser.add_argument("--runtime", type=Path, help="RAM-linked viewer binary")
    parser.add_argument("--compressed", type=Path, help="ZX0-packed viewer binary")
    parser.add_argument("--decoded", type=Path, help="independent ZX0 round-trip output")
    parser.add_argument("--map", type=Path, help="final ROM/RAM link map")
    parser.add_argument("--functions", type=Path, help="RAM-function manifest")
    parser.add_argument("--report", type=Path, help="write validated layout measurements")
    parser.add_argument("--verify", action="store_true", help="check an existing ROM")
    args = parser.parse_args()

    code = args.input.read_bytes()
    data = args.data.read_bytes() if args.data is not None else b""
    runtime_options = (args.runtime, args.compressed, args.decoded, args.map, args.functions)
    packed = b""
    report = None
    if any(runtime_options):
        if not all(runtime_options) or args.data is None:
            parser.error("RAM packaging requires data, runtime, compressed, decoded, map and functions")
        packed = args.compressed.read_bytes()
        try:
            report = validate_runtime(
                code, data, args.runtime.read_bytes(), packed, args.decoded.read_bytes(),
                read_symbols(args.map), json.loads(args.functions.read_text()),
            )
        except ValueError as error:
            parser.error(str(error))
    payload = code + data + packed
    if len(payload) > ROM_SIZE:
        parser.error(
            f"linked image is {len(payload)} bytes; cartridge limit is {ROM_SIZE}"
        )
    if len(payload) < 16 or payload[0] != 0x5E:
        parser.error("linked image does not contain a P2000T cartridge header")

    image = payload + bytes([ERASED_BYTE]) * (ROM_SIZE - len(payload))
    if args.verify:
        existing = args.output.read_bytes()
        # Signing fills bytes 1..4; sign_cartridge.py verifies those separately.
        if len(existing) != ROM_SIZE or existing[:1] != image[:1] or existing[5:] != image[5:]:
            parser.error("Existing cartridge differs from the validated linked payload")
    else:
        args.output.write_bytes(image)
    if args.report is not None and report is not None:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(
        f"{args.output}: {len(code)} code/rodata bytes, {len(data)} initialized "
        f"data bytes, {len(packed)} packed runtime bytes, {ROM_SIZE - len(payload)} padding bytes"
    )
    if report is not None:
        print(
            f"RAM: {report['mutable_ram_bytes']} mutable bytes, {report['runtime_bytes']} "
            f"runtime bytes, {STACK_RESERVE} stack reserve bytes, "
            f"{report['ram_growth_budget']} growth bytes"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
