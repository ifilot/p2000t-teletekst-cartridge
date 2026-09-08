#!/usr/bin/env python3
"""Tests for the lightweight custom Teletekst server."""

import base64
import json
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

import server as teletekst_server


class ServerTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.pages = Path(self.temporary_directory.name)
        (self.pages / "100.txt").write_text("PAGE 100\n", encoding="utf-8")
        (self.pages / "100-2.txt").write_text("SUBPAGE 2\n", encoding="utf-8")
        (self.pages / "101.bin").write_bytes(b"X" * 960)
        self.httpd = teletekst_server.ThreadingHTTPServer(
            ("127.0.0.1", 0), teletekst_server.TeletekstHandler
        )
        self.httpd.pages_directory = self.pages
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()
        self.base_url = f"http://127.0.0.1:{self.httpd.server_port}"

    def tearDown(self):
        self.httpd.shutdown()
        self.httpd.server_close()
        self.thread.join()
        self.temporary_directory.cleanup()

    def fetch(self, path):
        with urllib.request.urlopen(self.base_url + path) as response:
            return response.status, json.load(response)

    def test_text_page_and_navigation(self):
        status, page = self.fetch("/json/100")
        self.assertEqual(status, 200)
        self.assertEqual(page["prevPage"], "")
        self.assertEqual(page["nextPage"], "101")
        self.assertEqual(page["nextSubPage"], "100-2")
        screen = base64.b64decode(page["binaryDisplay"])
        self.assertEqual(len(screen), 960)
        self.assertEqual(screen[:8], b"PAGE 100")

    def test_binary_page(self):
        _, page = self.fetch("/json/101")
        self.assertEqual(base64.b64decode(page["binaryDisplay"]), b"X" * 960)
        self.assertEqual(page["prevPage"], "100")
        self.assertEqual(page["nextPage"], "")

    def test_text_page_byte_escapes(self):
        path = self.pages / "102.txt"
        path.write_text(r"\x07PUBLIC\x18SECRET\x07" + "\n", encoding="utf-8")
        screen = teletekst_server.load_screen(path)
        expected = b"\x07PUBLIC\x18SECRET\x07"
        self.assertEqual(screen[: len(expected)], expected)
        self.assertEqual(len(screen), 960)

    def test_included_conceal_example(self):
        path = Path(__file__).with_name("pages") / "101.txt"
        screen = teletekst_server.load_screen(path)
        conceal = screen.index(b"\x18")
        self.assertEqual(screen[:37], b" " * 37)
        self.assertEqual(screen[37:40], b"101")
        self.assertEqual(screen.count(b"\x18"), 1)
        self.assertEqual(screen[conceal + 1 : conceal + 9], b"A piano!")

    def test_missing_page(self):
        with self.assertRaises(urllib.error.HTTPError) as context:
            self.fetch("/json/404")
        self.assertEqual(context.exception.code, 404)


if __name__ == "__main__":
    unittest.main()
