# P2000T Teletekst Cartridge

This repository contains the public hardware and client side of the P2000T
Teletekst project:

- `src/` is the 16 KiB slot-1 cartridge client.
- `firmware/` is the Raspberry Pi Pico W firmware for the slot-2 interface.
- `PROTOCOL.md` defines the P2WP/2 link protocol between them.
- `pcb/` contains the KiCad hardware design and manufacturing files.
- `enclosure/` contains the enclosure and label models.

The current release version of both the cartridge and Pico W firmware is
`v0.1.0`. The P2WP wire protocol remains version `2`.

## Compilation

### Cartridge

Build the cartridge with `make -C src`. 

### PICO Firmware

Build the firmware with `cmake -S firmware -B firmware/build -G Ninja` followed
by `cmake --build firmware/build`; this requires `PICO_SDK_PATH` to point to a
Raspberry Pi Pico SDK.
