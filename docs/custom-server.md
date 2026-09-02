# Hosting a custom Teletekst server

Choose **0 - EIGEN SERVER** and enter the server's complete base URL, for
example `http://terra:8080`, `http://192.168.1.20:8080`, or
`https://example.test/teletekst`. The maximum is 96 characters. HTTP and HTTPS,
DNS names, IPv4 addresses, ports, and a base path are supported. The cartridge
keeps the address only for the current session.

For immediate testing, [`server/server.py`](../server/server.py) implements
this contract using only Python's standard library. Its page files are designed
to be edited directly; page 101 demonstrates concealed text using the `0x18`
SAA5050 control code. See `server/README.md` in the repository.

## Minimum HTTP contract

The Pico makes a `GET` request by appending one of these paths to the base URL:

- `/json/100` for the default subpage of page 100;
- `/json/100-2` for page 100, subpage 2.

Return HTTP `200` with a JSON body of no more than 16 KiB. Using
`Content-Type: application/json` is recommended, although the current client
does not inspect that header. The smallest practical response has these fields:

```json
{
  "nextSubPage": "",
  "binaryDisplay": "BASE64_OF_EXACTLY_960_BYTES"
}
```

`binaryDisplay` is the recommended format: base64-encode exactly 960 SAA5050
display bytes, arranged as 24 rows of 40 bytes. Those bytes are copied directly
to the visible screen, so they may contain the normal SAA5050 colour, graphics,
double-height, flash, conceal, and hold-graphics control codes.
Replace the uppercase placeholder above with that base64 string.

When the clock has synchronized, the cartridge replaces byte indexes `0`–`18`
of the first row with its yellow `ww DD.mmm HH:MM:SS` clock. Leave those 19
cells blank in a custom page header; columns 20–40 remain available for a title
and three-digit page number. The bundled Python pages use only the final three
cells for the page number, avoiding any overlap with the clock.

`nextSubPage` is required. Use an empty string to disable automatic rotation,
or a value such as `"100-2"`; its page number must match the requested page.
The optional `prevPage` and `nextPage` strings enable the `P` and `N` keys. Each
is either empty or a three-digit page from `100` through `899`:

```json
{
  "prevPage": "100",
  "nextPage": "102",
  "nextSubPage": "101-2",
  "binaryDisplay": "BASE64_OF_EXACTLY_960_BYTES"
}
```

As an alternative to `binaryDisplay`, a server may return the NOS-compatible
HTML-like `content` string. It must describe exactly 25 rows of 40 cells; the
25th Fastext row is accepted but not displayed. Because the binary format is
smaller to specify and preserves every SAA5050 attribute exactly, it is the
simplest target for a new server.

Return HTTP `404` for a page that does not exist. Other HTTP status codes are
shown as a server error.

## HTTPS security

Custom HTTPS endpoints deliberately skip certificate-chain and hostname
verification. This permits self-signed certificates and private certificate
authorities without provisioning CA certificates on the Pico. It also means a
custom connection is vulnerable to interception: use it only on a network and
with a server you trust. Certificate verification remains enabled for the two
built-in services.
