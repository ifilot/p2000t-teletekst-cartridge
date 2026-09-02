# Changelog

All notable changes to the P2000T Teletekst Cartridge project are documented
in this file. The format is based on [Keep a Changelog].

Entries through version 0.2.0 were reconstructed from the tagged Git history
and release diffs.

## [Unreleased]

### Added

- Added session-only custom HTTP(S) Teletekst servers, including DNS names,
  IPv4 addresses, optional ports/base paths, and an intentionally permissive
  custom-endpoint TLS mode for self-signed and private-CA certificates.
- Added `START`/`I` index, `?`/`R` reveal, three-state `Z` zoom, and metadata-driven
  `P`/`N` previous/next page shortcuts, with an expanded on-screen reference.
- Added P2WP/4 custom URL requests and previous/next page metadata while
  retaining P2WP/2–3 compatibility.
- Added deterministic emulator coverage for custom URL entry and shortcuts.
- Added a dependency-free Python example server with editable text or raw
  SAA5050 pages and localhost HTTP tests.

### Changed

- Moved pause/resume from `P` to `A` so `P` can mean previous page.

## [0.4.0] - 2026-08-29

### Added

- Added a visual P2000T emulator that boots the real monitor and cartridge
  ROMs and emulates the slot-2 P2WP interface, including fixture-backed and
  live Teletekst access.
- Added headless end-to-end coverage for both NOS and P2000T Teletekst sources.
- Added Dutch date and time synchronization, local clock advancement, midnight
  calendar rollover, and source-specific clock presentation.
- Added negotiated P2WP/2–3 operation, a legacy-firmware compatibility warning,
  and a dedicated no-common-protocol error screen.

### Changed

- Reworked the opening screen around a joined native SAA5050 `P2000T
  TELETEKST` mosaic logo, a blinking start prompt, and a fixed release footer.
- Polished the Teletekst source selector with aligned choices and a complete
  one-action-per-line key reference.
- Added emulator keyboard shortcuts for the P2000T STOP key and machine reset.
- Kept the original five-byte P2WP/2 fetch status for bidirectional v0.3
  compatibility; the thirteen-byte date/time status is now explicitly P2WP/3.

## [0.3.0] - 2026-08-28

### Added

- Added a two-second timeout for a missing or unresponsive Pico interface.
- Added controls for returning to Wi-Fi selection, pausing automatic subpage
  cycling, and selecting a specific subpage.
- Added a cartridge-resident Teletekst-style help page.
- Added a fully Dutch cartridge interface.
- Added a native C replay utility that runs captured or live NOS responses
  through the same Teletekst decoder used by the Pico firmware.
- Added decoder diagnostics that distinguish invalid function arguments,
  malformed external JSON fields (`nextSubPage` and `binaryDisplay`), malformed
  page content, and unrepresentable SAA5050 rows.
- Added regression coverage for bare ampersands and source rows that change
  directly between graphics and alphabetic modes.
- Added Sphinx documentation for the hardware interface, P2WP/2 assembly use,
  and P2000T BASIC use, including a complete BASIC example.
- Added GitHub Pages build and deployment jobs and a manual workflow trigger.
- Added detailed cartridge and Pico W/Pico 2 W installation instructions.

### Changed

- Added consistent Doxygen-style contracts for all maintained C functions and
  Doxygen-compatible docstrings for the cartridge signing helper.
- Removed generated `src/*.bin` cartridge images from version control; release
  workflows continue to build and publish the signed ROM.
- Cleared the previous page number when page entry starts and reserved the
  top-right cell for the pause indicator.
- Allowed the SAA5050 row compiler to use a source glyph as a control-code
  position when a page cannot otherwise be represented. Blank cells remain
  preferred, followed by graphics cells, so alphabetic text is preserved
  whenever possible.
- Moved the protocol specification into the documentation tree and expanded
  the project README and website content.

### Fixed

- Removed page numbers from error-screen headers so they cannot interfere with
  new page entry.
- Accepted bare ampersands in NOS HTML-like content instead of treating every
  ampersand as the start of an HTML entity. This fixes pages such as 101, 102,
  and 106 when their headlines contain `Saints & Stars`.
- Prevented error 07 on pages that switch directly from mosaic graphics to
  alphabetic text without reserving a cell for an SAA5050 mode control. This
  fixes pages such as 200.

## [0.2.0] - 2026-08-21

### Added

- Added support for the P2000T service's base64 `binaryDisplay` field, allowing
  exact 960-byte SAA5050 display images to preserve flash, conceal, double
  height, separated graphics, and hold graphics attributes.
- Added an exact-display regression fixture and malformed `binaryDisplay`
  validation tests.
- Added embedded Pico firmware metadata for identification with `picotool`.
- Added separate release artifacts and documentation for Pico W and Pico 2 W.
- Added the project license, release/download links, hardware imagery, and a
  rendered schematic link.

### Changed

- Revised the PCB design to hardware revision v1.1 and removed diode D1.
- Updated the enclosure bottom model for the revised hardware.
- Updated the cartridge ROM and Pico firmware release metadata to version
  0.2.0 while retaining P2WP wire-protocol version 2.

## [0.1.0] - 2026-08-17

### Added

- Initial public release of the 16 KiB P2000T slot-1 cartridge ROM.
- Initial Raspberry Pi Pico W firmware for Wi-Fi setup and Internet-backed
  Teletekst page retrieval.
- P2WP/2 framed link protocol between the P2000T cartridge and Pico interface.
- NOS and P2000T community Teletekst source selection, subpage rotation, and
  SAA5050 display compilation.
- Encrypted Wi-Fi profile storage and automatic reconnection.
- KiCad PCB design, manufacturing outputs, enclosure models, and build/release
  automation.
