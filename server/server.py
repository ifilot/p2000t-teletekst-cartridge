#!/usr/bin/env python3
"""Tiny, dependency-free custom Teletekst server for development."""

import argparse
import base64
import json
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


PAGE_NAME = re.compile(r"^([1-8][0-9]{2})(?:-([1-9][0-9]?))?$")
REQUEST_PATH = re.compile(r"^/json/([1-8][0-9]{2})(?:-([1-9][0-9]?))?/?$")
SCREEN_SIZE = 24 * 40
BYTE_ESCAPE = re.compile(rb"\\x([0-9a-fA-F]{2})")


def page_catalog(pages_directory):
    """Return {(page, subpage): path} for the editable page directory."""
    pages = {}
    for path in Path(pages_directory).iterdir():
        match = PAGE_NAME.fullmatch(path.stem)
        if match and path.suffix.lower() in (".txt", ".bin"):
            key = (int(match.group(1)), int(match.group(2) or 0))
            if key not in pages or path.suffix.lower() == ".bin":
                pages[key] = path
    return pages


def load_screen(path):
    """Load an exact .bin screen or pad an easy-to-edit .txt page."""
    if path.suffix.lower() == ".bin":
        screen = path.read_bytes()
        if len(screen) != SCREEN_SIZE:
            raise ValueError(f"{path} must contain exactly {SCREEN_SIZE} bytes")
        return screen

    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) > 24:
        raise ValueError(f"{path} has more than 24 lines")
    screen = bytearray()
    for line in lines:
        row = line.encode("ascii", "replace").replace(b"#", b"_")
        row = BYTE_ESCAPE.sub(
            lambda match: bytes((int(match.group(1), 16),)),
            row,
        )
        screen.extend(row[:40].ljust(40, b" "))
    return bytes(screen).ljust(SCREEN_SIZE, b" ")


def page_response(pages_directory, page, subpage=0):
    """Build the NOS-compatible JSON object consumed by the Pico firmware."""
    pages = page_catalog(pages_directory)
    path = pages.get((page, subpage))
    if path is None:
        return None

    page_numbers = sorted({number for number, _ in pages})
    position = page_numbers.index(page)
    previous_page = str(page_numbers[position - 1]) if position else ""
    next_page = str(page_numbers[position + 1]) if position + 1 < len(page_numbers) else ""

    subpages = sorted(part for number, part in pages if number == page and part)
    following_subpage = next((part for part in subpages if part > subpage), None)
    next_subpage = f"{page}-{following_subpage}" if following_subpage else ""

    return {
        "prevPage": previous_page,
        "nextPage": next_page,
        "nextSubPage": next_subpage,
        "binaryDisplay": base64.b64encode(load_screen(path)).decode("ascii"),
    }


class TeletekstHandler(BaseHTTPRequestHandler):
    """Serve custom Teletekst pages and a small discovery response."""

    def do_GET(self):
        if self.path == "/":
            self.send_body(200, b"P2000T test server: use /json/100\n", "text/plain")
            return

        match = REQUEST_PATH.fullmatch(self.path)
        if match is None:
            self.send_error(404)
            return
        try:
            response = page_response(
                self.server.pages_directory,
                int(match.group(1)),
                int(match.group(2) or 0),
            )
        except (OSError, UnicodeError, ValueError) as error:
            self.send_error(500, str(error))
            return
        if response is None:
            self.send_error(404, "Teletekst page not found")
            return
        body = json.dumps(response, separators=(",", ":")).encode("ascii")
        self.send_body(200, body, "application/json")

    def send_body(self, status, body, content_type):
        self.send_response(status)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument(
        "--pages",
        type=Path,
        default=Path(__file__).with_name("pages"),
        help="directory containing PAGE.txt or PAGE.bin files",
    )
    arguments = parser.parse_args()
    if not arguments.pages.is_dir():
        parser.error(f"page directory does not exist: {arguments.pages}")

    server = ThreadingHTTPServer((arguments.host, arguments.port), TeletekstHandler)
    server.pages_directory = arguments.pages
    print(
        f"Serving {arguments.pages} at "
        f"http://{arguments.host}:{server.server_port}",
        flush=True,
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
