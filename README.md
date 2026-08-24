# P2000T Teletekst Cartridge

[![Latest release](https://img.shields.io/github/v/release/ifilot/p2000t-teletekst-cartridge?display_name=tag&sort=semver)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest)
[![Build and release](https://github.com/ifilot/p2000t-teletekst-cartridge/actions/workflows/build-and-release.yml/badge.svg?branch=master)](https://github.com/ifilot/p2000t-teletekst-cartridge/actions/workflows/build-and-release.yml)
[![License](https://img.shields.io/github/license/ifilot/p2000t-teletekst-cartridge)](LICENSE)

<p align="center">
  <img src="docs/images/p2000t-teletekst.jpg" alt="Philips P2000T displaying NOS Teletekst through the cartridge" width="700">
</p>

## Introduction

The P2000T Teletekst Cartridge brings internet-connected teletext to the Philips
P2000T. A slot-1 ROM provides the native SAA5050 user interface, while a
Raspberry Pi Pico W or Pico 2 W interface in slot 2 manages Wi-Fi and fetches
pages from either the NOS service or the P2000T community service. Together,
they provide wireless network setup, optional encrypted credential storage, and
direct three-digit page selection on the original computer.

This repository contains the public hardware and client side of the P2000T
Teletekst project:

- `src/` is the 16 KiB slot-1 cartridge client.
- `firmware/` is the Raspberry Pi Pico W firmware for the slot-2 interface.
- [`docs/protocol.md`](docs/protocol.md) defines the P2WP/2 link protocol
  between them.
- `pcb/` contains the KiCad hardware design and manufacturing files.
- `enclosure/` contains the enclosure and label models.

## Downloads

Download the latest release artifacts:

- [P2000T cartridge ROM (`p2wp-cartridge.bin`)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest/download/p2wp-cartridge.bin)
- [Raspberry Pi Pico W firmware (`p2wp-pico-w.uf2`)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest/download/p2wp-pico-w.uf2)
- [Raspberry Pi Pico 2 W firmware (`p2wp-pico-2-w.uf2`)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest/download/p2wp-pico-2-w.uf2)

## Installing the firmware

Download all required binaries from the [Downloads](#downloads) section. Turn
the P2000T off before inserting or removing either cartridge.

### Slot-1 cartridge ROM

`p2wp-cartridge.bin` is a raw, signed 16 KiB ROM image for the slot-1
Teletekst cartridge. The programming procedure depends on the ROM, EEPROM, or
flash cartridge being used:

1. Select the exact memory device fitted to the cartridge in its programmer or
   flashing software.
2. Load `p2wp-cartridge.bin` as a raw binary. Do not byte-swap or add a file
   header.
3. Erase the device first if its technology requires it, then program and
   verify all 16,384 bytes.
4. Disconnect the programmer and insert the cartridge into slot 1 while the
   P2000T is powered off.

If the cartridge has an integrated loader rather than a removable memory chip,
follow that cartridge's instructions and use `p2wp-cartridge.bin` as its ROM
image. Do not guess a memory-device setting: an incorrect programming voltage
or pin configuration can damage the device.

### Pico W or Pico 2 W

> [!IMPORTANT]
> The Pico W and Pico 2 W use different firmware images, and the module marking
> may be hidden inside the cartridge. When the BOOTSEL drive opens in File
> Explorer, identify the Pico by its drive name: `RPI-RP2` means Pico W and
> `RP2350` means Pico 2 W. Do not copy a firmware file until the drive name has
> been checked.

| Installed module | BOOTSEL drive | Firmware file |
| --- | --- | --- |
| Raspberry Pi **Pico W** (RP2040) | `RPI-RP2` | `p2wp-pico-w.uf2` |
| Raspberry Pi **Pico 2 W** (RP2350) | `RP2350` | `p2wp-pico-2-w.uf2` |

1. Turn off the P2000T. Leave it off throughout the update.
2. Connect a data-capable USB cable to the Pico while holding its **BOOTSEL**
   button, then release the button when the USB drive appears.
3. In File Explorer (or the equivalent file manager), check the BOOTSEL drive
   name against the table above to determine which Pico variant is installed.
4. Copy the matching `.uf2` file to that drive. The drive automatically ejects
   and the Pico reboots when programming is complete.
5. Unplug the USB cable, install the slot-2 cartridge if it was removed, and
   then power on the P2000T.

BOOTSEL is stored in the Pico's read-only boot ROM, so this update method
remains available even if an earlier firmware image does not start. See the
official [Raspberry Pi drag-and-drop instructions](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html#your-first-binaries)
for more detail.

#### Pico W versus Pico 2 W performance

Both modules provide the same cartridge features and should feel the same in
normal use. Page loading is dominated by the shared CYW43439 wireless subsystem,
Internet and server latency, and the same P2WP/2 cartridge transport, so the
Pico 2 W's faster processor is not expected to provide a noticeable benefit
here.

## Hardware

<p align="center">
  <img src="docs/images/p2kpico.jpg" alt="P2000T Teletekst cartridge with Raspberry Pi Pico 2 W" width="450">
</p>

### Circuit schematic

<p align="center">
  <a href="pcb/p2000t-pico-web-interface.svg">
    <img src="pcb/p2000t-pico-web-interface.svg" alt="P2000T Teletekst cartridge circuit schematic" width="100%">
  </a>
</p>

## Documentation

[`docs/protocol.md`](docs/protocol.md) is the canonical P2WP/2 interface
specification. The Sphinx documentation adds implementation guides for
[P2000T BASIC](docs/basic.rst) and
[Z80 assembly](docs/assembly.rst).

Pushes to `master` publish the rendered documentation to
[GitHub Pages](https://ifilot.github.io/p2000t-teletekst-cartridge/). Manual
deployment is also available from the GitHub Actions page. Before the first
deployment, select **GitHub Actions** as the publishing source under
**Settings → Pages**.

Build the HTML documentation with:

```sh
python3 -m pip install -r docs/requirements.txt
make -C docs html
```

## Compilation

### Cartridge

Build the cartridge with `make -C src`. 

### PICO Firmware

Build the firmware with `cmake -S firmware -B firmware/build -G Ninja` followed
by `cmake --build firmware/build`; this requires `PICO_SDK_PATH` to point to a
Raspberry Pi Pico SDK.

The host-side replay tool fetches a live NOS response and passes it through the
same size limit and JSON-to-SAA5050 decoder as the Pico firmware:

```sh
firmware/tools/replay-nos-page 101
firmware/tools/replay-nos-page 200-2
```

For a saved response, build with `make -C firmware/tools` and run
`firmware/tools/teletekst-replay PAGE response.json [screen.bin]`. Failures are
reported as the Pico's `06`/`07` code plus the rejected decoder stage or row,
which makes captured API responses suitable as regression fixtures.
