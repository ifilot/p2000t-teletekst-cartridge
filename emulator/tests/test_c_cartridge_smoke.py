#!/usr/bin/env python3
"""Boot the Z88DK cartridge and verify C and monitor-call execution."""

from pathlib import Path
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
) -> bytes:
    command = [
        str(binary),
        "--monitor",
        str(monitor),
        "--cartridge",
        str(CARTRIDGE),
        "--fixture",
        str(EMU / "tests" / "fixtures" / "nos-100.json"),
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
    if protocol_version != 0:
        command.extend(("--p2wp-version", str(protocol_version)))
    if saved_profile:
        command.append("--wifi-profile")
    if wifi_security:
        command.extend(("--wifi-security", str(wifi_security)))
    if sources is not None:
        command.extend(("--dump-sources", str(sources)))
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
        assert b"DRUK OP EEN TOETS" in before
        assert b"PICO VERBINDING TESTEN" not in before

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

        secured = run_emulator(
            build / "p2000t-emulator",
            monitor,
            temp / "secured.bin",
            500,
            True,
            wifi_security=1,
        )
        assert b"NOS Telet" in secured

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
