#!/usr/bin/env python3
"""Ensure cartridge, firmware, and release metadata use one version."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
expected = tuple(int(part) for part in (ROOT / "VERSION").read_text().strip().split("."))
if len(expected) != 3:
    raise SystemExit("VERSION must contain MAJOR.MINOR.PATCH")

firmware = (ROOT / "firmware/src/version.h").read_text()
firmware_version = tuple(
    int(re.search(rf"#define P2WP_FIRMWARE_VERSION_{name} (\d+)u", firmware).group(1))
    for name in ("MAJOR", "MINOR", "PATCH")
)
cartridge = (ROOT / "src/p2wp-cartridge.asm").read_text()
cartridge_version = tuple(
    int(re.search(rf"CARTRIDGE_VERSION_{name}: equ (\d+)", cartridge).group(1))
    for name in ("MAJOR", "MINOR", "PATCH")
)
if firmware_version != expected or cartridge_version != expected:
    raise SystemExit(
        f"release mismatch: VERSION={expected}, firmware={firmware_version}, "
        f"cartridge={cartridge_version}"
    )
print(f"release versions agree: {'.'.join(map(str, expected))}")
