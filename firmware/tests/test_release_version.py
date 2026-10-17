"""Regression checks for stale embedded versions and mislabelled releases."""

from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

from check_release_version import ROOT, check_versions


class ReleaseVersionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in ("VERSION", "src/version.h", "firmware/src/version.h"):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        self.version = (self.root / "VERSION").read_text().strip()

    def test_matching_versions_and_tags(self):
        for tag in (None, self.version, "v" + self.version):
            self.assertEqual(check_versions(self.root, tag), self.version)

    def test_stale_display_strings(self):
        for name in ("src/version.h", "firmware/src/version.h"):
            with self.subTest(header=name):
                path = self.root / name
                original = path.read_text()
                path.write_text(original.replace(self.version, "99.99.99"))
                with self.assertRaises(ValueError):
                    check_versions(self.root)
                path.write_text(original)

    def test_numeric_mismatch_and_missing_macro(self):
        path = self.root / "src/version.h"
        original = path.read_text()
        wrong_major = (int(self.version.split(".")[0]) + 1) % 256
        for replacement in (f"#define P2WP_CARTRIDGE_VERSION_MAJOR {wrong_major}u", ""):
            path.write_text(re.sub(r"^#define P2WP_CARTRIDGE_VERSION_MAJOR [0-9]+u$",
                                   replacement, original, flags=re.MULTILINE))
            with self.assertRaises(ValueError):
                check_versions(self.root)

    def test_mismatched_tag(self):
        with self.assertRaisesRegex(ValueError, "release tag"):
            check_versions(self.root, "v99.99.99")

    def test_invalid_or_unrepresentable_version(self):
        for value in ("0.6", "01.6.0", "-1.6.0", "0.256.0"):
            (self.root / "VERSION").write_text(value)
            with self.assertRaises(ValueError):
                check_versions(self.root)

    def test_ci_tag_is_checked_automatically(self):
        for variables in (
            {"GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": "v99.99.99"},
            {"GITHUB_REF": "refs/tags/v99.99.99"},
        ):
            environment = {k: v for k, v in os.environ.items()
                           if k not in ("GITHUB_REF", "GITHUB_REF_TYPE", "GITHUB_REF_NAME")}
            result = subprocess.run(
                [sys.executable, str(ROOT / "firmware/tests/check_release_version.py")],
                env={**environment, **variables}, capture_output=True, text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("release tag", result.stderr)
