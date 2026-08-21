P2000T BASIC client
===================

The P2000T BASIC cartridge can use P2WP/2 directly. ``INP`` reads an I/O port,
``OUT`` writes one, integer arrays can hold binary frames, and the logical
operators provide the operations required by CRC-16. The BASIC cartridge
occupies slot 1 while the Pico interface occupies slot 2.

The complete example below establishes a version-2 session and sends an
``ECHO`` request containing ``BASIC``. It implements byte handshaking, finite
timeouts, escaping, CRC generation and validation, response matching, and
three identical transaction attempts.

.. literalinclude:: examples/p2wp-basic.bas
   :language: basic
   :linenos:
   :caption: Complete P2WP/2 HELLO and ECHO diagnostic

Running the diagnostic
----------------------

#. Connect the Pico interface in slot 2 and boot the P2000T BASIC cartridge in
   slot 1.
#. Enter or load ``p2wp-basic.bas``.
#. Start the program with ``RUN``.
#. A working interface prints the selected protocol version, capability byte,
   negotiated payload limit, and ``ECHO: BASIC``.

The example advertises a 240-byte receive limit. This is the minimum usable
limit for the Internet-fetch capability because every ``TELETEKST_FETCH_ROWS``
response carries one fixed 240-byte chunk.

Program structure
-----------------

The important subroutines are:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Lines
     - Purpose
   * - 1000--1120
     - Construct the six-byte header and calculate the request CRC.
   * - 2000--2080
     - Update CRC-16/CCITT-FALSE while keeping separate high and low bytes.
   * - 3000--3450
     - Escape and transmit a frame through ``HOST_TX``.
   * - 4000--4650
     - Receive and unescape a frame from ``HOST_RX``.
   * - 5000--5170
     - Validate its length, CRC, version, flags, type, and sequence.
   * - 6000--6070
     - Perform a stop-and-wait transaction with identical retries.

``ER%`` is zero on success. Values 2 through 6 are local transport or frame
errors. Values 101 through 107 represent protocol errors ``0x01`` through
``0x07`` returned by the peripheral.

Do not replace the finite polling loops with BASIC's ``WAIT`` statement in a
general-purpose client. ``WAIT`` is compact, but it has no timeout and can
leave the machine blocked forever when the interface is absent or stalled.

Sending other requests
----------------------

The application sets ``TY%`` to the message type, ``PL%`` to the payload
length, stores payload bytes in ``P%()``, and calls line 6000. A successful
response body is returned in ``R%()``; its payload begins at ``R%(6)``.

For example, start a fetch of NOS page 100, using the API's default subpage:

.. code-block:: basic

   700 TY%=&H30:PL%=4
   710 PG%=100
   720 P%(0)=PG% AND 255:P%(1)=INT(PG%/256)
   730 P%(2)=0:P%(3)=0
   740 GOSUB 6000
   750 IF ER%<>0 THEN PRINT "FETCH START ERROR";ER%:STOP
   760 SQ%=(SQ%+1) AND 255

The host then polls ``TELETEKST_FETCH_STATUS`` (``TY%=&H31``, ``PL%=0``).
State 3 means that all rows are available; state 4 reports failure, with the
application error in ``R%(7)``. While state 1 or 2 is returned, delay briefly
and issue a new status transaction with the next sequence number.

After completion, request chunk indexes 0 through 3:

.. code-block:: basic

   800 FOR CK%=0 TO 3
   810 TY%=&H32:PL%=1:P%(0)=CK%:GOSUB 6000
   820 IF ER%<>0 OR R%(4)<>240 THEN PRINT "ROW ERROR":STOP
   830 FOR I%=0 TO 239
   840 RW%=CK%*6+INT(I%/40)
   850 CL%=I%-40*INT(I%/40)
   860 POKE &H5000+RW%*80+CL%,R%(6+I%)
   870 NEXT I%
   880 SQ%=(SQ%+1) AND 255
   890 NEXT CK%

The 40 display bytes in each source row are copied into the visible half of
the corresponding 80-byte P2000T video-memory row.

Recommended application sequence
--------------------------------

A full BASIC viewer should use this order:

#. Complete ``HELLO`` and check the required capability bits.
#. Query ``WIFI_PROFILE_STATUS``.
#. If a profile exists, start ``WIFI_PROFILE_CONNECT`` and poll
   ``WIFI_STATUS``.
#. Otherwise scan, retrieve the indexed results, collect a password when
   required, and start ``WIFI_CONNECT``.
#. Start a Teletekst fetch, poll its status, and retrieve all four row chunks.
#. Increment the sequence number only after every accepted response.

For a larger interactive client, a hybrid implementation is recommended:
retain menus and application logic in BASIC, but load the framing and CRC
routines into reserved RAM and call them with ``DEF USR``. This reduces page
transfer time while preserving a BASIC-facing API.

