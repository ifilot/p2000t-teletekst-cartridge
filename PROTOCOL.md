# P2000T to Pico W Protocol (P2WP/2)

P2WP/2 is a reliable, framed request/response protocol carried by the three
I/O ports on the P2000T Pico W interface. Multi-byte fields are little-endian.

## Hardware transport

The P2000T is the host and the Pico W is the peripheral.

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

The host must never write while `TX_READY` is zero, because doing so overwrites
the byte latch.

### Pico to host byte transfer

1. The Pico waits until `RX_ACK_PENDING` is zero.
2. The Pico drives the byte on GPIO8 through GPIO15.
3. The Pico sets `RX_READY` high.
4. The host waits for `RX_READY`, then reads `HOST_RX` exactly once.
5. The hardware sets `RX_ACK_PENDING`.
6. The Pico first clears `RX_READY`, then drives `RX_ACK_CLR` low until
   `RX_ACK_PENDING` clears, and finally releases `RX_ACK_CLR` high.

Both endpoints must use a finite timeout for every wait. A transport timeout
aborts the partial frame and causes transaction recovery.

## Framing

`0x7E` is the frame delimiter. A frame consists of a delimiter, an escaped
body, and a closing delimiter. A closing delimiter may also serve as the
opening delimiter for the next frame. Repeated delimiters are idle fill.

Within the body, bytes `0x7E` and `0x7D` are encoded as `0x7D` followed by the
original byte XOR `0x20`. No other bytes are escaped.

The unescaped body is:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | Protocol version (`0x02`) |
| 1 | 1 | Flags |
| 2 | 1 | Message type |
| 3 | 1 | Sequence number |
| 4 | 2 | Payload length |
| 6 | N | Payload |
| 6+N | 2 | CRC-16, low byte first |

The initial maximum payload is 512 bytes. Implementations may advertise a
smaller limit during `HELLO`, but must reject a frame rather than truncate it.

### Flags

| Bit | Name | Meaning |
| ---: | --- | --- |
| 0 | `RESPONSE` | Frame is a response |
| 1 | `ERROR` | Response payload starts with an error code |
| 2 | `MORE` | Additional chunks belong to this transaction |
| 3-7 | reserved | Must be sent as zero and ignored on receipt |

### CRC

The CRC covers the unescaped bytes from version through the final payload
byte. It does not cover delimiters, escape bytes, or the transmitted CRC.

P2WP/2 uses CRC-16/CCITT-FALSE:

- polynomial: `0x1021`
- initial value: `0xFFFF`
- input and output are not reflected
- final XOR: `0x0000`

A receiver silently discards malformed, oversized, truncated, or CRC-invalid
frames. It may set its local error indication and statistics counter.

## Transactions and recovery

The P2000T is the only request initiator in P2WP/2. The Pico sends a frame only
in response to a valid request. This avoids unsolicited traffic and lets future
web events be retrieved with polling requests.

- A new request uses the next sequence number modulo 256.
- A response uses the request's type and sequence number and sets `RESPONSE`.
- An empty successful response is the acknowledgement for a command with no
  result data.
- On timeout, the host retries the identical request with the identical
  sequence number. Three attempts are recommended.
- The Pico caches the most recent request identity and complete response. An
  identical retry resends that response without repeating side effects.
- Reuse of the most recent sequence number for different request content is a
  protocol error.
- After all retries fail, the host abandons the partial frame and starts again
  with `HELLO`.
- A valid non-duplicate `HELLO` starts a session and invalidates older cached
  transaction state.

An inter-byte timeout resets only the frame parser. Transaction timeouts are
message-specific; the MWE uses a short local-link timeout, while future network
operations may use longer timeouts or chunked progress responses.

## Version 2 messages

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
| `0x31` | `TELETEKST_FETCH_STATUS` | Empty | Fetch state, error, byte count, and next subpage |
| `0x32` | `TELETEKST_FETCH_ROWS` | Chunk index | Six display-ready rows |

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

`HELLO` is independent of Wi-Fi state and must work while `WIFI_UP` is zero.

If the `ERROR` flag is set, the first payload byte is one of:

| Code | Meaning |
| ---: | --- |
| `0x01` | Unsupported version |
| `0x02` | Unknown message type |
| `0x03` | Invalid payload |
| `0x04` | Sequence conflict |
| `0x05` | Internal error |
| `0x06` | Wi-Fi hardware or driver unavailable |
| `0x07` | Wi-Fi operation already in progress |

## Wi-Fi provisioning

Scanning and connecting are asynchronous so the Pico can acknowledge every
link transaction quickly. The host starts an operation and polls its status;
it must not hold a P2WP transaction open for the duration of a radio scan,
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

The password must be empty for an open network and 8-63 bytes for a secured
network. `WIFI_STATUS` returns one state byte: `0` disconnected, `1`
connecting, `2` connected with an IP address, `3` SSID not found, `4`
authentication failed, or `5` connection failed/timed out.

## Encrypted Wi-Fi profile

The Pico may store one optional Wi-Fi profile. Profile operations are
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
P2000T Teletekst endpoint at `teletekst.philips-p2000t.nl` over verified
HTTPS. A
`TELETEKST_FETCH_START` request contains:

| Offset | Field |
| ---: | --- |
| 0-1 | Page number, little-endian (`100`-`899`) |
| 2 | Subpage (`0` selects the API's default first subpage, otherwise `1`-`99`) |
| 3 | Source: `0` NOS Teletekst, `1` P2000T Teletekst |

`TELETEKST_FETCH_STATUS` returns five bytes:

| Offset | Field |
| ---: | --- |
| 0 | State: `0` idle, `1` connecting/requesting, `2` receiving, `3` complete, `4` failed |
| 1 | Error: `0` none, `1` not connected, `2` TLS setup, `3` request start, `4` network, `5` HTTP status, `6` response too large, `7` invalid data |
| 2-3 | HTTP response bytes received so far, little-endian |
| 4 | Next subpage number, or zero when the API supplies none |

After state `3`, request chunk indexes `0` through `3` from
`TELETEKST_FETCH_ROWS`. Each successful response is exactly 240 bytes: six
consecutive 40-column rows ready to copy to the SAA5050-backed display. Together
the chunks provide the P2000T's 24 visible rows. Compatible source documents
may have 25 rows; row 25 contains Fastext prompts and is intentionally omitted.

The Pico translates HTML colour classes and Unicode private-use mosaic glyphs
into native SAA5050 bytes. Because foreground and graphics controls occupy a
visible column and take effect after that column, the encoder selects control
positions against the desired visual cells rather than inserting bytes and
shifting text. Background controls and inverse video are considered as
additional ways to reproduce the requested colours. A new successful fetch
replaces the cached screen and subpage metadata.
