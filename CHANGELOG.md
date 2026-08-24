# Changelog

All notable changes to the P2000T Teletekst Cartridge project are documented
in this file. The format is based on [Keep a Changelog].

Entries through version 0.2.1 were reconstructed from the tagged Git history
and release diffs.

## [Unreleased]

## [0.2.1] - 2026-08-24

### Added

- Added a native C replay utility that runs captured or live NOS responses
  through the same Teletekst decoder used by the Pico firmware.
- Added decoder diagnostics that distinguish invalid arguments,
  `nextSubPage`, `binaryDisplay`, content, and unrepresentable SAA5050 rows.
- Added regression coverage for bare ampersands and source rows that change
  directly between graphics and alphabetic modes.
- Added Sphinx documentation for the hardware interface, P2WP/2 assembly use,
  and P2000T BASIC use, including a complete BASIC example.
- Added GitHub Pages build and deployment jobs and a manual workflow trigger.
- Added detailed cartridge and Pico W/Pico 2 W installation instructions.

### Changed

- Allowed the SAA5050 row compiler to use a source glyph as a control-code
  position when a page cannot otherwise be represented. Blank cells remain
  preferred, followed by graphics cells, so alphabetic text is preserved
  whenever possible.
- Moved the protocol specification into the documentation tree and expanded
  the project README and website content.

### Fixed

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