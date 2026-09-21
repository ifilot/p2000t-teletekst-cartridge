# Z88DK cartridge migration handoff

Snapshot date: 2026-09-21
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
3. Negotiates P2WP/2 through P2WP/7 without exposing an internal diagnostics
   screen, and reads device information for the source-menu runtime line.
4. Connects through a saved encrypted Wi-Fi profile when one is available.
5. Otherwise scans for up to nine networks, accepts a selection, asks whether
   WPA/WPA2 password entry should be visible (`J`) or masked (`N`), accepts up
   to 63 characters across two rows, connects, and optionally saves the
   encrypted profile. The list and password screens now match the assembly
   screen bytes in emulator comparison.
6. Displays a blue/white Teletekst-style source menu.
7. Selects NOS, P2000T, a custom server, or TeletekstArchief.nl. The custom URL
   editor supports 96 characters and P2WP/5 persistence; Archive currently
   requires P2WP/7.
8. Starts an asynchronous request for page 100, polls its status, downloads
   four validated 240-byte chunks into a 960-byte staging buffer, and commits
   the completed SAA5050 page to video RAM.
9. Remains in an interactive viewer: three digits select pages 100-899,
   Backspace edits input, `START`/`I` retrieves page 100, `P`/left and
   `N`/right follow provider metadata, and `STOP` returns to the graphical
   source menu.
10. Follows subpage metadata every ten seconds, wraps from the last subpage to
    the default first subpage, supports `A` pause/resume with a visible marker,
    and accepts manual `S` subpage selection using one digit plus Enter or two
    digits.
11. Supports reveal (`R`/`?`), three-state zoom (`Z`), help with exact display
    restoration (`H`), ten-second automatic next-page mode (`V`), and Wi-Fi
    reselection (`W`).

The UI restoration is intentionally partial. The opening and source-selection
screens use blue-background headers, white/blue panels, mosaic rules and the
cartridge footer. Page loading preserves the current display and uses the
assembly version's rotating upper-left mosaic tile. Not every remaining screen
is yet a pixel-exact assembly port.

## Current program size

The most recent successful build reports:

| Phase | Linked bytes | Increase | Free ROM capacity |
| --- | ---: | ---: | ---: |
| Generic protocol and Wi-Fi scan | 5,462 | - | 10,922 |
| Interactive Wi-Fi onboarding | 7,143 | 1,681 | 9,241 |
| Source menu and page-100 display | 9,184 | 2,041 | 7,200 |
| Complete opening-screen mosaic | 9,791 | 607 | 6,593 |
| Persistent page-viewer loop | 11,076 | 1,285 | 5,308 |
| Metadata navigation, subpages and password parity | 13,503 | 2,427 | 2,881 |
| Assembly-style page-fetch indicator | 13,539 | 36 | 2,845 |
| Wi-Fi/source-screen parity | 13,001 | -538 | 3,383 |
| Viewer controls, help, auto-page and Wi-Fi return | 15,110 | 2,109 | 1,274 |
| Custom server with persisted URL | 16,263 | 1,153 | 121 |
| Assembly video/arithmetic optimization | 15,608 | -655 | 776 |

The current 15,608 bytes consist of 15,607 code/read-only-data bytes and one
initialized-data byte. The output ROM is always padded and signed to exactly
16,384 bytes. The 776-byte figure is unused ROM capacity, not free RAM.

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
rendered NOS and custom-server page content, persisted custom URLs across two
emulator boots, edited three-digit page entry, `START` recovery,
`STOP` return to the source menu, previous/next metadata navigation, automatic
and manual subpages, paused rotation, reveal, zoom, help restoration, automatic
next-page mode, Wi-Fi reselection, and visible versus masked password entry.
A separate byte-for-byte assembly/C emulator comparison of the Wi-Fi list and
password-visibility prompt also passes at the corresponding screen states.
Page-fetch coverage verifies that the previous page remains visible behind the
same four-byte rotating mosaic indicator used by the assembly cartridge.
The post-opening Wi-Fi scan and full source-selection screens are also compared
at stable emulator states and match the assembly display bytes exactly. The
former C-only `PICO TEST` transition screen has been removed.

## Hardware-test status

All phases through navigation, subpages, password parity, and the restored
screen layouts were confirmed on real hardware by the user. The controls and
custom-server additions in this snapshot have passed the emulator but still
need a hardware run.

For the next hardware session:

1. Program `src/p2wp-cartridge-c.bin` as a raw 16 KiB cartridge image.
2. Check the new opening screen and press a key.
3. Continue through Wi-Fi; a valid saved profile should bypass scanning.
4. Select source `1`, `2`, or `3` (`3` needs P2WP/7), then source `0` and enter
   a custom base URL. Reopen source `0` after a reboot to verify persistence.
5. Confirm that page 100 appears with its provider-supplied SAA5050 graphics
   and colours.
6. On a protected network, verify both `J` (visible) and `N` (asterisks) at the
   password visibility prompt, including Backspace and passwords longer than
   26 characters if practical.
7. Enter a three-digit page number, use Backspace while entering another, and
   confirm the requested page is fetched after its third digit.
8. Try `N`/right on page 100, `S` plus a subpage number, and `A` pause/resume;
   verify the top-right `A` marker and ten-second automatic advance.
9. Press `START` or `I` to return to page 100.
10. Press `STOP` and confirm that the graphical source menu returns.
11. Exercise `R`/`?`, all three `Z` states, `H`, `V`, and `W`.

## Known limitations

- Persistent source-menu autostart settings have not been migrated.
- Archive compatibility through the custom-source fallback for P2WP/4-6 has
  not been ported; the C menu accepts Archive only with P2WP/7.
- Clock metadata is validated by response length but not displayed.
- Error presentation and recovery are basic compared with the assembly ROM.
- The page-fetch indicator now matches assembly, but the 960-byte screen commit
  still writes rows directly to video RAM. The assembly
  version's vertical-retrace/video-blanked atomic commit is still to be ported.
- The assembly autostart countdown on the opening screen has not been ported;
  the C opening otherwise uses the complete original fourteen-row joined
  P2000T/TELETEKST mosaic.

## Recommended next phase

Test this image on hardware before adding more behavior. The assembly video
primitives and fixed-purpose decimal conversion recovered 655 bytes, leaving
776 ROM bytes. The map now contains no `sprintf`/`printf`, general division,
general multiplication, or arithmetic-error runtime. The next phase can use
the recovered space for the live clock and detailed error/recovery behavior;
source-menu autostart and the P2WP/4-6 Archive fallback also remain.

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
