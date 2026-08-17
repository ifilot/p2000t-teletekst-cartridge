# Standalone Pico W scan diagnostic

This deliberately excludes the P2000 mailbox, lwIP, and multicore code. Wi-Fi
scanning is performed directly by the CYW43 driver and does not require an IP
stack. It uses the repository's configured Pico SDK to initialize the CYW43 in
the Netherlands regulatory domain and perform one scan.

The onboard LED blinks slowly during scanning, remains on after one or more
results, and blinks rapidly after a failure or a zero-result scan. USB CDC
output reports the exact initialization and scan status every two seconds. The
diagnostic waits until a terminal has opened the USB CDC serial port before it
prints its startup messages. It then remains idle until a space character is
received over USB. CYW43 initialization and scanning only start after that
explicit command.

Build from this directory with:

```sh
cmake -S . -B build -G Ninja
cmake --build build
```
