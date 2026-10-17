#!/usr/bin/env python3
"""Check numeric, displayed and tagged release versions before publishing."""

import argparse
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def check_versions(root: Path = ROOT, tag: str | None = None) -> str:
    """Validate all embedded versions and an optional release tag."""
    release = (root / "VERSION").read_text().strip()
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", release):
        raise ValueError("VERSION must contain MAJOR.MINOR.PATCH")
    expected = tuple(map(int, release.split(".")))
    if any(component > 255 for component in expected):
        raise ValueError("VERSION components must fit the uint8 protocol fields")
    for path, prefix, string_macro, display in (
        ("firmware/src/version.h", "P2WP_FIRMWARE_VERSION", "P2WP_FIRMWARE_VERSION_STRING", "v" + release),
        ("src/version.h", "P2WP_CARTRIDGE_VERSION", "P2WP_CARTRIDGE_VERSION", release),
    ):
        header = (root / path).read_text()
        for name, value in zip(("MAJOR", "MINOR", "PATCH"), expected):
            match = re.search(rf"^#define {prefix}_{name} ([0-9]+)u$", header, re.MULTILINE)
            if match is None or int(match.group(1)) != value:
                raise ValueError(f"{path}: {prefix}_{name} must equal {value}u")
        match = re.search(rf'^#define {string_macro} "([^"\n]*)"$', header, re.MULTILINE)
        if match is None or match.group(1) != display:
            raise ValueError(f"{path}: {string_macro} must equal {display!r}")
    if tag is not None and tag not in (release, "v" + release):
        raise ValueError(f"release tag {tag!r} does not match VERSION {release}")
    return release


def main() -> None:
    """Use the GitHub tag automatically in tagged CI builds."""
    ref = os.environ.get("GITHUB_REF", "")
    tag = os.environ.get("GITHUB_REF_NAME") if os.environ.get("GITHUB_REF_TYPE") == "tag" else None
    if tag is None and ref.startswith("refs/tags/"):
        tag = ref[len("refs/tags/"):]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default=tag, help="release tag to verify against VERSION")
    args = parser.parse_args()
    try:
        release = check_versions(tag=args.tag)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    print(f"release versions agree: {release}")


if __name__ == "__main__":
    main()
