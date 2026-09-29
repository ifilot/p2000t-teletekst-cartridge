# P2000T slot-1 Teletekst cartridge

The `cartridge-c` tree builds the project's sole 16 KiB slot-1 ROM. It starts
at `0x1010`, communicates with the slot-2 Pico interface at ports `0x40`
through `0x42`, and writes directly to the P2000T video RAM at `0x5000`.

## Source layout for new contributors

Execution flows from the small runtime shim into `main.c`, then through the
following deliberately narrow modules:

| Source | Responsibility |
| --- | --- |
| `main.c` | Startup orchestration: opening screen, P2WP negotiation, Wi-Fi, and entry into the viewer. |
| `p2wp.c` / `p2wp.h` | Transport-independent P2WP framing, CRC, retries, HELLO negotiation, sequence numbers, and response validation. |
| `wifi.c` / `wifi.h` | Saved-profile recovery and the interactive scan, password, connection, and profile-save workflow. |
| `teletekst.c` / `teletekst.h` | Source/settings selection and the cohesive page-fetch/viewer state machine, including subpages, zoom, reveal, clock, and recovery. |
| `ui.c` / `ui.h` | Stateless shared panels, rules, logo, opening screen, and footer. |
| `platform.c` / `platform.h` | Portable key translation plus the public boundary for all P2000T-specific operations. |
| `platform.asm` | Size-critical monitor, video-RAM, clock, and cartridge-port implementations behind `platform.h`; it contains no application policy. |
| `crt0.asm` | Cartridge header, ROM/RAM map, C-runtime initialization, and terminal exit trap. |
| `lz4_z80.asm` / `lz4_z80.h` | Isolated raw-LZ4 decoder used for resident screen assets. |
| `make_cartridge_rom.py` | Joins Z88DK code and initialized data, validates the header, and pads the ROM. |
| `sign_cartridge.py` | Writes and verifies the monitor-compatible additive checksum. |
| `check_doxygen_style.py` | Enforces C, assembly, and Python documentation contracts used by `make lint`. |

The larger `teletekst.c` module intentionally keeps the page buffers and
viewer state machine together. Splitting individual viewer actions into more
translation units would expose mutable state, add cross-module interfaces, and
consume scarce ROM/RAM without creating an independent abstraction. New
provider or navigation policy belongs there; reusable presentation belongs in
`ui`, wire behaviour in `p2wp`, and hardware access in `platform`.

Public C contracts are documented once in their headers, including parameter
direction and return semantics. Private C functions are documented at their
definitions. Assembly contracts describe stack/register inputs, outputs, and
clobbers; the Python build tools use the same Doxygen tags in docstrings.

## Z88DK C cartridge

The application, protocol, Wi-Fi, and viewer state machines are implemented in
C. Three focused Z80 support modules provide CRT startup, monitor/port/video
primitives, and raw-LZ4 decoding. The build pads and signs the result and emits
`p2wp-cartridge.bin`. The reusable P2WP layer implements framing, CRC, retry,
sequence, protocol-error, and HELLO validation. `DEVICE_INFO` supplies the Pico
version shown on the source menu.

The fixed 40x24 video-memory operations and unsigned-byte decimal writer are
implemented as compact Z80 routines. The latter uses repeated subtraction;
the linked cartridge therefore contains no `sprintf`/`printf`, division,
multiplication, or arithmetic-error support from the general C runtime.

The current Wi-Fi vertical slice queries the encrypted-profile state and, when
a profile exists, starts it and polls through acquisition of an IP address. If
no usable profile exists, it starts an asynchronous scan, polls radio and scan
state, validates and displays up to nine SSIDs, accepts network selection and a
masked WPA/WPA2 password, connects, and optionally asks the Pico to encrypt and
save that profile. Password storage is wiped after use. Unsupported security,
short passwords, timeouts and connection failures produce explicit messages.

After Wi-Fi connects, the C image presents the restored Teletekst-style source
menu and can select NOS, P2000T, a custom server, or TeletekstArchief.nl.
Archive requires P2WP/7 so it always uses its dedicated verified transport.
The custom-server editor accepts 96 characters over three
rows; P2WP/5 restores and saves the URL in Pico flash, while P2WP/4 retains it
for the cartridge session. It starts an asynchronous request for page 100, validates
the version-dependent status response, retrieves all four 240-byte display
chunks, stages the complete 960-byte screen in RAM, and then displays it. Page
state now persists in a viewer loop: three digits select pages 100-899,
Backspace edits an unfinished number, `START`/`I` returns to page 100,
`P`/left and `N`/right follow previous/next metadata, and `STOP` returns to
source selection. Subpages advance automatically every ten seconds and wrap
to the default first subpage; `<`/`>` move to the previous/next subpage, `A`
pauses/resumes that sequence, and `S` selects a subpage manually. `R`/`?`
reveals concealed text, `Z` cycles normal and both
half-page zoom modes, `H` shows and dismisses the help page without refetching,
`V` enables automatic next-page navigation, and `W` returns to Wi-Fi setup.
Clock updates, the persistent source-menu autostart countdown, and detailed
error/recovery behaviour are included in the C image.

The Wi-Fi screen presents network results and password input. Protected
networks first ask whether entry should be visible
(`J`) or masked (`N`); the 63-character input spans two rows, supports
Backspace, preserves the original protocol bytes, and displays `#` using its
Viewdata glyph.

The reusable C UI layer reintroduces SAA5050 blue-background headers,
white-and-blue content panels, mosaic separator rules, the complete
fourteen-row joined P2000T/TELETEKST mosaic, and the cartridge footer. The
opening screen, source menu, and basic error recovery use this layer. Page
requests preserve the current display and animate a six-phase mosaic tile in
the upper-left corner until the complete replacement page has arrived.

Build the image using the pinned Z88DK 2.4 Docker image. The build uses the
SDCC frontend with `--opt-code-size -SO3` and
`--max-allocs-per-node10000`. The Z80 platform entry points follow SDCC's
packed-byte stack ABI:

```sh
make -C src
```

This produces `src/p2wp-cartridge.bin`. To test the runtime, keyboard shim,
viewer, and P2WP/2–7 negotiation in the emulator, run:

```sh
make -C src smoke
```

Every C ROM build reports code/read-only-data bytes, initialized-data bytes,
and the remaining padding capacity in the 16 KiB cartridge. The generated ROM
itself is always exactly 16,384 bytes after padding and signing.

The opening logo, help page, and error-description tables use a
cartridge-resident raw-LZ4 decoder. Page rendering, clock
arithmetic/formatting, fixed-width numeric formatting, atomic screen commits
and reveal updates use focused Z80 routines; the protocol and viewer state
machines remain in C for maintainability. `make -C src lint` verifies Google
C formatting, readable multi-line Doxygen block layout, and all documentation
headers.

The generated map, symbols and listing are placed in `src/build-c`. The ROM
builder appends Z88DK's initialized-data image after code and read-only data so
the CRT can copy it to RAM during startup. ROM code starts at `0x1000`,
execution starts at `0x1010`, mutable sections start at `0x7000`, and the stack
starts at `0x9ff0`. Keep platform-specific entry, monitor-call and port-I/O
details in the Z80 shim; application and protocol logic belongs in C
modules.

`p2wp-cartridge.bin` is a generated ROM image and is not stored in Git. Build it
with `make -C src`; this requires Docker and the pinned Z88DK image. The build
signs the image with the P2000T additive 16-bit cartridge checksum.
Use `make -C src verify` to validate it. This is an integrity checksum, not a
cryptographic signature.
The cartridge release version is `v0.5.0`. `../VERSION`, the cartridge
constants, and the Pico firmware constants are checked together during tests
so both release artifacts always identify the same version.

At runtime it negotiates P2WP/2 through P2WP/7 with `HELLO`, requests a wireless scan, and
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
continuing to Wi-Fi setup. Its first two rows extend the blue background, its
centered title is a full white-on-blue bar without a page number, and the bold `P2000T` mark is a
centered blue mosaic badge with a white border matching the Teletekst motif.
The P2000T contour and upper Teletekst frame share a mosaic row, making them a
single stacked logo. Its footer identifies cartridge version 0.5.0. The four
original slogan rows remain on the opening screen. After Wi-Fi connects, the
two spare rows below the source menu's function-key list show the cartridge and
Pico versions and latest published release. All cartridge UI text
is Dutch. The scan, network
list, connection state, prompts,
and errors reserve the first screen row, use full white-on-blue menu headers,
and retain blue separator/action bands and white content panels with blue text.
Network numbers use explicit white-on-blue tiles.
Authentication or association is attempted automatically up to three times.
If all three fail, `O` (opnieuw) starts another three-attempt batch with the
same session password and `W` wipes it and returns to password entry.

After a manual connection succeeds, the cartridge offers to remember or
replace one Wi-Fi profile with a single `J` (ja) confirmation. On later startups the
saved profile connects automatically without another password. If it is damaged
or cannot connect, the cartridge offers retry, new network, and delete choices.

While scanning, an acknowledged-poll counter and spinner advance only after a
complete, CRC-valid `WIFI_SCAN_STATUS` response of the expected type and
sequence has arrived. A second line shows how many networks the Pico has
reported so far, and a third distinguishes radio initialization from an active
scan or initialization failure. This separates a live P2000T-to-Pico link from
actual CYW43 progress.

Initial `HELLO` negotiation and every later local-link transaction have a
two-second overall timeout. A missing or unresponsive Pico W therefore shows a
clear error instead of leaving the cartridge waiting indefinitely. Each
transaction still makes up to three attempts within that deadline.
The v0.5 cartridge advertises P2WP/2–7 and selects the newest revision shared
with the Pico. P2WP/2 remains fully usable, but the cartridge displays a
one-time compatibility warning recommending a Pico firmware update. A HELLO
response with no common revision displays a dedicated protocol-incompatibility
screen instead of being reported as an Internet or server failure.

The cartridge then starts an asynchronous connection and polls until the Pico
has either acquired an IP address or reported a specific failure. Open networks
skip password entry. Once connected, a matching blue, white, and black screen
offers the NOS API, the P2000T Teletekst API at
`https://teletekst.philips-p2000t.nl`, TeletekstArchief.nl at
`https://teletekstarchief.nl`, or a custom HTTP(S) server.
The selection lasts for the current session and is attached to every request.
The custom URL may contain a DNS name, IPv4 address, port, and base path. With
P2WP/5 it is restored from Pico flash when the dialog opens and saved only if
the accepted value changed. P2WP/4 keeps it for the cartridge session. Custom
HTTPS accepts self-signed or private-CA certificates
by disabling certificate and hostname verification. Archive requires P2WP/7
and uses strict certificate and hostname checks. See
[`../docs/custom-server.md`](../docs/custom-server.md)
for the server-side contract and security warning.
The cartridge then requests page 100. Enter any page number from 100 through
899 as three digits using the number row or numeric keypad; the request starts
after the third digit, without Enter. Backspace corrects an unfinished entry.
If the selected API identifies another subpage, the cartridge retrieves it
automatically after ten seconds and continues following the subpage sequence.
After the last subpage reports no successor, an active loop requests subpage
zero and returns to the first subpage. Pausing suppresses this wrap as well as
ordinary advances; pressing `A` again resumes with a fresh ten-second interval.
A newly entered page always starts at its default first subpage.
Pressing the dedicated P2000T `STOP` key returns to the source-selection
screen without reconnecting Wi-Fi. After choosing a new source, the cartridge
requests the current page from its default subpage on that server.
Shift-`STOP` also cancels custom-server entry and Wi-Fi selection, returning to
the source menu without accepting partial input.
The source-selection screen also lists the controls available while viewing a
page: `START` or `I` jumps to page 100, `?` or `R` toggles concealed text, `Z` cycles
normal/top-half/bottom-half zoom, and arrow left/`P` or arrow right/`N` selects
the previous/next page advertised by the server. `V` toggles automatic
next-page mode: every ten seconds it follows subpages first, then advances to
the advertised next page (or page 100 when none exists). Invalid, oversized,
missing, or HTTP-error pages are skipped numerically; network failures retry
the same page. A failure on page 100 remains visible and stops auto-page mode.
`W` returns to Wi-Fi scanning, `A` pauses or resumes
automatic subpage cycling, `<`/`>` selects the previous/next subpage, and `S`
selects a subpage. Enter either two digits,
or one digit followed by Enter. Subpage `0`/`00` asks the API for its default
first subpage. A manual subpage choice pauses cycling so it remains visible
until `A` is pressed. While cycling is paused, an `A` appears in the top-right
corner; resuming restores the header cell that it covered.
On the source menu, `A` cycles the persistent 60-second auto-start setting
through off, NOS, P2000T, TeletekstArchief.nl, and the custom server. Archive
is included only under P2WP/7. Auto-start also enables automatic next-page mode.
Pressing `H` opens a cartridge-resident Teletekst-style Dutch help page. It
explains page entry, source and Wi-Fi selection, manual and automatic subpages,
the pause marker, and error recovery. Any key restores the exact prior display
without fetching it again.
If either API returns HTTP 404, the cartridge replaces the generic error view
with the blue-and-white P2000T masthead above a centered red panel containing
the missing page number and a prompt to type a new three-digit page number.
Other HTTP, network, and protocol failures retain their diagnostic error screen
and code. A missing-page clock is right-aligned so it does not overwrite the
error masthead or page-entry area.

The P2000T service may additionally return `binaryDisplay`, a base64 encoding
of the exact 960 SAA5050 display-memory bytes. Firmware prefers this field so
double height, flash, conceal, separated graphics and hold graphics are
preserved byte-for-byte. NOS responses and ordinary custom pages continue to
use the compatible `content` decoder as a fallback.

When page entry starts, the cartridge clears all four top-right navigation
cells before displaying the three new digits. This removes the previous page
number even when a provider aligns its header differently.

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

After Wi-Fi connects, the Pico obtains Dutch civil time from an NTP server.
The cartridge places an alpha-yellow control in column zero. NOS pages show
`ww DD.mmm HH:MM:SS` in columns one through eighteen of the first row. P2000T
pages use the shorter `ww DD.mmm HH:MM` in columns one through fifteen, with a
half-second blinking colon between hours and minutes. Dutch two-letter weekdays
and three-letter month names are used throughout. The display advances from the
monitor's 20 ms clock between network synchronizations, including the date and
weekday at midnight. It remains live during page-number entry, paused subpage
rotation, automatic page mode, error display, and both zoom views. This leaves
provider text on the right free.

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
