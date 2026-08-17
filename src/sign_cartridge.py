#!/usr/bin/env python3
"""Add or verify the P2000T cartridge additive checksum."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys
import tempfile

HEADER_SIZE = 5
MARKER = 0x5E
ROM_SIZE = 16 * 1024


def payload_sum(image: bytes, length: int) -> int:
    return sum(image[HEADER_SIZE:HEADER_SIZE + length]) & 0xFFFF


def sign(path: Path, length: int | None) -> tuple[int, int]:
    image = path.read_bytes()
    if len(image) != ROM_SIZE:
        raise ValueError(f"expected a {ROM_SIZE}-byte cartridge, got {len(image)} bytes")
    if image[0] != MARKER:
        raise ValueError(f"not a P2000T cartridge (expected marker 0x{MARKER:02x})")
    payload_length = len(image) - HEADER_SIZE if length is None else length
    if not 0 < payload_length <= len(image) - HEADER_SIZE:
        raise ValueError("checksum length must be between 1 and 16379 bytes")

    # The monitor starts its additive checksum with the value in bytes 3-4
    # and accepts the image when the final 16-bit sum wraps to zero.
    value = (-payload_sum(image, payload_length)) & 0xFFFF
    output = bytearray(image)
    output[1:3] = payload_length.to_bytes(2, "little")
    output[3:5] = value.to_bytes(2, "little")

    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=f".{path.name}.", delete=False) as temporary:
        temporary.write(output)
        temporary.flush()
        os.fsync(temporary.fileno())
        temporary_path = Path(temporary.name)
    try:
        os.replace(temporary_path, path)
    except BaseException:
        temporary_path.unlink(missing_ok=True)
        raise
    return payload_length, value


def verify(path: Path) -> tuple[int, int]:
    image = path.read_bytes()
    if len(image) != ROM_SIZE or image[0] != MARKER:
        raise ValueError("not a valid 16 KiB P2000T cartridge")
    length = int.from_bytes(image[1:3], "little")
    stored = int.from_bytes(image[3:5], "little")
    if length == 0 or length > len(image) - HEADER_SIZE:
        raise ValueError(f"invalid checksum length: {length}")
    calculated = (stored + payload_sum(image, length)) & 0xFFFF
    if calculated != 0:
        raise ValueError(f"checksum mismatch: stored 0x{stored:04x}, residual 0x{calculated:04x}")
    return length, stored


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="16 KiB cartridge image")
    parser.add_argument("--verify", action="store_true", help="verify instead of signing")
    parser.add_argument("--length", type=int, help="payload bytes to checksum (default: complete image)")
    args = parser.parse_args()
    try:
        if args.verify:
            length, value = verify(args.image)
            print(f"{args.image}: checksum OK (length {length}, value 0x{value:04x})")
        else:
            length, value = sign(args.image, args.length)
            print(f"{args.image}: signed (length {length}, value 0x{value:04x})")
    except (OSError, ValueError) as error:
        print(f"sign_cartridge.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
