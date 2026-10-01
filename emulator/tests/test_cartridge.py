#!/usr/bin/env python3
"""Boot the Z88DK cartridge and verify C and monitor-call execution."""

from pathlib import Path
import base64
import json
import subprocess
import tempfile
import runpy
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
EMU = ROOT / "emulator"
CARTRIDGE = ROOT / "build" / "p2wp-cartridge.bin"


def run_emulator(
    binary: Path,
    monitor: Path,
    screen: Path,
    frames: int,
    inject_key: bool,
    protocol_version: int = 0,
    saved_profile: bool = False,
    wifi_security: int = 0,
    sources: Path | None = None,
    pages: Path | None = None,
    fetches: Path | None = None,
    auto_keys: str | None = None,
    password_visible: bool = False,
    pause_frame: int | None = None,
    resume_frame: int | None = None,
    fixture: Path | None = None,
    auto_source: int | None = None,
    custom_server: str | None = None,
    flash: Path | None = None,
    auto_source_cycles: int = 0,
    wait_opening: bool = False,
    fail_page: int = 0,
    fail_error: int = 0,
    repeat_fixture_subpage: bool = False,
    stall_fetch: bool = False,
    stall_fetch_after: int | None = None,
    firmware_fetch_timeout: bool = True,
    clock_valid: bool = True,
) -> bytes:
    command = [
        str(binary),
        "--monitor",
        str(monitor),
        "--cartridge",
        str(CARTRIDGE),
        "--fixture",
        str(fixture or EMU / "tests" / "fixtures" / "nos-100.json"),
        "--font",
        str(EMU / "assets" / "Default.fnt"),
        "--headless",
        "--frames",
        str(frames),
        "--dump-screen",
        str(screen),
    ]
    if inject_key:
        command.append("--auto")
    if repeat_fixture_subpage:
        command.append("--fixture-repeat-subpage")
    if stall_fetch:
        command.append("--stall-fetch")
    if stall_fetch_after is not None:
        command.extend(("--stall-fetch-after", str(stall_fetch_after)))
    if not firmware_fetch_timeout:
        command.append("--no-firmware-fetch-timeout")
    if not clock_valid:
        command.append("--clock-invalid")
    if auto_source is not None:
        command.extend(("--auto-source", str(auto_source)))
    if custom_server is not None:
        command.extend(("--custom-server", custom_server))
    if flash is not None:
        command.extend(("--flash", str(flash)))
    if auto_source_cycles:
        command.extend(("--auto-source-cycles", str(auto_source_cycles)))
    if wait_opening:
        command.append("--auto-wait-opening")
    if fail_page:
        command.extend(("--fail-page", str(fail_page),
                        "--fail-error", str(fail_error)))
    if protocol_version != 0:
        command.extend(("--p2wp-version", str(protocol_version)))
    if saved_profile:
        command.append("--wifi-profile")
    if wifi_security:
        command.extend(("--wifi-security", str(wifi_security)))
    if password_visible:
        command.append("--wifi-password-visible")
    if sources is not None:
        command.extend(("--dump-sources", str(sources)))
    if pages is not None:
        command.extend(("--dump-pages", str(pages)))
    if fetches is not None:
        command.extend(("--dump-fetches", str(fetches)))
    if auto_keys is not None:
        command.extend(("--auto-keys", auto_keys))
    if pause_frame is not None:
        command.extend(("--auto-pause-frame", str(pause_frame)))
    if resume_frame is not None:
        command.extend(("--auto-resume-frame", str(resume_frame)))
    subprocess.run(
        command,
        check=True,
    )
    return screen.read_bytes()


def test_build_timestamp_timezone() -> None:
    timestamp = runpy.run_path(str(ROOT / "src/build_info.py"))["build_timestamp"]
    for utc, expected in (
        ("2026-01-15T12:00:00", "15-01-2026 13:00 CET"),
        ("2026-07-15T12:00:00", "15-07-2026 14:00 CEST"),
        ("2026-03-29T00:59:00", "29-03-2026 01:59 CET"),
        ("2026-03-29T01:00:00", "29-03-2026 03:00 CEST"),
        ("2026-10-25T00:59:00", "25-10-2026 02:59 CEST"),
        ("2026-10-25T01:00:00", "25-10-2026 02:00 CET"),
    ):
        assert timestamp(datetime.fromisoformat(utc).replace(tzinfo=timezone.utc)) == expected


def test_cartridge_end_to_end() -> None:
    assert CARTRIDGE.read_bytes()[5:16] == b"P2K-TELETXT"
    with tempfile.TemporaryDirectory(prefix="p2000t-cartridge-") as directory:
        temp = Path(directory)
        build = temp / "build"
        monitor = temp / "monitor.bin"

        subprocess.run(
            ["cmake", "-S", str(EMU), "-B", str(build), "-G", "Ninja"],
            check=True,
        )
        subprocess.run(["cmake", "--build", str(build)], check=True)
        subprocess.run(
            ["python3", str(EMU / "tests" / "make_monitor.py"), str(monitor)],
            check=True,
        )

        before = run_emulator(
            build / "p2000t-emulator", monitor, temp / "before.bin", 10, False
        )
        assert before[0:3] == bytes((0x04, 0x1D, 0x07))
        assert b"P2000T  INTERNET TELETEKST" in before
        assert before[3 * 40 : 3 * 40 + 6] == bytes(
            (0x04, 0x1D, 0x17, 0x04, 0x1D, 0x17)
        )
        assert before[5 * 40 + 13 : 5 * 40 + 15] == bytes((0x7F, 0x7F))
        assert before[9 * 40 + 3] == 0x3C
        assert before[10 * 40 : 10 * 40 + 4] == bytes((0x04, 0x1D, 0x17, 0x35))
        assert before[16 * 40 : 16 * 40 + 3] == bytes((0x17, 0x7C, 0x7C))
        assert b"DRUK OP EEN TOETS" in before
        assert b"PICO VERBINDING TESTEN" not in before

        scanning = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "scanning.bin",
            10,
            True,
        )
        assert scanning[1 * 40 : 2 * 40] == (
            b"\x04\x1d\x07 P2000T  WIFI-INSTELLING             "
        )
        assert scanning[2 * 40 : 3 * 40] == (
            b"\x04\x1d\x07   WIFI-NETWERKEN ZOEKEN...          "
        )
        assert scanning[3 * 40 : 3 * 40 + 3] == bytes((0x07, 0x1D, 0x04))
        assert b"LINK ACTIEF - POLL 0000 |" in scanning
        assert scanning[6 * 40 : 6 * 40 + 2] == bytes((0x14, 0x73))
        assert b"PICO TEST" not in scanning

        # Capture the completed prompt before the automatic keyboard answers N.
        for frame in range(24, 40):
            save_prompt = run_emulator(
                build / "p2000t-emulator", monitor, temp / "save-prompt.bin",
                frame, True,
            )
            if (b"WIFI-PROFIEL BEWAREN? J/N" in save_prompt and
                    b"v0.5.0" in save_prompt[23 * 40:]):
                break
        assert b"WIFI-PROFIEL BEWAREN? J/N" in save_prompt
        assert save_prompt[6 * 40:6 * 40 + 3] == bytes((7, 29, 4))
        assert b"J: BEWAREN VOOR VOLGENDE KEER" in save_prompt
        assert b"N: ALLEEN DEZE SESSIE" in save_prompt
        assert b"v0.5.0" in save_prompt[23 * 40:]
        for row in range(24):
            assert save_prompt[row * 40:row * 40 + 2] in (
                bytes((4, 29)), bytes((7, 29)),
            ), f"save prompt row {row} has no background control"

        source_menu = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "source-menu.bin",
            100,
            True,
        )
        assert source_menu[:6] == bytes(
            (0x04, 0x1D, 0x17, 0x04, 0x1D, 0x17)
        )
        assert b"KIES UW TELETEKSTBRON" in source_menu[14 * 40 : 15 * 40]
        assert b"0\x07  EIGEN SERVER" in source_menu
        assert b"A\x07 AUTOSTART NA 60S: UIT" in source_menu[20 * 40 : 21 * 40]
        assert b"H\x07 HULP" in source_menu[21 * 40 : 22 * 40]
        shifted_menu = run_emulator(
            build / "p2000t-emulator", monitor, temp / "shifted-menu.bin",
            650, True, auto_keys="STOP,LSHIFT,RSHIFT",
        )
        # The monitor status cell overlaps the top stroke above the logo's 2.
        assert shifted_menu[:14 * 40] == source_menu[:14 * 40]
        assert b"START/I INDEX" not in source_menu

        custom_sources = temp / "custom-sources.bin"
        custom_flash = temp / "custom-flash.bin"
        custom = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "custom.bin",
            650,
            True,
            sources=custom_sources,
            auto_source=0,
            custom_server="http://terra:8080",
            flash=custom_flash,
        )
        assert b"NOS Telet" in custom
        assert custom_sources.read_bytes() == bytes((2,))

        restored_custom_sources = temp / "restored-custom-sources.bin"
        restored_custom = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "restored-custom.bin",
            500,
            True,
            sources=restored_custom_sources,
            auto_source=0,
            flash=custom_flash,
        )
        assert b"NOS Telet" in restored_custom
        assert restored_custom_sources.read_bytes() == bytes((2,))

        sources = temp / "sources.bin"
        after = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "after.bin",
            500,
            True,
            sources=sources,
        )
        assert b"NOS Telet" in after
        assert b"Meer bevoegdheden" in after
        assert sources.read_bytes() == bytes((0,))
        assert b"za 05.sep 12:" in after[:40]

        old_archive_sources = temp / "old-archive-sources.bin"
        old_archive = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "old-archive.bin",
            500,
            True,
            protocol_version=6,
            sources=old_archive_sources,
            auto_source=3,
        )
        assert old_archive_sources.read_bytes() == b""
        assert b"ARCHIEF VEREIST P2WP/7" in old_archive

        archive_sources = temp / "archive-sources.bin"
        archive = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "archive.bin",
            500,
            True,
            protocol_version=7,
            sources=archive_sources,
            auto_source=3,
        )
        assert archive_sources.read_bytes() == bytes((3,))
        assert b"NOS Telet" in archive

        settings_flash = temp / "settings-flash.bin"
        run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "settings.bin",
            500,
            True,
            flash=settings_flash,
            auto_source_cycles=1,
        )
        assert settings_flash.read_bytes()[8:11] == bytes((0xFE, 1, 0))

        timed_sources = temp / "timed-sources.bin"
        timed = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "timed.bin",
            3400,
            True,
            flash=settings_flash,
            wait_opening=True,
            sources=timed_sources,
        )
        assert b"NOS Telet" in timed
        assert timed_sources.read_bytes()[0] == 0
        assert timed[35] == ord("V")

        viewer_pages = temp / "viewer-pages.txt"
        viewer = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "viewer.bin",
            1400,
            True,
            pages=viewer_pages,
            auto_keys="KP1,KP0,BACKSPACE,KP0,KP1,START,STOP",
        )
        assert viewer_pages.read_text().splitlines()[:3] == ["100", "101", "100"]
        assert b"KIES UW TELETEKSTBRON" in viewer
        assert b"H\x07 HULP" in viewer

        metadata_pages = temp / "metadata-pages.txt"
        metadata_viewer = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "metadata-viewer.bin",
            650,
            True,
            pages=metadata_pages,
            auto_keys="RIGHT,START,STOP",
        )
        assert metadata_pages.read_text().splitlines()[:3] == ["100", "101", "100"]
        assert b"KIES UW TELETEKSTBRON" in metadata_viewer

        fetching = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "fetching.bin",
            198,
            True,
            auto_keys="RIGHT",
        )
        assert fetching[0] == 0x17
        assert fetching[2:4] == bytes((0x19, 0x07))
        assert b"NOS Telet" in fetching
        assert b"PAGINA WORDT OPGEHAALD" not in fetching

        graphics_page = bytearray(b" " * 960)
        graphics_page[:40] = bytes((0x17,)) + bytes((0x70,)) * 39
        graphics_page[80:90] = b"NOS Telet "
        graphics_fixture = temp / "graphics-header.json"
        graphics_fixture.write_text(
            json.dumps(
                {
                    "nextPage": "101",
                    "nextSubPage": "",
                    "binaryDisplay": base64.b64encode(graphics_page).decode("ascii"),
                }
            )
        )
        graphics_fetching = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "graphics-fetching.bin",
            198,
            True,
            auto_keys="RIGHT",
            fixture=graphics_fixture,
            stall_fetch_after=1,
            clock_valid=False,
        )
        assert graphics_fetching[4:40] == b" " * 36, \
            ("graphics header leaked behind the fetch indicator: " +
             repr(graphics_fetching[:40]))

        rotating_fetches = temp / "rotating-fetches.bin"
        run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "rotating.bin",
            1450,
            True,
            fetches=rotating_fetches,
        )
        assert rotating_fetches.read_bytes()[:3] == bytes((0, 2, 0))

        paused_fetches = temp / "paused-fetches.bin"
        paused = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "paused.bin",
            900,
            True,
            fetches=paused_fetches,
            pause_frame=350,
        )
        assert paused_fetches.read_bytes() == bytes((0,))
        assert paused[39] == ord("A")

        loop_off_fetches = temp / "loop-off-fetches.bin"
        loop_off = run_emulator(
            build / "p2000t-emulator", monitor, temp / "loop-off.bin",
            1450, True, fetches=loop_off_fetches, auto_keys="L",
        )
        assert loop_off_fetches.read_bytes() == bytes((0,))
        assert loop_off[39] == ord("A")

        loop_on_fetches = temp / "loop-on-fetches.bin"
        run_emulator(
            build / "p2000t-emulator", monitor, temp / "loop-on.bin",
            1450, True, fetches=loop_on_fetches, auto_keys="L,L",
        )
        assert loop_on_fetches.read_bytes()[:3] == bytes((0, 2, 0)), \
            "L did not resume looping back to the first subpage"

        resumed_fetches = temp / "resumed-fetches.bin"
        run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "resumed.bin",
            1050,
            True,
            fetches=resumed_fetches,
            pause_frame=350,
            resume_frame=450,
        )
        assert resumed_fetches.read_bytes()[:2] == bytes((0, 2)), \
            "A did not resume automatic subpage progression"

        manual_fetches = temp / "manual-fetches.bin"
        run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "manual.bin",
            650,
            True,
            fetches=manual_fetches,
            auto_keys="S,KP2,ENTER,STOP",
        )
        assert manual_fetches.read_bytes()[:2] == bytes((0, 2))

        stepped_fetches = temp / "stepped-fetches.bin"
        run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "stepped.bin",
            700,
            True,
            fetches=stepped_fetches,
            auto_keys=">,<,STOP",
        )
        stepped_subpages = stepped_fetches.read_bytes()
        assert stepped_subpages[:3] == bytes((0, 2, 1)), stepped_subpages

        zoom_removed = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "zoom-removed.bin",
            650,
            True,
            auto_keys="Z",
        )
        assert zoom_removed[0] != 0x0D
        assert b"NOS Telet" in zoom_removed

        shifted = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "shifted.bin",
            650,
            True,
            auto_keys="LSHIFT,RSHIFT",
        )
        assert b"12:34:" in shifted[:40]
        assert b"NOS Telet" in shifted

        help_page = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "help.bin",
            650,
            True,
            auto_keys="H",
        )
        expected_help = bytearray(b" " * 960)

        def help_band(row: int, text: str) -> None:
            offset = row * 40
            expected_help[offset:offset + 3] = bytes((0x04, 0x1D, 0x07))
            encoded = text.encode("ascii")[:37]
            expected_help[offset + 3:offset + 3 + len(encoded)] = encoded

        def help_row(row: int, *parts: tuple[int, str | bytes]) -> None:
            offset = row * 40
            for colour, text in parts:
                expected_help[offset] = colour
                offset += 1
                encoded = text if isinstance(text, bytes) else text.encode("ascii")
                expected_help[offset:offset + len(encoded)] = encoded
                offset += len(encoded)

        help_band(0, " P2000T TELETEKST  HULP")
        help_row(2, (0x03, " PAGINA"))
        help_row(3, (0x06, " 100-899"), (0x07, "  KIES PAGINA"))
        help_row(4, (0x06, " START / I"), (0x07, " INDEX PAGINA 100"))
        help_row(5, (0x06, b"\x5b / P"), (0x07, " VORIGE  "),
                 (0x06, b"\x5d / N"), (0x07, " VOLGENDE"))
        help_row(6, (0x06, " V"), (0x07, " AUTO VOLGENDE PAGINA"))
        help_row(8, (0x03, " SUBPAGINA'S"))
        help_row(9, (0x06, " < / >"), (0x07, " VORIGE / VOLGENDE"))
        help_row(10, (0x06, " S"), (0x07, " KIES EEN SUBPAGINA"))
        help_row(11, (0x06, " L / A"), (0x07, " LUSSEN AAN/UIT"))
        help_row(12, (0x03, " WEERGAVE"))
        help_row(13, (0x06, " ? / R"), (0x07, " VERBORGEN TEKST TONEN"))
        help_row(15, (0x03, " VERBINDING"))
        help_row(16, (0x06, " W"), (0x07, " KIES EEN ANDER WIFI-NETWERK"))
        help_row(17, (0x06, " STOP"), (0x07, " ANDERE TELETEKSTBRON"))
        help_row(19, (0x03, " BRONKEUZE"))
        help_row(20, (0x06, " A"), (0x07, " AUTOSTARTBRON WIJZIGEN"))
        help_row(21, (0x06, " H"), (0x07, " HULP VANAF DE BRONKEUZE"))
        help_band(22, "2: INFO   ANDERE TOETS: TERUG")
        help_band(23, "P2000T Teletekst Cartridge     v0.5.0")
        assert help_page == expected_help

        info_page = run_emulator(
            build / "p2000t-emulator", monitor, temp / "info.bin",
            650, True, auto_keys="H,2",
        )
        assert b"INFO 2/2" in info_page
        assert b"Z88DK 2.4 / SDCC - Z80" in info_page
        assert b"github.com/ifilot/" in info_page
        assert b"p2000t-teletekst-cartridge" in info_page
        assert b"CET" in info_page or b"CEST" in info_page
        for row in (0, 22, 23):
            assert info_page[row * 40:row * 40 + 3] == bytes((4, 29, 7))
        for row in (2, 8, 13):
            assert info_page[row * 40] == 3  # Same yellow section headings.
        for row in (3, 15):
            assert info_page[row * 40] == 6  # Cyan labels on black.
        for row in (1, 5, 6, 7, 9, 11, 12, 14, 17, 18, 19, 21):
            assert info_page[row * 40:(row + 1) * 40] == b" " * 40
        help_back = run_emulator(
            build / "p2000t-emulator", monitor, temp / "help-back.bin",
            650, True, auto_keys="H,2,1",
        )
        assert help_back == expected_help

        help_pages = temp / "help-pages.txt"
        restored = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "help-restored.bin",
            750,
            True,
            pages=help_pages,
            auto_keys="H,ENTER,STOP",
        )
        assert help_pages.read_text().splitlines() == ["100"]
        assert b"KIES UW TELETEKSTBRON" in restored

        reveal_page = bytearray(b" " * 960)
        reveal_page[80:90] = b"NOS Telet "
        reveal_page[40:54] = bytes((0x07,)) + b"PUBLIC" + bytes((0x18,)) + b"SECRET"
        reveal_fixture = temp / "reveal.json"
        reveal_fixture.write_text(
            json.dumps(
                {
                    "nextSubPage": "",
                    "binaryDisplay": base64.b64encode(reveal_page).decode("ascii"),
                }
            )
        )
        revealed = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "revealed.bin",
            650,
            True,
            auto_keys="?",
            fixture=reveal_fixture,
        )
        assert revealed[47] == 0x07
        assert revealed[48:54] == b"SECRET"

        rotating_reveal_fixture = temp / "rotating-reveal.json"
        rotating_reveal_fixture.write_text(
            json.dumps(
                {
                    "prevPage": "",
                    "nextPage": "101",
                    "nextSubPage": "100-1",
                    "binaryDisplay": base64.b64encode(reveal_page).decode("ascii"),
                }
            )
        )
        rotating_reveal_fetches = temp / "rotating-reveal-fetches.bin"
        rotating_revealed = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "rotating-revealed.bin",
            1350,
            True,
            fetches=rotating_reveal_fetches,
            auto_keys="?",
            fixture=rotating_reveal_fixture,
        )
        assert rotating_reveal_fetches.read_bytes()[:3] == bytes((0, 1, 0))
        assert rotating_revealed[47] == 0x07
        assert rotating_revealed[48:54] == b"SECRET"

        reconcealed = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "reconcealed.bin",
            675,
            True,
            auto_keys="?,?",
            fixture=reveal_fixture,
        )
        assert reconcealed[47] == 0x18
        assert reconcealed[48:54] == b"SECRET"

        auto_pages = temp / "auto-pages.txt"
        automatic = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "automatic.bin",
            1900,
            True,
            pages=auto_pages,
            auto_keys="V",
            repeat_fixture_subpage=True,
        )
        assert auto_pages.read_text().splitlines()[:3] == ["100", "100", "101"]
        assert automatic[35] == ord("V")

        for pause_key in ("A", "L"):
            held_pages = temp / f"held-{pause_key}-pages.txt"
            held_fetches = temp / f"held-{pause_key}-fetches.bin"
            held = run_emulator(
                build / "p2000t-emulator", monitor,
                temp / f"held-{pause_key}.bin", 1900, True,
                pages=held_pages, fetches=held_fetches,
                auto_keys=f"V,{pause_key}", repeat_fixture_subpage=True,
            )
            assert held_pages.read_text().splitlines() == ["100"], \
                f"{pause_key} let autorun replace the held page"
            assert held_fetches.read_bytes() == bytes((0,))
            assert held[35] == ord("V")  # Autorun remains selected but held.
            assert held[39] == ord("A")

        resumed_auto_pages = temp / "resumed-auto-pages.txt"
        run_emulator(
            build / "p2000t-emulator", monitor, temp / "resumed-auto.bin",
            1900, True, pages=resumed_auto_pages, auto_keys="V,L,L",
            repeat_fixture_subpage=True,
        )
        assert resumed_auto_pages.read_text().splitlines()[:3] == [
            "100", "100", "101",
        ]

        auto_skip_pages = temp / "auto-skip-pages.txt"
        run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "auto-skip.bin",
            1900,
            True,
            pages=auto_skip_pages,
            auto_keys="V",
            fail_page=101,
            fail_error=7,
        )
        assert auto_skip_pages.read_text().splitlines()[:4] == [
            "100", "100", "101", "102"
        ]

        timeout = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "timeout.bin",
            450,
            True,
            auto_keys="KP1,KP0,KP1",
            fail_page=101,
            fail_error=12,
        )
        assert b"FOUTCODE: 0C" in timeout
        assert b"FOUT: SERVER REAGEERT NIET" in timeout
        assert b"DETAIL: HTTP 000 LWIP 00 NET 00" in timeout

        not_found_input = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "not-found-input.bin",
            650,
            True,
            auto_keys="KP1,KP0,KP1,KP2",
            fail_page=101,
            fail_error=8,
        )
        assert not_found_input[36] == ord("2"), \
            "the not-found clock overwrote page-number entry"
        assert not_found_input[29] == ord(":")
        assert not_found_input[32] == ord(":")
        assert not_found_input[35] == ord(" "), \
            "the compact error-screen clock overlapped page-number entry"

        firmware_timeout = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "firmware-timeout.bin",
            3400,
            True,
            stall_fetch=True,
        )
        assert b"FOUTCODE: 0C" in firmware_timeout
        assert b"FOUT: SERVER REAGEERT NIET" in firmware_timeout

        fallback_timeout = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "fallback-timeout.bin",
            8000,
            True,
            stall_fetch=True,
            firmware_fetch_timeout=False,
        )
        assert b"FOUTCODE: 81" in fallback_timeout
        assert b"STOP: BRON" in fallback_timeout
        assert b"v0.5.0" in fallback_timeout[23 * 40:]
        assert b"FOUT: TIMEOUT" in fallback_timeout

        wifi_return = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "wifi-return.bin",
            750,
            True,
            auto_keys="W,STOP",
        )
        assert b"KIES UW TELETEKSTBRON" in wifi_return

        secured = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "secured.bin",
            500,
            True,
            wifi_security=1,
        )
        assert b"NOS Telet" in secured

        masked_entry = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "masked-entry.bin",
            51,
            True,
            wifi_security=1,
        )
        assert masked_entry[14 * 40 : 14 * 40 + 15] == b"\x07\x1d\x04WACHTWOORD>*"

        visible_entry = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "visible-entry.bin",
            51,
            True,
            wifi_security=1,
            password_visible=True,
        )
        assert visible_entry[14 * 40 : 14 * 40 + 15] == b"\x07\x1d\x04WACHTWOORD>p"

        legacy = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "legacy.bin",
            500,
            True,
            2,
        )
        assert b"NOS Telet" in legacy

        profile = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "profile.bin",
            100,
            True,
            saved_profile=True,
        )
        assert b"KIES UW TELETEKSTBRON" in profile
        assert b"1\x07  NOS TELETEKST" in profile

        incompatible = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "incompatible.bin",
            30,
            True,
            1,
        )
        assert b"GEEN GEDEELDE P2WP-VERSIE" in incompatible
