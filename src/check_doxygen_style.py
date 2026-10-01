#!/usr/bin/env python3
"""@file check_doxygen_style.py
@brief Enforce source and function documentation conventions for the ROM.

The checker complements Doxygen itself: Doxygen validates C declarations and
parameter coverage, while this script checks readable block layout, Python
docstrings, and the assembly entry points that Doxygen does not parse.

SPDX-License-Identifier: GPL-3.0-only
"""

from __future__ import annotations

import ast
from pathlib import Path
import re
import sys


def check_doxygen_blocks(path: Path) -> list[str]:
    """@brief Check the readable layout of Doxygen blocks in one C file.

    @param[in] path C source or header to inspect.
    @return Human-readable errors, or an empty list for a valid file.
    """
    lines = path.read_text(encoding="utf-8").splitlines()
    errors: list[str] = []
    if not any("@file" in line for line in lines[:15]):
        errors.append(f"{path}:1: file header must contain @file")
    if not any("@brief" in line for line in lines[:15]):
        errors.append(f"{path}:1: file header must contain @brief")

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
            tags = re.findall(r"(?<!\w)@(?!p\b)\w+", stripped)
            if len(tags) > 1:
                errors.append(
                    f"{path}:{offset}: put each Doxygen tag on a separate line"
                )
        index += 1
    return errors


def assembly_contract(lines: list[str], label_index: int) -> str:
    """@brief Collect the comment contract immediately preceding a label.

    @param[in] lines Complete assembly source split into lines.
    @param[in] label_index Index of the label whose contract is requested.
    @return Adjacent semicolon comment text, including consecutive label aliases.
    """
    index = label_index - 1
    while index >= 0 and re.match(r"^[A-Za-z_.$][\w.$]*:\s*$", lines[index]):
        index -= 1
    comments: list[str] = []
    while index >= 0 and (
        lines[index].lstrip().startswith(";") or not lines[index].strip()
    ):
        if lines[index].lstrip().startswith(";"):
            comments.append(lines[index])
        index -= 1
    return "\n".join(reversed(comments))


def check_assembly(path: Path) -> list[str]:
    """@brief Check file and callable-entry contracts in one assembly module.

    @param[in] path Assembly source to inspect.
    @return Human-readable errors, or an empty list for a valid file.
    """
    lines = path.read_text(encoding="utf-8").splitlines()
    errors: list[str] = []
    if not any("@file" in line for line in lines[:15]):
        errors.append(f"{path}:1: file header must contain @file")
    if not any("@brief" in line for line in lines[:15]):
        errors.append(f"{path}:1: file header must contain @brief")

    public = {
        match.group(1)
        for line in lines
        if (match := re.match(r"\s*PUBLIC\s+([A-Za-z_.$][\w.$]*)", line, re.I))
    }
    local_labels = {
        match.group(1): index
        for index, line in enumerate(lines)
        if (match := re.match(r"^([A-Za-z_.$][\w.$]*):\s*$", line))
    }
    called = {
        match.group(1)
        for line in lines
        if (match := re.search(r"\bcall\s+([A-Za-z_.$][\w.$]*)", line, re.I))
    }
    for label in sorted(public | (called & local_labels.keys())):
        if label not in local_labels:
            continue
        contract = assembly_contract(lines, local_labels[label])
        if "@brief" not in contract:
            errors.append(f"{path}:{local_labels[label] + 1}: {label} needs @brief")
        if "@return" not in contract:
            errors.append(f"{path}:{local_labels[label] + 1}: {label} needs @return")
        for line in contract.splitlines():
            if "@param" in line and not re.search(
                r"@param\[(?:in|out|in,out)\]", line
            ):
                errors.append(
                    f"{path}:{local_labels[label] + 1}: {label} parameter needs a direction"
                )
    return errors


def check_python(path: Path) -> list[str]:
    """@brief Check module and function docstrings in one Python source.

    @param[in] path Python source to inspect.
    @return Human-readable errors, or an empty list for a valid file.
    """
    source = path.read_text(encoding="utf-8")
    tree = ast.parse(source, filename=str(path))
    errors: list[str] = []
    module_doc = ast.get_docstring(tree, clean=False) or ""
    if "@file" not in module_doc or "@brief" not in module_doc:
        errors.append(f"{path}:1: module docstring needs @file and @brief")

    for node in ast.walk(tree):
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        doc = ast.get_docstring(node, clean=False) or ""
        if "@brief" not in doc:
            errors.append(f"{path}:{node.lineno}: {node.name} needs @brief")
        if "@return" not in doc:
            errors.append(f"{path}:{node.lineno}: {node.name} needs @return")
        arguments = [*node.args.posonlyargs, *node.args.args, *node.args.kwonlyargs]
        for argument in arguments:
            if argument.arg in {"self", "cls"}:
                continue
            pattern = (
                rf"@param\[(?:in|out|in,out)\]\s+{re.escape(argument.arg)}\b"
            )
            if not re.search(pattern, doc):
                errors.append(
                    f"{path}:{node.lineno}: {node.name} needs a directed "
                    f"@param for {argument.arg}"
                )
    return errors


def check_file(path: Path) -> list[str]:
    """@brief Dispatch one source file to its language-specific checker.

    @param[in] path Source file to inspect.
    @return Human-readable errors, or an empty list for a valid file.
    """
    if path.suffix in {".c", ".h"}:
        return check_doxygen_blocks(path)
    if path.suffix == ".asm":
        return check_assembly(path)
    if path.suffix == ".py":
        return check_python(path)
    return [f"{path}: unsupported source type"]


def main(arguments: list[str]) -> int:
    """@brief Check all source paths supplied on the command line.

    @param[in] arguments Source paths to check.
    @return Zero when all contracts are valid, otherwise one.
    """
    errors: list[str] = []
    for argument in arguments:
        errors.extend(check_file(Path(argument)))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
