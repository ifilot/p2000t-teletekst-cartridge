#!/usr/bin/env python3
"""Add or verify the P2000T cartridge additive checksum."""

from __future__ import annotations

import argparse
import errno
import os
from pathlib import Path
import sys
import tempfile

HEADER_SIZE = 5
MARKER = 0x5E
ROM_SIZE = 16 * 1024


def payload_sum(image: bytes, length: int) -> int:
    """@brief Calculate the monitor's additive checksum over a payload.

    @param image Complete cartridge image including its five-byte header.
    @param length Number of payload bytes to include after the header.
    @return Unsigned 16-bit payload sum.
    """
    return sum(image[HEADER_SIZE:HEADER_SIZE + length]) & 0xFFFF


def sign(path: Path, length: int | None) -> tuple[int, int]:
    """@brief Atomically write a valid additive checksum header.

    @param path Cartridge image to validate and update.
    @param length Payload length override, or None for the complete image.
    @return Tuple containing the signed payload length and checksum value.
    @raise ValueError If the image structure or payload length is invalid.
    """
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

    write_atomically(path, bytes(output))
    return payload_length, value


def write_atomically(path: Path, data: bytes) -> None:
    """@brief Replace a file's contents through a temporary file and rename.

    @param path File to overwrite.
    @param data Complete new contents.

    WSL2 drvfs (9p) mounts occasionally refuse to rename over a file that was
    written moments ago and report EXDEV even though both paths share a
    filesystem. Atomicity is only a nicety here, so fall back to writing the
    file in place when that happens.
    """
    path = path.resolve()
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=f".{path.name}.", delete=False) as temporary:
        temporary.write(data)
        temporary.flush()
        os.fsync(temporary.fileno())
        temporary_path = Path(temporary.name)
    try:
        os.replace(temporary_path, path)
    except OSError as error:
        temporary_path.unlink(missing_ok=True)
        if error.errno != errno.EXDEV:
            raise
        path.write_bytes(data)
    except BaseException:
        temporary_path.unlink(missing_ok=True)
        raise


def verify(path: Path) -> tuple[int, int]:
    """@brief Verify the structure and additive checksum of an image.

    @param path Cartridge image to read.
    @return Tuple containing the stored payload length and checksum value.
    @raise ValueError If the image or checksum is invalid.
    """
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
    """@brief Run the cartridge signing or verification command.

    @return Process exit status: zero on success and one on validation/I/O error.
    """
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
