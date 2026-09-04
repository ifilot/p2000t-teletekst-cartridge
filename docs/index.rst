P2000T Teletekst Cartridge Interface
=====================================

Welcome
-------

The P2000T Teletekst Cartridge brings internet-backed teletext to an original
Philips P2000T. The computer supplies the familiar teletext display and user
interface; a Raspberry Pi Pico W or Pico 2 W handles Wi-Fi and retrieves the
requested pages. The slot-2 cartridge connects these two very different
computers.

They communicate using **P2WP/2–6**, a small protocol designed specifically for
this interface. In simple terms, the P2000T asks the Pico to do something---for
example, connect to Wi-Fi or fetch a page---and the Pico sends a reply. P2WP
adds the checks, acknowledgements, and retry rules needed to make that exchange
reliable.

You do not need to understand every electrical signal or frame field to begin.
The BASIC and assembly guides provide working implementations; the protocol
specification is available when you need the exact details.

How the pieces fit together
---------------------------

.. list-table::
   :header-rows: 1
   :widths: 27 73

   * - Part
     - Role
   * - P2000T slot-1 ROM
     - Displays teletext, accepts keyboard input, and initiates every request.
   * - Slot-2 cartridge logic
     - Moves one byte at a time in either direction and exposes the three
       P2000T I/O ports.
   * - Pico W firmware
     - Responds to requests, manages Wi-Fi, and retrieves teletext pages.
   * - P2WP/2–6
     - Defines how bytes become validated requests and matching responses.

Choose a starting point
-----------------------

* **I am new to the project.** Start with the :ref:`protocol overview
  <protocol-overview>`, then use the :doc:`hardware` guide to see how the
  cartridge moves bytes between the two machines. Together they explain the
  overall system without requiring you to read every message definition.
* **I want to communicate from BASIC.** Follow the :doc:`basic` guide. It
  includes a complete ``HELLO`` and ``ECHO`` diagnostic that can be entered or
  loaded on a P2000T.
* **I am writing Z80 assembly.** See the :doc:`assembly` guide for reusable
  byte-transfer, framing, CRC, and transaction routines from the production
  cartridge ROM.
* **I am implementing compatible hardware or firmware.** Use the
  :doc:`protocol` as the normative reference. Its requirements define what a
  conforming P2WP endpoint must do.
* **I want to build or download the project.** The `project README`_ links to
  release files and contains the hardware and firmware build instructions.

A transaction in plain language
-------------------------------

#. The P2000T checks that the cartridge is ready for a byte.
#. It sends a framed request, beginning a new transaction.
#. The Pico validates the request and performs the requested operation.
#. The Pico returns a response carrying the same message type and sequence
   number.
#. The P2000T validates that response. If communication times out, it can retry
   the same request safely.

``HELLO`` is always the first transaction in a session. ``ECHO`` is the best
first experiment: the Pico simply returns the bytes it received. Both client
guides demonstrate this sequence before moving on to Wi-Fi and teletext
operations.

Reference and implementation guides
-----------------------------------

.. toctree::
   :maxdepth: 2
   :caption: Interface documentation

   hardware
   protocol
   custom-server
   basic
   assembly

.. _project README: https://github.com/ifilot/p2000t-teletekst-cartridge#readme
