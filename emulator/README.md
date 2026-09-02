# P2000T + Teletekst cartridge emulator

This is a focused graphical P2000T emulator for the SLOT1 ROM and SLOT2 Pico W
interface developed in this repository. It uses the M2000 Z80/P2000T core and
authentic `Default.fnt` SAA5050 glyphs. SDL provides the display and keyboard.

Dependencies: CMake, Ninja, SDL2, libcurl, and a C compiler. On Debian/Ubuntu:

```sh
sudo apt install cmake ninja-build libsdl2-dev libcurl4-openssl-dev
```

Build the cartridge and emulator:

```sh
make -C src
make -C emulator
```

and run it via

```sh
emulator/run
```

The emulator presents one open network named `Emulated WiFi`; select it with
`1`, decline profile storage with `N`, then choose NOS with `1`. Choose `0` to
type a custom server URL. Regular host
letter, number, arrow, Enter, Backspace, Shift and keypad-Enter (P2000 STOP)
keys are mapped to the P2000T keyboard matrix. Press `F11` for a warm reset or
`F12` for a cold reset.

Page shortcuts are `START`/`I` (page 100), `?`/`R` (reveal), `Z` (top, bottom, and
normal size), `P`/`N` (previous/next page), `A` (pause subpages), `S` (choose
subpage), `W` (Wi-Fi), `H` (help), and P2000 `STOP` (source selection). The host
keypad Enter key maps to `STOP`; `I` is the convenient host equivalent for
`START`.

The integration test uses a tiny generated monitor shim and a recorded NOS
response, so it is deterministic and does not need proprietary ROMs or network:

```sh
make -C emulator test
```

It executes the production cartridge, completes negotiated P2WP HELLO, fictitious Wi-Fi
scan/connect, source selection, live-code JSON decoding, four chunk transfers,
and asserts that built-in and custom page content reached emulated video RAM.
It also exercises the expanded shortcut/help screens and zoom mode.

For a deterministic custom-source boot from the repository root, run:

```sh
emulator/build/p2000t-emulator \
  --monitor /path/to/P2000ROM.bin --cartridge src/p2wp-cartridge.bin \
  --font emulator/assets/Default.fnt \
  --fixture emulator/tests/fixtures/nos-100.json \
  --headless --auto --auto-source 0 --custom-server http://terra:8080 \
  --frames 650 --dump-screen /tmp/custom-screen.bin
```

Fixture mode tests URL entry and P2WP/5 without network access. Add
`--flash /path/to/pico-flash.bin` to retain the last custom URL across emulator
runs; the file is rewritten only when the accepted URL changes. Replace the
`--fixture` pair with `--live`, change the custom URL to
`http://127.0.0.1:8080`, and run `python3 server/server.py` in another terminal
to contact the included example server. Add
`--auto-key Z`, `--auto-key R`, or `--auto-key H` to inject a page shortcut
after loading, which keeps new controls quick to test without a physical
keyboard.

The Pico side is a native simulation built around the production
`firmware/src/firmware_core.c` command processor. Consequently, negotiation,
payload validation, device/version replies, retry caching, sequence-conflict
handling, sensitive-payload erasure, and command dispatch are identical to the
real Pico firmware. The emulator supplies deterministic platform operations for
Wi-Fi, HTTP and storage; it does not simulate the RP2040/RP2350 processor,
CYW43 radio, GPIO electrical timing, or Pico SDK drivers.

Use `--p2wp-version 2` to emulate legacy firmware and exercise the cartridge's
compatibility warning, or `--p2wp-version 1` to exercise the no-common-version
error screen. Without this option the emulator negotiates the current P2WP/5.
The intermediate pre-release clock firmware can be reproduced with
`--p2wp-version 2 --p2wp-status-length 9`.

The vendored M2000 core retains its upstream copyright and is GPL-3.0; see
`LICENSE` and `vendor/m2000/UPSTREAM.md`.
