# Lightweight example server

This test server uses only Python's standard library. From the repository root:

```sh
python3 server/server.py
```

Then select **0 - EIGEN SERVER** and enter `http://127.0.0.1:8080` in the
emulator. For a physical Pico on another computer, listen on the local network:

```sh
python3 server/server.py --host 0.0.0.0
```

Enter that computer's LAN address on the P2000T, for example
`http://192.168.1.20:8080`. Do not expose this development server directly to
the Internet.

## Customizing pages

Edit `pages/100.txt`. Each text line becomes one 40-column row; short rows and
missing rows are filled with spaces, while text beyond 40 columns is clipped.
Only the first 24 lines are displayed.

Page `101.txt` is a conceal/reveal example. Press `N` from page 100 to open it,
then press `?` or `R` to reveal its answer. Its answer row contains literal
SAA5050 control bytes so it also works in the original minimal text loader.
Some editors show those bytes as control symbols or blank cells.

For easier customization, text pages also accept `\xNN` escapes for raw
SAA5050 bytes: `\x18` starts concealed text and the next colour code, such as
`\x07` (white alphanumeric), ends it. Escapes count as one of the 40 display
cells, not four text characters.

The cartridge writes `ww DD.mmm HH:MM:SS` into cells 1–19 of the first row
(byte indexes 0–18). Keep those cells blank when composing a header. The two
included pages leave the complete header blank except for the page number in
the final three cells.

- Add another numbered `.txt` page. Neighboring files automatically populate
  `prevPage` and `nextPage`, enabling the `P` and `N` shortcuts.
- Add `pages/100-2.txt` for subpage 2. Subpage links are generated
  automatically in numeric order.
- For a completely byte-exact page, use `100.bin` instead. It must contain
  exactly 960 raw SAA5050 bytes and takes priority over `100.txt`.

The defaults can be changed with `--host`, `--port`, and `--pages`. Run
`python3 server/server.py --help` for the complete command line.

## Testing

Run the dependency-free server tests with:

```sh
python3 server/test_server.py
```

To exercise the production cartridge and this live server through the emulator,
start the server in one terminal and follow the `--live` example in
[`../emulator/README.md`](../emulator/README.md).
