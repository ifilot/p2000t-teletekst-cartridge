Z80 assembly client
===================

The slot-1 ROM in ``src/p2wp-cartridge.asm`` is the production P2WP/2 host
implementation. It is also the reference for clients written in Z80 assembly.
The code uses ``z80asm`` syntax and communicates with the interface through
three constants:

.. code-block:: asm

   HOST_TX_PORT:    equ 040h
   HOST_RX_PORT:    equ 041h
   STATUS_PORT:     equ 042h

Raw byte transport
------------------

The transmitter waits for status bit 1 before writing exactly once to
``HOST_TX``. The receiver waits for status bit 0 before reading exactly once
from ``HOST_RX``. Both routines return with carry set after a finite timeout.

.. literalinclude:: ../src/p2wp-cartridge.asm
   :language: asm
   :start-after: ; Wait for TX_READY, then write A. Preserves BC, DE and HL.
   :end-before: ; ---------------------------------------------------------------------------
   :caption: Host-to-Pico byte transport

.. literalinclude:: ../src/p2wp-cartridge.asm
   :language: asm
   :start-after: ; Wait for RX_READY, then read one byte. Preserves BC, DE and HL.
   :end-before: validate_received_frame:
   :caption: Pico-to-host byte transport

Frame transmission and reception
--------------------------------

``send_frame`` emits the opening delimiter, updates the CRC over every
unescaped body byte, escapes reserved values, appends the CRC low byte first,
and emits the closing delimiter. ``receive_frame`` reverses that process and
rejects a dangling escape or an oversized frame.

.. literalinclude:: ../src/p2wp-cartridge.asm
   :language: asm
   :start-at: send_frame:
   :end-before: ; Wait for TX_READY, then write A. Preserves BC, DE and HL.
   :caption: Framing, escaping, and transmission

The CRC routine implements CRC-16/CCITT-FALSE directly in ``HL``. It is useful
as a compact testable unit in other clients; the ASCII test vector
``123456789`` must produce ``0x29B1``.

.. literalinclude:: ../src/p2wp-cartridge.asm
   :language: asm
   :start-at: crc_reset:
   :end-before: ; ---------------------------------------------------------------------------
   :caption: CRC-16/CCITT-FALSE

Transactions
------------

Application code constructs an unescaped six-byte header followed by its
payload in ``FRAME_BUFFER``. It records the expected type and sequence, calls
``transact``, validates the response-specific payload, and advances the
sequence only on success.

The production Teletekst request demonstrates the complete asynchronous
pattern: start the request, poll status, then retrieve four independently
validated 240-byte chunks.

.. literalinclude:: ../src/p2wp-cartridge.asm
   :language: text
   :start-at: teletekst_fetch_page:
   :end-before: teletekst_fetch_failed:
   :caption: Asynchronous Teletekst fetch

Porting the routines
--------------------

When reusing these routines outside the production ROM:

- relocate ``FRAME_BUFFER``, ``RX_BUFFER``, and all state variables to RAM
  owned by the client;
- preserve the P2000T monitor's interrupt handling unless the client supplies
  equivalent keyboard and timing services;
- keep the receive buffer at least 248 bytes for a 240-byte negotiated payload;
- retain the fixed timeout in both byte primitives;
- preserve request bytes across retries; and
- do not advance the sequence until a matching response passes CRC and payload
  validation.

For a BASIC/assembly hybrid, reserve the top of BASIC memory with ``CLEAR``,
load the relocated routines there with ``POKE``, define an entry point using
``DEF USR``, and pass buffers through addresses obtained from ``VARPTR``. The
machine-code routine should perform the complete transaction rather than
returning between individual bytes; this comfortably satisfies the
peripheral's inter-byte deadline.

Building the reference client
-----------------------------

Install ``z80asm`` and run:

.. code-block:: console

   $ make -C src
   $ make -C src verify

The first command assembles and signs the 16 KiB cartridge image. The second
validates its P2000T additive cartridge checksum.
