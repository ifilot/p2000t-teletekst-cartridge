#!/usr/bin/env python3
"""@file prepare_runtime.py
@brief Place the optimized viewer core in its dedicated RAM linker section.

Uses SDCC's generated function headers, so forward declarations and changes in
C return types cannot silently change which viewer routines execute from RAM.
Constants remain in ROM. Instructions and calling conventions stay unchanged.

SPDX-License-Identifier: GPL-3.0-only
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


def select_functions(assembly: Path, variant: str = "viewer-core") -> set[str]:
    """@brief Resection optimized assembly in place and validate its function labels.

    @param[in,out] assembly Optimized SDCC assembly to modify.
    @param[in] variant Production viewer-core or an experimental selection.
    @return All function names that must subsequently be linked into RAM.
    """
    text = assembly.read_text()
    functions = re.findall(r"^\s*; Function (\w+)\s*$", text, re.M)
    if len(functions) != len(set(functions)):
        raise ValueError("Duplicate generated function headers")
    if "expected_status_length" not in functions or functions[-1] != "teletekst_start":
        raise ValueError("Viewer-core boundaries changed; review runtime placement")
    selected = (
        {"fetch_page", "viewer_loop"}
        if variant == "fetch-viewer"
        else set(functions[functions.index("expected_status_length") :])
    )
    active = False
    output = []
    found = set()
    lines = text.splitlines()
    for index, line in enumerate(lines):
        match = re.fullmatch(r"_(\w+):", line)
        if match and match[1] in functions:
            active = match[1] in selected
            if active:
                found.add(match[1])
            output.append("SECTION runtime_code" if active else "SECTION code_compiler")
        if active and line.strip() == "SECTION code_compiler":
            line = "SECTION runtime_code"
        if (
            variant == "core-strings"
            and line.strip() == "SECTION rodata_compiler"
            and index + 1 < len(lines)
            and lines[index + 1].startswith("___str_")
        ):
            line = "SECTION runtime_const"
        output.append(line)
    if found != selected:
        raise ValueError(f"Missing generated function labels: {selected - found}")
    assembly.write_text("\n".join(output) + "\n")
    return selected


def main() -> int:
    """@brief Prepare the production assembly and its link-validation manifest.

    @return Zero after writing assembly and the selected-function manifest.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_bytes(args.input.read_bytes())
    functions = select_functions(args.output)
    args.manifest.write_text(json.dumps(sorted(functions), indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
