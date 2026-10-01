#!/usr/bin/env python3
"""@file make_cartridge_rom.py
@brief Assemble linked code/data into a padded 16 KiB P2000T ROM image.

The Z88DK ROM memory model emits initialized RAM separately from code and
read-only data. This tool joins those outputs and validates the cartridge
marker before padding unused EPROM space with erased bytes.

SPDX-License-Identifier: GPL-3.0-only
"""

from __future__ import annotations

import argparse
from pathlib import Path

ROM_SIZE = 16 * 1024
ERASED_BYTE = 0xFF


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
    args = parser.parse_args()

    code = args.input.read_bytes()
    data = args.data.read_bytes() if args.data is not None else b""
    payload = code + data
    if len(payload) > ROM_SIZE:
        parser.error(
            f"linked image is {len(payload)} bytes; cartridge limit is {ROM_SIZE}"
        )
    if len(payload) < 16 or payload[0] != 0x5E:
        parser.error("linked image does not contain a P2000T cartridge header")

    args.output.write_bytes(payload + bytes([ERASED_BYTE]) * (ROM_SIZE - len(payload)))
    print(
        f"{args.output}: {len(code)} code/rodata bytes, {len(data)} initialized "
        f"data bytes, {ROM_SIZE - len(payload)} padding bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
