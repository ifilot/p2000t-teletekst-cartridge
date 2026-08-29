# P2000T to Pico W Protocol (P2WP/2–3)

P2WP is a reliable, version-negotiated request/response protocol carried by the three
I/O ports on the P2000T Pico W interface. Multi-byte fields are little-endian.

## Status and conformance

This document is the normative specification for protocol versions 2 and 3. The key
words **MUST**, **MUST NOT**, **REQUIRED**, **SHOULD**, **SHOULD NOT**, and
**MAY** describe conformance requirements.

A conforming host implementation MUST implement the hardware handshake,
framing, escaping, CRC validation, sequence matching, finite timeouts, and
`HELLO`. A conforming peripheral MUST implement the same link layer, respond to
`HELLO`, reject invalid requests with a protocol error when possible, and MUST
NOT send an unsolicited frame. Features advertised in the `HELLO` capability
byte are optional, but an endpoint MUST implement every feature that it
advertises.

The following terms are used throughout this specification:

- **Host**: the P2000T and the only endpoint permitted to initiate requests.
- **Peripheral**: the Pico W or Pico 2 W interface.
- **Body**: the unescaped header, payload, and CRC between frame delimiters.
- **Transaction**: one request and its matching response, including retries.
- **Session**: the sequence of transactions established by a valid `HELLO`.

All quantities are unsigned unless explicitly described as signed. All
multi-byte integers use little-endian byte order.

(protocol-overview)=
## Protocol overview

After either endpoint starts or resets, the host establishes a session with
`HELLO`. It then sends one request at a time and waits for the matching
response. Long-running Wi-Fi and Internet operations are started by one
transaction and observed through separate status transactions. This keeps the
local link responsive and bounds every request/response exchange.

(hardware-transport)=
## Hardware transport

The P2000T is the host and the Pico W is the peripheral.

For a circuit-level explanation of the address decoder, data buffers, and
handshake flip-flops, see {doc}`hardware`.

```{figure} images/hardware-transport.svg
:alt: Block diagram of the bidirectional byte lanes and handshake signals between the P2000T, cartridge logic, and Pico W
:width: 100%

The cartridge provides one eight-bit data lane in each direction. Handshake
signals ensure that neither endpoint overwrites an unread byte.
```

| Port | P2000T operation | Name | Purpose |
| --- | --- | --- | --- |
| `0x40` | write | `HOST_TX` | Send one byte to the Pico |
| `0x41` | read | `HOST_RX` | Receive one byte from the Pico |
| `0x42` | read | `STATUS` | Read transport and application status |

The status register is defined as follows:

| Bit | Schematic signal | Meaning |
| ---: | --- | --- |
| 0 | `RX_READY` | A Pico-to-host byte is ready |
| 1 | `TX_READY` | The host may write `HOST_TX` |
| 2 | `WIFI_UP` | Informational: the network is available |
| 3 | `BUSY` | Informational: the Pico is processing a request |
| 4 | `ERROR` | Informational: the Pico detected an error |
| 5 | `RX_ACK_PENDING` | Diagnostic: the host read `HOST_RX` |
| 6 | `TX_FULL` | Diagnostic: a host-to-Pico byte is pending |
| 7 | reserved | Reads zero |

`TX_READY` and `TX_FULL` are complementary. Only `RX_READY` and `TX_READY`
are used by the P2000T transport implementation; the remaining bits are
advisory or diagnostic.

### Host to Pico byte transfer

1. The host waits until `TX_READY` is one.
2. The host writes exactly one byte to `HOST_TX`.
3. The hardware sets `TX_FULL` and clears `TX_READY`.
4. The Pico samples GPIO0 through GPIO7.
5. The Pico drives `TX_CLR` low until `TX_FULL` clears, then releases it high.

The host MUST NOT write while `TX_READY` is zero, because doing so overwrites
the byte latch.

### Pico to host byte transfer

1. The Pico waits until `RX_ACK_PENDING` is zero.
2. The Pico drives the byte on GPIO8 through GPIO15.
3. The Pico sets `RX_READY` high.
4. The host waits for `RX_READY`, then reads `HOST_RX` exactly once.
5. The hardware sets `RX_ACK_PENDING`.
6. The Pico first clears `RX_READY`, then drives `RX_ACK_CLR` low until
   `RX_ACK_PENDING` clears, and finally releases `RX_ACK_CLR` high.

Both endpoints MUST use a finite timeout for every wait. A transport timeout
aborts the partial frame and causes transaction recovery.

## Framing

`0x7E` is the frame delimiter. A frame consists of a delimiter, an escaped
body, and a closing delimiter. A closing delimiter MAY also serve as the
opening delimiter for the next frame. Repeated delimiters are idle fill.

Within the body, bytes `0x7E` and `0x7D` are encoded as `0x7D` followed by the
original byte XOR `0x20`. No other bytes are escaped.

The unescaped body is:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | Negotiated protocol version (`0x02` or `0x03`) |
| 1 | 1 | Flags |
| 2 | 1 | Message type |
| 3 | 1 | Sequence number |
| 4 | 2 | Payload length |
| 6 | N | Payload |
| 6+N | 2 | CRC-16, low byte first |

The protocol maximum payload is 512 bytes. Implementations MAY advertise a
smaller receive limit during `HELLO`. An endpoint MUST NOT send a payload larger
than the negotiated limit; a receiver MUST reject an oversized frame rather
than truncate it.

The Teletekst application profile uses fixed 240-byte row responses. A host
that intends to advertise or use the Internet-fetch capability MUST therefore
advertise a receive limit of at least 240 bytes.

### Flags

| Bit | Name | Meaning |
| ---: | --- | --- |
| 0 | `RESPONSE` | Frame is a response |
| 1 | `ERROR` | Response payload starts with an error code |
| 2 | `MORE` | Additional chunks belong to this transaction |
| 3-7 | reserved | Sent as zero and ignored on receipt |

A request MUST clear `RESPONSE` and `ERROR`. A successful response MUST set
`RESPONSE` and clear `ERROR`. An error response MUST set both bits and contain
at least the one-byte error code. Reserved bits MUST be sent as zero and MUST
be ignored on receipt.

### CRC

The CRC covers the unescaped bytes from version through the final payload
byte. It does not cover delimiters, escape bytes, or the transmitted CRC.

P2WP uses CRC-16/CCITT-FALSE:

- polynomial: `0x1021`
- initial value: `0xFFFF`
- input and output are not reflected
- final XOR: `0x0000`

The standard ASCII check value is `CRC("123456789") = 0x29B1`. A version-2
`HELLO` request with sequence zero and a 240-byte host limit has the following
wire representation. Its unescaped body CRC is `0xD9B2`, transmitted low byte
first. None of the body bytes require escaping in this example.

```text
7e 02 00 01 00 08 00 50 32 57 50 02 02 f0 00 b2 d9 7e
```

A receiver MUST silently discard malformed, oversized, truncated, or
CRC-invalid frames because their header cannot be trusted. It MAY set its local
error indication and statistics counter.

## Transactions and recovery

The P2000T is the only request initiator in P2WP. The Pico MUST send a frame
only in response to a valid request. This avoids unsolicited traffic and lets
future web events be retrieved with polling requests.

- The host MUST have no more than one transaction in flight.
- A new request uses the next sequence number modulo 256.
- A response MUST use the request's type and sequence number and set `RESPONSE`.
- An empty successful response is the acknowledgement for a command with no
  result data.
- On timeout, the host retries the identical request with the identical
  sequence number. Three attempts are recommended.
- The peripheral MUST cache the most recent successfully processed request
  identity and complete response. An identical retry resends that response
  without repeating side effects. Error responses for rejected requests MAY be
  sent without caching because they perform no side effects.
- Reuse of the most recent sequence number for different request content MUST
  produce `SEQUENCE_CONFLICT`.
- After all retries fail, the host abandons the partial frame and starts again
  with `HELLO`.
- A valid non-duplicate `HELLO` starts a session and invalidates older cached
  transaction state.

The host accepts a response only when its protocol version, type, sequence,
payload length, flags, and CRC are valid for the outstanding request. It MUST
discard all other frames and retry or re-establish the session. The host MUST
increment its sequence number only after accepting a complete response.

An inter-byte timeout resets only the frame parser. Transaction timeouts are
message-specific; the reference implementation uses a short local-link
timeout, while network operations MAY use longer timeouts or chunked progress
responses.

## Versioned messages

| Type | Name | Request payload | Successful response payload |
| ---: | --- | --- | --- |
| `0x01` | `HELLO` | Hello request below | Hello response below |
| `0x02` | `ECHO` | Arbitrary bytes | Identical bytes |
| `0x03` | `LINK_STATS` | Empty | Implementation counters; format reserved |
| `0x10` | `WIFI_SCAN_START` | Empty | Empty acknowledgement |
| `0x11` | `WIFI_SCAN_STATUS` | Empty | Scan state, result count, and radio state |
| `0x12` | `WIFI_SCAN_RESULT` | One-byte result index | Indexed network record |
| `0x13` | `WIFI_CONNECT` | Network index and password | Empty acknowledgement |
| `0x14` | `WIFI_STATUS` | Empty | Connection state |
| `0x20` | `WIFI_PROFILE_STATUS` | Empty | Profile state and error |
| `0x21` | `WIFI_PROFILE_CONNECT` | Empty | Empty acknowledgement |
| `0x22` | `WIFI_PROFILE_SAVE` | Wi-Fi password | Empty acknowledgement |
| `0x23` | `WIFI_PROFILE_DELETE` | Empty | Empty acknowledgement |
| `0x30` | `TELETEKST_FETCH_START` | Page, subpage, and source | Empty acknowledgement |
| `0x31` | `TELETEKST_FETCH_STATUS` | Empty | Fetch state, error, byte count, next subpage, and clock |
| `0x32` | `TELETEKST_FETCH_ROWS` | Chunk index | Six display-ready rows |

`LINK_STATS` reserves its message number for a future statistics format. A
host MUST NOT depend on this message until a payload format is specified. A
version-2 peripheral MAY respond with `UNKNOWN_TYPE`.

The eight-byte `HELLO` request is:

| Offset | Field |
| ---: | --- |
| 0-3 | ASCII `P2WP` |
| 4 | Minimum supported version |
| 5 | Maximum supported version |
| 6-7 | Host maximum payload |

The eight-byte `HELLO` response is:

| Offset | Field |
| ---: | --- |
| 0-3 | ASCII `P2WP` |
| 4 | Selected version |
| 5 | Capability bits (`bit 0`: `ECHO`, `bit 1`: Wi-Fi provisioning, `bit 2`: Internet fetch, `bit 3`: encrypted Wi-Fi profile) |
| 6-7 | Negotiated maximum payload |

The capability byte is defined as follows:

| Bit | Name | Meaning |
| ---: | --- | --- |
| 0 | `ECHO` | `ECHO` request support |
| 1 | `WIFI` | Wi-Fi scan, connect, and status support |
| 2 | `INTERNET` | Teletekst Internet-fetch support |
| 3 | `WIFI_PROFILE` | Encrypted Wi-Fi profile support |
| 4-7 | reserved | Sent as zero and ignored on receipt |

The selected version MUST fall within the host's advertised range. The
negotiated maximum payload is the smaller of the host and peripheral limits and
MUST be non-zero. Reserved capability bits MUST be sent as zero and ignored on
receipt.

`HELLO` request and response frame headers use bootstrap version `2`. After a
successful response, both endpoints MUST use the selected version in every
subsequent frame header. A peripheral selects the newest revision in the
intersection of its supported range and the host's advertised range. If that
intersection is empty, it returns `UNSUPPORTED_VERSION` in a bootstrap-version
error response. Version 3 peripherals MUST retain version 2 operation; this
allows both a new cartridge with old Pico firmware and an old cartridge with
new Pico firmware to remain usable.

`HELLO` is independent of Wi-Fi state and MUST work while `WIFI_UP` is zero.

If the `ERROR` flag is set, the first payload byte is one of:

| Code | Name | Meaning |
| ---: | --- | --- |
| `0x01` | `UNSUPPORTED_VERSION` | Unsupported version |
| `0x02` | `UNKNOWN_TYPE` | Unknown message type |
| `0x03` | `INVALID_PAYLOAD` | Invalid payload |
| `0x04` | `SEQUENCE_CONFLICT` | Sequence conflict |
| `0x05` | `INTERNAL` | Internal error |
| `0x06` | `WIFI_UNAVAILABLE` | Wi-Fi hardware or driver unavailable |
| `0x07` | `WIFI_BUSY` | Wi-Fi operation already in progress |

Unknown error codes MUST be treated as an unspecified transaction failure.

## Wi-Fi provisioning

Scanning and connecting are asynchronous so the Pico can acknowledge every
link transaction quickly. The host starts an operation and polls its status;
it MUST NOT hold a P2WP transaction open for the duration of a radio scan,
authentication exchange, or DHCP request.

`WIFI_SCAN_STATUS` returns three bytes:

| Offset | Field |
| ---: | --- |
| 0 | State: `0` idle, `1` scanning, `2` complete, `3` failed |
| 1 | Number of results reported so far (final when complete) |
| 2 | Radio state: `0` initializing, `1` ready, `2` initialization failed |

Radio initialization and scanning have finite implementation deadlines. A
queued scan may report state `1` with radio state `0` while CYW43 startup is
still in progress; it changes to scan state `3` if startup or scanning exceeds
its deadline.

Results are de-duplicated by SSID, retaining the strongest access point, and
ordered by decreasing RSSI. A `WIFI_SCAN_RESULT` response is:

| Offset | Field |
| ---: | --- |
| 0 | Result index from the request |
| 1 | RSSI in dBm as a signed 8-bit integer |
| 2 | Security: `0` open, `1` supported WPA/WPA2 PSK, `2` unsupported |
| 3 | SSID length, 1-32 |
| 4 | SSID bytes, not zero-terminated |

A `WIFI_CONNECT` request contains:

| Offset | Field |
| ---: | --- |
| 0 | Index from the most recently completed scan |
| 1 | Password length, 0-63 |
| 2 | Password bytes, not zero-terminated |

The password MUST be empty for an open network and 8-63 bytes for a secured
network. `WIFI_STATUS` returns one state byte: `0` disconnected, `1`
connecting, `2` connected with an IP address, `3` SSID not found, `4`
authentication failed, or `5` connection failed/timed out.

## Encrypted Wi-Fi profile

The Pico MAY store one optional Wi-Fi profile. Profile operations are
asynchronous because authenticated decryption and flash updates can take longer
than one link transaction. The host starts an operation and polls
`WIFI_PROFILE_STATUS`, which returns:

| Offset | Field |
| ---: | --- |
| 0 | State: `0` absent, `1` encrypted profile ready, `2` operation busy |
| 1 | Last result: `0` none, `1` not found, `2` corrupt record, `3` flash failure, `4` invalid data |

`WIFI_PROFILE_CONNECT` has an empty payload. A successful asynchronous load
queues a connection using the saved SSID, password, and authentication mode.
The credentials are not returned to the host; the host continues by polling
`WIFI_STATUS`.

`WIFI_PROFILE_SAVE` is accepted only after a successful manually provisioned
connection. It contains:

| Offset | Field |
| ---: | --- |
| 0 | Wi-Fi password length, 0-63 |
| 1 | Wi-Fi password bytes |

The Pico takes the SSID and authentication mode from the current connection.
Saving replaces the previous profile. `WIFI_PROFILE_DELETE` forgets the record
by erasing its dedicated flash sector.

The reference firmware stores a fixed-length AES-256-GCM ciphertext and tag in
the final flash sector. Its key is derived from a domain-separated hash of the
Pico's unique board identifier, and every save receives a fresh random 96-bit
GCM nonce. This binds the record to one board, detects alteration, and prevents
credentials appearing as plaintext in flash. The RP2040 identifier and firmware
are not protected secrets, however, so this is not a defence against an attacker
who can dump and analyse the complete device flash.

## Teletekst fetch

Teletekst retrieval is asynchronous. Source `0` uses the public JSON endpoint
at `teletekst-data.nos.nl` over verified HTTPS; source `1` uses the compatible
P2000T Teletekst endpoint at `teletekst.philips-p2000t.nl` over verified HTTPS.
A `TELETEKST_FETCH_START` request contains:

| Offset | Field |
| ---: | --- |
| 0-1 | Page number, little-endian (`100`-`899`) |
| 2 | Subpage (`0` selects the API's default first subpage, otherwise `1`-`99`) |
| 3 | Source: `0` NOS Teletekst, `1` P2000T Teletekst |

In P2WP/2, `TELETEKST_FETCH_STATUS` returns the original five-byte payload:

| Offset | Field |
| ---: | --- |
| 0 | State: `0` idle, `1` connecting/requesting, `2` receiving, `3` complete, `4` failed |
| 1 | Error: `0` none, `1` not connected, `2` TLS setup, `3` request start, `4` network, `5` HTTP status, `6` response too large, `7` invalid data, `8` page not found |
| 2-3 | HTTP response bytes received so far, little-endian |
| 4 | Next subpage number, or zero when the API supplies none |

P2WP/3 extends that payload to thirteen bytes:

| Offset | Field |
| ---: | --- |
| 0 | State: `0` idle, `1` connecting/requesting, `2` receiving, `3` complete, `4` failed |
| 1 | Error: `0` none, `1` not connected, `2` TLS setup, `3` request start, `4` network, `5` HTTP status, `6` response too large, `7` invalid data, `8` page not found |
| 2-3 | HTTP response bytes received so far, little-endian |
| 4 | Next subpage number, or zero when the API supplies none |
| 5 | Dutch local hour (`0`-`23`) from NTP |
| 6 | Dutch local minute (`0`-`59`) from NTP |
| 7 | Dutch local second (`0`-`59`) from NTP |
| 8 | Clock validity (`1` after NTP synchronization, otherwise `0`) |
| 9 | Dutch local day of month (`1`-`31`) |
| 10 | Dutch local month (`1`-`12`) |
| 11 | Dutch local year minus 2000 |
| 12 | Dutch local weekday (`0` Sunday through `6` Saturday) |

After state `3`, request chunk indexes `0` through `3` from
`TELETEKST_FETCH_ROWS`. Each successful response is exactly 240 bytes: six
consecutive 40-column rows ready to copy to the SAA5050-backed display. Together
the chunks provide the P2000T's 24 visible rows. Compatible source documents
MAY have 25 rows; row 25 contains Fastext prompts and is intentionally omitted.

The clock and calendar fields are a P2WP/3 addition. When the clock-valid byte
is set, the cartridge places an alpha-yellow control
in column zero. NOS pages show `ww DD.mmm HH:MM:SS` in yellow in columns one
through eighteen. P2000T pages show the shorter `ww DD.mmm HH:MM` in columns
one through fifteen, with the time separator blinking every half second. Both
use Dutch two-letter weekdays and three-letter month names. The cartridge
advances the time, date, weekday, and blink using the P2000T's 20 ms monitor
clock between fetches, leaving provider text on the right untouched.

The Pico translates HTML colour classes and Unicode private-use mosaic glyphs
into native SAA5050 bytes. Because foreground and graphics controls occupy a
visible column and take effect after that column, the encoder selects control
positions against the desired visual cells rather than inserting bytes and
shifting text. Background controls and inverse video are considered as
additional ways to reproduce the requested colours. A new successful fetch
replaces the cached screen and subpage metadata.

## Implementation requirements

Before an implementation is considered conforming, verify that it:

- uses only `RX_READY` and `TX_READY` for the mandatory byte handshake;
- applies a finite timeout to every byte and transaction wait;
- escapes only `0x7D` and `0x7E`, and calculates CRC over unescaped bytes;
- validates the complete frame before acting on its payload;
- permits only one outstanding host request;
- retries with byte-for-byte identical request content and sequence numbers;
- increments the sequence only after accepting a matching response;
- starts or re-establishes every session with `HELLO`;
- respects the negotiated payload limit and advertised capability bits;
- treats all Wi-Fi and Internet operations as asynchronous; and
- does not expose saved credentials through the host protocol.

## Client examples

- {doc}`basic` implements a portable link diagnostic in P2000T BASIC.
- {doc}`assembly` explains the production Z80 implementation and reusable
  transport routines.
