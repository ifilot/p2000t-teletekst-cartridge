# P2000T slot-1 Teletekst cartridge

`p2wp-cartridge.asm` is a standalone 16 KiB slot-1 ROM program. It starts at
`0x1010`, communicates with the slot-2 Pico interface at ports `0x40` through
`0x42`, and writes progress directly to the P2000T video RAM at `0x5000`.

The checked-in `p2wp-cartridge.bin` is the ROM image to program. Rebuild it with
`make -C src`; this requires `z80asm` 1.8 or a compatible assembler.
The build signs the image with the P2000T additive 16-bit cartridge checksum.
Use `make -C src verify` to validate it. This is an integrity checksum, not a
cryptographic signature.
The cartridge release version is `v0.2.0`.

At runtime it negotiates P2WP/2 with `HELLO`, requests a wireless scan, and
polls while the Pico W performs it asynchronously. Up to nine unique SSIDs are
shown strongest first using one to four signal bars. `*` marks supported
WPA/WPA2 networks and `!` marks security modes that this first client cannot
join. Select a network with keys 1-9 and, when required, enter an 8-63 character
password. Before entry, choose whether its characters should be visible or
masked; long passwords may continue onto a second row. Password bytes retain
their case. For conventional password entry, unshifted letters are lower-case
and Shift produces upper-case letters.

Startup presents a native SAA5050 `P2000T TELETEKST` composition inspired by
the blue, white, and black NOS page 100 masthead, and waits for any key before
continuing to Wi-Fi setup. Its first row is blank, its title is a full
white-on-blue bar without a page number, and its footer identifies cartridge
version 2.0 and P2WP/2. The scan, network list, connection state, prompts,
and errors reserve the first screen row, use full white-on-blue menu headers,
and retain blue separator/action bands and white content panels with blue text.
Network numbers use explicit white-on-blue tiles.
Authentication or association is attempted automatically up to three times.
If all three fail, `R` starts another three-attempt batch with the same session
password and `P` wipes it and returns to password entry.

After a manual connection succeeds, the cartridge offers to remember or
replace one Wi-Fi profile with a single `Y` confirmation. On later startups the
saved profile connects automatically without another password. If it is damaged
or cannot connect, the cartridge offers retry, new network, and delete choices.

While scanning, an acknowledged-poll counter and spinner advance only after a
complete, CRC-valid `WIFI_SCAN_STATUS` response of the expected type and
sequence has arrived. A second line shows how many networks the Pico has
reported so far, and a third distinguishes radio initialization from an active
scan or initialization failure. This separates a live P2000T-to-Pico link from
actual CYW43 progress.

Initial `HELLO` negotiation waits continuously so the P2000T and Pico W may be
reset or reflashed independently. The Pico firmware services this local
mailbox on core 1 while core 0 initializes the radio. Once the session is
established, ordinary commands retain finite three-attempt failure handling.

The cartridge then starts an asynchronous connection and polls until the Pico
has either acquired an IP address or reported a specific failure. Open networks
skip password entry. Once connected, a matching blue, white, and black screen
offers the NOS API or the P2000T Teletekst API at
`https://teletekst.philips-p2000t.nl`.
The selection lasts for the current session and is attached to every request.
The cartridge then requests page 100. Enter any page number from 100 through
899 as three digits; the request starts after the third digit, without Enter.
If the selected API identifies another subpage, the cartridge retrieves it
automatically after ten seconds and continues following the subpage sequence.
A newly entered page always starts at its default first subpage.
Pressing the dedicated P2000T `STOP` key returns to the source-selection
screen without reconnecting Wi-Fi. After choosing a new source, the cartridge
requests the current page from its default subpage on that server.
If either API returns HTTP 404, the cartridge replaces the generic error view
with the blue-and-white P2000T masthead above a centered red panel containing
the missing page number and a prompt to type a new three-digit page number.
Other HTTP, network, and protocol failures retain their diagnostic error screen
and code.

The P2000T service may additionally return `binaryDisplay`, a base64 encoding
of the exact 960 SAA5050 display-memory bytes. Firmware prefers this field so
double height, flash, conceal, separated graphics and hold graphics are
preserved byte-for-byte. NOS responses and ordinary custom pages continue to
use the compatible `content` decoder as a fallback.

Fetching does not replace the screen with a loading page. A six-phase SAA5050
mosaic spinner rotates one block around a 2-by-3 graphics cell in the upper-left
corner while the previous page remains visible. The required graphics control
occupies column zero, so the mosaic itself uses column one, the leftmost place a
native mosaic can appear. Incoming chunks are staged in
RAM, and the display changes only after the complete 960-byte page has
validated. A failed fetch restores the four cells covered by the indicator. To
avoid tearing, the final commit
waits for vertical retrace, disables video through port `0x30`, copies the
staged page, and re-enables video at the following retrace. This produces one
clean blank field instead of exposing a partially updated screen.

ASCII and P2000T display bytes are kept separate. A `#` remains ASCII `0x23`
inside passwords and protocol payloads, but is translated to Viewdata screen
code `0x5F` wherever cartridge text is displayed. This applies to signal bars,
visible password entry, SSIDs, and the echo console.

Pages arrive as four validated 240-byte chunks. Each contains six packed
40-column rows, which the cartridge copies into the visible half of each
80-byte P2000T video-RAM row. The bytes already contain the SAA5050 spacing
attributes needed for the NOS foreground and background colours and native
mosaic graphics. The API's 25th Fastext row is not shown.

Without an explicitly saved profile, Wi-Fi credentials remain session-only.
The cartridge keeps the password in RAM only while retry or profile creation
remains an option, then wipes it. A saved profile is authenticated and encrypted
in Pico flash; its plaintext credentials never return to the cartridge during
automatic connection. Temporary decrypted Pico buffers are wiped after use.

The cartridge consumes keycodes through the monitor ROM's blocking `readkey`
entry at `0x0026`. The monitor's 20 ms interrupt routine therefore remains in
charge of scanning, debouncing, repeat handling, Shift/Shift-Lock state, and
the 12-byte keyboard FIFO. Cartridge buffers live at `0x7000` and above, away
from the monitor workspace at `0x6000`. For conventional case-sensitive
password entry, unshifted letter keys produce lower-case text and Shift produces
upper-case text.
