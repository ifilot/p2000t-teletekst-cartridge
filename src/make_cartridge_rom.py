#!/usr/bin/env python3
"""Pad a linked cartridge payload to the P2000T's 16 KiB ROM size."""

from __future__ import annotations

import argparse
from pathlib import Path

ROM_SIZE = 16 * 1024
ERASED_BYTE = 0xFF


def main() -> int:
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
