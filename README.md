# P2000T Teletekst Cartridge

[![Latest release](https://img.shields.io/github/v/release/ifilot/p2000t-teletekst-cartridge?display_name=tag&sort=semver)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest)
[![Build and release](https://github.com/ifilot/p2000t-teletekst-cartridge/actions/workflows/build-and-release.yml/badge.svg?branch=master)](https://github.com/ifilot/p2000t-teletekst-cartridge/actions/workflows/build-and-release.yml)
[![License](https://img.shields.io/github/license/ifilot/p2000t-teletekst-cartridge)](LICENSE)

This repository contains the public hardware and client side of the P2000T
Teletekst project:

- `src/` is the 16 KiB slot-1 cartridge client.
- `firmware/` is the Raspberry Pi Pico W firmware for the slot-2 interface.
- `PROTOCOL.md` defines the P2WP/2 link protocol between them.
- `pcb/` contains the KiCad hardware design and manufacturing files.
- `enclosure/` contains the enclosure and label models.

The current release version of both the cartridge and Pico W firmware is
`v0.1.0`. The P2WP wire protocol remains version `2`.

## Downloads

Download the latest release artifacts:

- [P2000T cartridge ROM (`p2wp-cartridge.bin`)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest/download/p2wp-cartridge.bin)
- [Raspberry Pi Pico W firmware (`p2wp-pico-w.uf2`)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest/download/p2wp-pico-w.uf2)
- [Raspberry Pi Pico 2 W firmware (`p2wp-pico-2-w.uf2`)](https://github.com/ifilot/p2000t-teletekst-cartridge/releases/latest/download/p2wp-pico-2-w.uf2)

## Compilation

### Cartridge

Build the cartridge with `make -C src`. 

### PICO Firmware

Build the firmware with `cmake -S firmware -B firmware/build -G Ninja` followed
by `cmake --build firmware/build`; this requires `PICO_SDK_PATH` to point to a
Raspberry Pi Pico SDK.
