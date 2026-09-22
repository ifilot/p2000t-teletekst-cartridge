#!/usr/bin/env python3
"""Check readable Doxygen block layout in the cartridge C sources.

SPDX-License-Identifier: GPL-3.0-only
"""

from pathlib import Path
import sys


def check_file(path: Path) -> list[str]:
    """Return Doxygen layout errors found in one C source or header."""
    lines = path.read_text(encoding="utf-8").splitlines()
    errors: list[str] = []
    index = 0
    while index < len(lines):
        line = lines[index]
        if "/**" not in line or "/**<" in line:
            index += 1
            continue

        start = index
        block = [line]
        while "*/" not in block[-1] and index + 1 < len(lines):
            index += 1
            block.append(lines[index])

        if "@brief" not in "\n".join(block):
            index += 1
            continue
        if block[0].strip() != "/**":
            errors.append(f"{path}:{start + 1}: Doxygen opener must be on its own line")
        if block[-1].strip() != "*/":
            errors.append(f"{path}:{index + 1}: Doxygen closer must be on its own line")
        for offset, content in enumerate(block[1:-1], start + 2):
            stripped = content.strip()
            if not stripped.startswith("*"):
                errors.append(f"{path}:{offset}: Doxygen content must start with ' *'")
            if stripped.count("@") > 1:
                errors.append(
                    f"{path}:{offset}: put each Doxygen tag on a separate line"
                )
        index += 1
    return errors


def main(arguments: list[str]) -> int:
    """Check all paths supplied on the command line."""
    errors: list[str] = []
    for argument in arguments:
        errors.extend(check_file(Path(argument)))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
