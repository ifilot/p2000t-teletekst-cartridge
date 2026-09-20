# Z88DK cartridge migration handoff

Snapshot date: 2026-09-20  
Working branch: `port-c`

## Purpose

The 16 KiB P2000T slot-1 cartridge is being migrated incrementally from the
single `src/p2wp-cartridge.asm` program to maintainable C compiled with Z88DK.
The assembly cartridge remains intact and is still the production/release ROM.
The C cartridge is a parallel development image and is not yet at behavioural
parity.

Do not remove or replace the assembly build until the C implementation has
passed full hardware and parity testing.

## Current working-tree state

The migration work is not committed. Preserve the existing modified and
untracked files; do not clean or reset the tree before reviewing them.

Important new files:

- `src/cartridge-c/crt0.asm`: P2000T ROM/RAM layout and Z88DK startup.
- `src/cartridge-c/platform.asm`: monitor, clock and Pico port shims.
- `src/cartridge-c/platform.c`: keyboard translation and video-RAM access.
- `src/cartridge-c/p2wp.c`: framing, escaping, CRC, retry, sequencing and
  P2WP/2-7 negotiation.
- `src/cartridge-c/wifi.c`: saved-profile startup and interactive Wi-Fi
  onboarding.
- `src/cartridge-c/ui.c`: reusable SAA5050 blue/white panel primitives.
- `src/cartridge-c/teletekst.c`: source selection and initial page retrieval.
- `src/make_cartridge_rom.py`: ROM padding and payload-size reporting.
- `emulator/tests/test_c_cartridge_smoke.py`: end-to-end C cartridge test.

The emulator has also gained options for an emulated saved Wi-Fi profile and
open/WPA network selection. The build workflow and repository documentation
contain corresponding migration targets and notes.

## What currently works

The C image currently performs this sequence:

1. Starts through the custom CRT at `0x1010`.
2. Displays a partially restored SAA5050 opening screen.
3. Negotiates P2WP/2 through P2WP/7 and runs device-info and escaped-ECHO
   diagnostics.
4. Connects through a saved encrypted Wi-Fi profile when one is available.
5. Otherwise scans for up to nine networks, accepts a selection and masked
   WPA/WPA2 password, connects, and optionally saves the encrypted profile.
6. Displays a blue/white Teletekst-style source menu.
7. Selects NOS, P2000T, or TeletekstArchief.nl. Archive selection currently
   requires P2WP/7.
8. Starts an asynchronous request for page 100, polls its status, downloads
   four validated 240-byte chunks into a 960-byte staging buffer, and commits
   the completed SAA5050 page to video RAM.

The UI restoration is intentionally partial. The opening, source-selection and
loading screens use blue-background headers, white/blue panels, mosaic rules,
a small mosaic motif and the cartridge footer. They are not yet a pixel-exact
port of every assembly screen.

## Current program size

The most recent successful build reports:

| Phase | Linked bytes | Increase | Free ROM capacity |
| --- | ---: | ---: | ---: |
| Generic protocol and Wi-Fi scan | 5,462 | - | 10,922 |
| Interactive Wi-Fi onboarding | 7,143 | 1,681 | 9,241 |
| Source menu and page-100 display | 9,184 | 2,041 | 7,200 |

The current 9,184 bytes consist of 9,183 code/read-only-data bytes and one
initialized-data byte. The output ROM is always padded and signed to exactly
16,384 bytes. The 7,200-byte figure is unused ROM capacity, not free RAM.

Every `c-rom` build prints the current linked and remaining sizes.

## Build and test

Build the C cartridge with the pinned Z88DK 2.4 Docker image:

```sh
make -C src c-rom
```

The hardware-test image is:

```text
src/p2wp-cartridge-c.bin
```

Run the end-to-end emulator test:

```sh
make -C src c-smoke
```

Run the unaffected production-ROM and firmware protocol checks:

```sh
make -C src verify
make -C firmware/tests test
python3 -m py_compile emulator/tests/test_c_cartridge_smoke.py \
  src/make_cartridge_rom.py
git diff --check
```

At this snapshot all commands above pass. The smoke test covers the graphical
opening, open Wi-Fi, WPA/WPA2 password entry, a saved profile, P2WP/2 legacy
operation, incompatible negotiation, source selection, all four page chunks,
and rendered NOS page content.

## Hardware-test status

The Wi-Fi-onboarding phase was confirmed working on real hardware by the user.
The newer source-selection, graphical-layout and page-100 phase has passed the
emulator but has not yet been reported as tested on real hardware.

For the next hardware session:

1. Program `src/p2wp-cartridge-c.bin` as a raw 16 KiB cartridge image.
2. Check the new opening screen and press a key.
3. Continue through Wi-Fi; a valid saved profile should bypass scanning.
4. Select source `1`, `2`, or `3` (`3` needs P2WP/7).
5. Confirm that page 100 appears with its provider-supplied SAA5050 graphics
   and colours.
6. Expect the cartridge to stop on that page; interactive viewing is not yet
   implemented.

## Known limitations

- After page 100 is displayed, execution halts in the main terminal loop.
- There is no three-digit page entry, page navigation, subpage cycling,
  pause/reveal/zoom handling, help screen or source-change loop yet.
- Custom-server entry, persisted custom URLs and autostart settings have not
  been migrated.
- Archive compatibility through the custom-source fallback for P2WP/4-6 has
  not been ported; the C menu accepts Archive only with P2WP/7.
- Fetch metadata such as next subpage, previous/next page and clock is
  validated as part of the version-specific status length but is not retained
  or displayed yet.
- Error presentation and recovery are basic compared with the assembly ROM.
- The 960-byte screen commit writes rows directly to video RAM. The assembly
  version's vertical-retrace/video-blanked atomic commit is still to be ported.
- The graphical opening contains only a small provisional mosaic motif rather
  than the complete seven-row assembly artwork.

## Recommended next phase

Build the first persistent page-viewer loop around the existing fetch code:

1. Introduce a small viewer state containing source, page, subpage,
   next-subpage and previous/next-page metadata.
2. Refactor `fetch_page()` so it fills that state and returns a detailed result
   instead of only success/failure.
3. Add three-digit page input for pages 100-899 and fetch immediately after the
   third digit.
4. Add `START`/`I` for page 100 and `STOP` for returning to source selection.
5. Keep the first loop deliberately small; add timed subpages, reveal, zoom,
   custom sources and automatic page mode in later slices.
6. Extend the emulator smoke test to inject a page number, assert the requested
   page log, press `STOP`, and verify that the graphical source menu returns.
7. Rebuild, record linked/free ROM sizes, and test the resulting binary on
   hardware before continuing.

The assembly implementation around `teletekst_main_loop`,
`teletekst_fetch_page`, `teletekst_accept_input` and
`teletekst_choose_source` is the behavioural reference. Port behaviour in
bounded slices rather than translating that entire block at once.

## Memory and build notes

- ROM begins at `0x1000`; execution begins at `0x1010`.
- mutable sections begin at `0x7000`.
- the stack begins at `0x9ff0`.
- P2WP response payloads are currently limited to 240 bytes on the cartridge,
  matching one page chunk.
- Compiler maps, listings and symbols are generated in `src/build-c` or as
  ignored `.lis`/`.sym` files beside the sources.
- The Docker image is pinned by tag and digest in `src/Makefile`.
- `src/p2wp-cartridge-c.bin` is generated and ignored by Git.

