#!/usr/bin/env python3
"""Boot the Z88DK cartridge and verify C and monitor-call execution."""

from pathlib import Path
import base64
import json
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
EMU = ROOT / "emulator"
CARTRIDGE = ROOT / "src" / "p2wp-cartridge-c.bin"


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
    fixture: Path | None = None,
    auto_source: int | None = None,
    custom_server: str | None = None,
    flash: Path | None = None,
    auto_source_cycles: int = 0,
    wait_opening: bool = False,
    fail_page: int = 0,
    fail_error: int = 0,
    repeat_fixture_subpage: bool = False,
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
    subprocess.run(
        command,
        check=True,
    )
    return screen.read_bytes()


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="p2000t-c-smoke-") as directory:
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
        assert before[3 * 40 : 3 * 40 + 3] == bytes((0x04, 0x1D, 0x17))
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

        source_menu = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "source-menu.bin",
            100,
            True,
        )
        assert source_menu[1 * 40 : 2 * 40] == (
            b"\x04\x1d\x07 P2000T TELETEKST           BRONKEUZE"
        )
        assert b"0 - EIGEN SERVER" in source_menu
        assert b"START/I INDEX       ?/R ONTHUL" in source_menu
        assert b"CARTRIDGE: v0.5.0 / PICO v0.5.0" in source_menu
        assert b"LAATSTE VERSIE ONLINE: v0.5.0" in source_menu

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

        legacy_archive_sources = temp / "legacy-archive-sources.bin"
        run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "legacy-archive.bin",
            500,
            True,
            protocol_version=6,
            sources=legacy_archive_sources,
            auto_source=3,
        )
        assert legacy_archive_sources.read_bytes() == bytes((2,))

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
        assert b"STOP - ANDERE TELETEKSTBRON" in viewer

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

        def help_line(row: int, foreground: int, colour: int, text: str) -> None:
            offset = row * 40
            expected_help[offset:offset + 3] = bytes((foreground, 0x1D, colour))
            encoded = text.encode("ascii")[:37]
            expected_help[offset + 3:offset + 3 + len(encoded)] = encoded

        def help_rule(row: int) -> None:
            offset = row * 40
            expected_help[offset] = 0x14
            expected_help[offset + 1:offset + 40] = bytes((0x73,)) * 39

        help_line(0, 0x04, 0x07, " P2000T  HULP")
        help_rule(1)
        help_line(2, 0x07, 0x04, "       BEDIENING VAN DE CARTRIDGE")
        help_line(4, 0x04, 0x07, " PAGINA EN VERBINDING")
        help_line(5, 0x07, 0x04, " 100-899  TYP DRIE CIJFERS")
        help_line(6, 0x07, 0x04, " START/I  INDEXPAGINA 100")
        help_line(7, 0x07, 0x04, " <-/P ->/N VORIGE / VOLGENDE PAGINA")
        help_line(8, 0x07, 0x04, " V        AUTO VOLGENDE PAGINA")
        help_line(9, 0x04, 0x07, " WEERGAVE")
        help_line(10, 0x07, 0x04, " ?/R      VERBORGEN TEKST ONTHULLEN")
        help_line(12, 0x04, 0x07, " SUBPAGINA'S")
        help_line(13, 0x07, 0x04, " S        KIES EEN SUBPAGINA")
        help_line(14, 0x07, 0x04, " A        SUBPAGINA PAUZE / DOOR")
        help_line(15, 0x07, 0x04, " W        KIES EEN ANDER WIFI-NETWERK")
        help_line(16, 0x07, 0x04, " STOP     ANDERE BRON / INVOER TERUG")
        help_line(17, 0x07, 0x04, " H        DEZE HULPPAGINA")
        help_rule(20)
        help_line(22, 0x04, 0x07, "     DRUK EEN TOETS OM TERUG TE GAAN")
        assert help_page == expected_help

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
        assert b"1 - NOS TELETEKST" in profile

        incompatible = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "incompatible.bin",
            30,
            True,
            1,
        )
        assert b"GEEN GEDEELDE P2WP-VERSIE" in incompatible

    print("Z88DK cartridge smoke test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
