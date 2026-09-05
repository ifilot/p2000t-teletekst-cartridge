; P2WP/2-7 Wi-Fi and Teletekst client for a Philips P2000T cartridge.
;
; The monitor maps this 16 KiB ROM at 1000h and enters it at 1010h. The
; sign_cartridge.py fills the checksum length and value after assembly.  The
; zero values below allow the assembler to produce an unsigned image.

        org 01000h

        defb 05eh              ; cartridge signature
        defw 0000h             ; zero bytes to checksum
        defw 0000h             ; zero initial checksum
        defb "P2WP TT    "     ; eleven-byte cartridge label

; ---------------------------------------------------------------------------
; Constants

HOST_TX_PORT:          equ 040h
HOST_RX_PORT:          equ 041h
STATUS_PORT:           equ 042h
VIDEO_CONTROL_PORT:    equ 030h
MONITOR_READ_KEY:      equ 0026h
MONITOR_KEY_AVAILABLE: equ 0029h
MONITOR_CLEAR_KEY:     equ 002ch
MONITOR_CLOCK:         equ 06010h

STATUS_RX_READY:       equ 001h
STATUS_TX_READY:       equ 002h

P2WP_BOOTSTRAP_VERSION: equ 002h
P2WP_MIN_VERSION:      equ 002h
P2WP_MAX_VERSION:      equ 007h
CARTRIDGE_VERSION_MAJOR: equ 0
CARTRIDGE_VERSION_MINOR: equ 5
CARTRIDGE_VERSION_PATCH: equ 0
P2WP_FLAG_RESPONSE:    equ 001h
P2WP_FLAG_ERROR:       equ 002h
P2WP_TYPE_HELLO:       equ 001h
P2WP_TYPE_ECHO:        equ 002h
P2WP_TYPE_DEVICE_INFO: equ 004h
P2WP_TYPE_VERSION_CHECK_START: equ 005h
P2WP_TYPE_VERSION_CHECK_STATUS: equ 006h
P2WP_TYPE_WIFI_SCAN_START:  equ 010h
P2WP_TYPE_WIFI_SCAN_STATUS: equ 011h
P2WP_TYPE_WIFI_SCAN_RESULT: equ 012h
P2WP_TYPE_WIFI_CONNECT:     equ 013h
P2WP_TYPE_WIFI_STATUS:      equ 014h
P2WP_TYPE_WIFI_PROFILE_STATUS: equ 020h
P2WP_TYPE_WIFI_PROFILE_CONNECT: equ 021h
P2WP_TYPE_WIFI_PROFILE_SAVE:   equ 022h
P2WP_TYPE_WIFI_PROFILE_DELETE: equ 023h
P2WP_TYPE_TELETEKST_FETCH_START:  equ 030h
P2WP_TYPE_TELETEKST_FETCH_STATUS: equ 031h
P2WP_TYPE_TELETEKST_FETCH_ROWS:   equ 032h
P2WP_TYPE_TELETEKST_CUSTOM_URL_LOAD: equ 033h
P2WP_TYPE_TELETEKST_CUSTOM_URL_SAVE: equ 034h
P2WP_TYPE_TELETEKST_SETTINGS_LOAD: equ 035h
P2WP_TYPE_TELETEKST_SETTINGS_SAVE: equ 036h
P2WP_CAPABILITY_DEVICE_INFO: equ 010h
P2WP_CAPABILITY_VERSION_CHECK: equ 020h
P2WP_DELIMITER:        equ 07eh
P2WP_ESCAPE:           equ 07dh
P2WP_ESCAPE_XOR:       equ 020h

FRAME_BUFFER:          equ 07000h
RX_BUFFER:             equ 07200h
RX_BUFFER_LIMIT:       equ 07300h
VIDEO_RAM:             equ 05000h
MENU_HEADER_RAM:       equ VIDEO_RAM+80
MENU_RULE_RAM:         equ VIDEO_RAM+160
STACK_TOP:             equ 09ff0h
DISPLAY_HASH:          equ 05fh
SAA5050_ALPHA_WHITE:   equ 007h
SAA5050_ALPHA_YELLOW:  equ 003h
SAA5050_GRAPHICS_WHITE: equ 017h
SAA5050_CONTIGUOUS_GRAPHICS: equ 019h
KEY_START_EVENT:       equ 0fdh
KEY_STOP_EVENT:        equ 0feh
KEY_LEFT_EVENT:        equ 0fch
KEY_RIGHT_EVENT:       equ 0fbh
HOST_MAX_PAYLOAD:      equ 240
MAX_LINE_LENGTH:       equ 32
MAX_PASSWORD_LENGTH:   equ 63
MAX_CUSTOM_URL_LENGTH: equ 96

WIFI_SCAN_RUNNING:     equ 1
WIFI_SCAN_COMPLETE:    equ 2
WIFI_INIT_STARTING:    equ 0
WIFI_INIT_READY:       equ 1
WIFI_CONNECTING:       equ 1
WIFI_CONNECTED:        equ 2
WIFI_CONNECT_ATTEMPTS: equ 3
WIFI_PROFILE_ABSENT:   equ 0
WIFI_PROFILE_READY:    equ 1
WIFI_PROFILE_BUSY:     equ 2
WIFI_PROFILE_ERROR_NONE: equ 0
TELETEKST_CONNECTING:  equ 1
TELETEKST_RECEIVING:   equ 2
TELETEKST_COMPLETE:    equ 3
TELETEKST_FAILED:      equ 4
TELETEKST_ERROR_NOT_CONNECTED: equ 1
TELETEKST_ERROR_TLS_CONFIG: equ 2
TELETEKST_ERROR_REQUEST_START: equ 3
TELETEKST_ERROR_NETWORK: equ 4
TELETEKST_ERROR_HTTP_STATUS: equ 5
TELETEKST_ERROR_TOO_LARGE: equ 6
TELETEKST_ERROR_INVALID_DATA: equ 7
TELETEKST_ERROR_PAGE_NOT_FOUND: equ 8
TELETEKST_ERROR_DNS: equ 9
TELETEKST_ERROR_CONNECT: equ 10
TELETEKST_ERROR_CONNECTION_CLOSED: equ 11
TELETEKST_ERROR_TIMEOUT: equ 12
TELETEKST_ERROR_OUT_OF_MEMORY: equ 13
TELETEKST_ERROR_CONTENT_LENGTH: equ 14
TELETEKST_ERROR_LOCAL_ABORT: equ 15
TELETEKST_CHUNK_COUNT: equ 4
TELETEKST_CHUNK_SIZE:  equ 240
TELETEKST_ROTATE_TICKS: equ 500
TELETEKST_AUTOSTART_TICKS: equ 3000 ; one minute at 20 ms per tick
TELETEKST_CLOCK_TICKS:  equ 50
TELETEKST_BLINK_TICKS:  equ 25
LINK_TIMEOUT_TICKS:    equ 100 ; 100 monitor ticks at 20 ms = 2 seconds
TELETEKST_SOURCE_NOS:   equ 0
TELETEKST_SOURCE_P2000T: equ 1
TELETEKST_SOURCE_CUSTOM: equ 2
TELETEKST_SOURCE_ARCHIVE: equ 3
TELETEKST_MENU_SOURCE_CUSTOM: equ 0
TELETEKST_MENU_SOURCE_NOS: equ 1
TELETEKST_MENU_SOURCE_P2000T: equ 2
TELETEKST_MENU_SOURCE_ARCHIVE: equ 3
TELETEKST_AUTOSTART_DISABLED: equ 0ffh
WIFI_SECURITY_OPEN:    equ 0
WIFI_SECURITY_PSK:     equ 1
VERSION_CHECK_RUNNING: equ 1
VERSION_CHECK_COMPLETE: equ 2
VERSION_CHECK_FAILED:  equ 3

; ---------------------------------------------------------------------------
; Cartridge entry point (1010h)

start:
        ld sp,STACK_TOP
        ; The monitor initialized its 20 ms keyboard interrupt before entering
        ; the cartridge. Keep interrupts enabled so its key FIFO stays active.
        ei
        xor a
        ld (teletekst_clock_valid),a
        ld (custom_url_length),a
        ld (teletekst_reveal_enabled),a
        ld (teletekst_zoom_state),a
        ld (teletekst_page_valid),a
        ld (opening_timed_out),a
        ld (teletekst_auto_page_enabled),a
        ld (teletekst_auto_retry_pending),a
        ld (wifi_cancel_enabled),a
        dec a
        ld (teletekst_auto_start_source),a
        xor a
        ld (hello_error_kind),a
        ld (p2wp_capabilities),a
        ld (device_info_valid),a
        ld (latest_version_valid),a
        ld (latest_version_checked),a
        ld a,P2WP_BOOTSTRAP_VERSION
        ld (p2wp_session_version),a
        call show_opening_screen
        ; Discard the key that may have launched the cartridge, then leave the
        ; completed splash visible, with a blinking prompt, until the user
        ; deliberately continues.
        call MONITOR_CLEAR_KEY
        call wait_for_opening_key
        ld de,VIDEO_RAM+1760
        call clear_line
        ld hl,hello_wait_text
        ld de,VIDEO_RAM+1760
        call write_string

        xor a
        ld (sequence),a
        call build_hello
        ld a,P2WP_TYPE_HELLO
        ld (expected_type),a
        xor a
        ld (expected_sequence),a
hello_retry:
        call transact
        ; A missing or unresponsive Pico must not leave the cartridge waiting
        ; forever. transact bounds the complete three-attempt exchange to two
        ; seconds using the monitor's interrupt-driven 20 ms clock.
        jr c,hello_check_failure
        call validate_hello
        jr c,hello_check_failure

        call next_sequence
        call read_device_info

        ld a,(p2wp_session_version)
        cp 4
        call c,show_protocol_legacy_warning

        jp wifi_profile_startup

hello_check_failure:
        ld a,(hello_error_kind)
        or a
        jp nz,show_protocol_incompatible
        jr hello_failed

echo_loop:
        ld de,VIDEO_RAM+160
        call clear_line
        ld hl,prompt_text
        ld de,VIDEO_RAM+160
        call write_string
        call read_line
        call build_echo
        ld a,P2WP_TYPE_ECHO
        ld (expected_type),a
        ld a,(sequence)
        ld (expected_sequence),a
        call transact
        jr c,echo_failed
        call validate_echo
        jr c,echo_failed

        ; Leave the last Pico response visible while the next line is typed.
        ; Clear it only after a new response has passed framing, CRC, sequence,
        ; length, and byte-for-byte payload validation.
        ld de,VIDEO_RAM+240
        call clear_line
        ld hl,response_text
        ld de,VIDEO_RAM+240
        call write_string
        ld de,VIDEO_RAM+240+6
        ld hl,RX_BUFFER+6
        ld a,(line_length)
        ld b,a
        call write_bytes

        ld hl,(exchange_count)
        inc hl
        ld (exchange_count),hl
        call show_exchange_count

        ld a,(sequence)
        inc a
        ld (sequence),a
        jr echo_loop

hello_failed:
        ld hl,hello_fail_text
        ld de,VIDEO_RAM+160
        call write_string
        jr fatal_loop

echo_failed:
        ld hl,echo_fail_text
        ld de,VIDEO_RAM+400
        call write_string

fatal_loop:
        halt
        jr fatal_loop

; ---------------------------------------------------------------------------
; Request builders

build_hello:
        ld hl,hello_template
        ld de,FRAME_BUFFER
        ld bc,hello_template_end-hello_template
        ldir
        ld a,(sequence)
        ld (FRAME_BUFFER+3),a
        ld hl,hello_template_end-hello_template
        ld (body_length),hl
        ret

build_echo:
        ld hl,echo_header
        ld de,FRAME_BUFFER
        ld bc,echo_header_end-echo_header
        ldir
        ld a,(p2wp_session_version)
        ld (FRAME_BUFFER),a
        ld a,(sequence)
        ld (FRAME_BUFFER+3),a
        ld a,(line_length)
        ld (FRAME_BUFFER+4),a
        or a
        jr z,build_echo_length
        ld c,a
        ld b,0
        ld hl,LINE_BUFFER
        ld de,FRAME_BUFFER+6
        ldir
build_echo_length:
        ld a,(line_length)
        add a,6
        ld l,a
        ld h,0
        ld (body_length),hl
        ret

hello_template:
        defb P2WP_BOOTSTRAP_VERSION,0,P2WP_TYPE_HELLO,0,8,0
        defb "P2WP",P2WP_MIN_VERSION,P2WP_MAX_VERSION,HOST_MAX_PAYLOAD,0
hello_template_end:

echo_header:
        defb P2WP_BOOTSTRAP_VERSION,0,P2WP_TYPE_ECHO,0,0,0
echo_header_end:

; ---------------------------------------------------------------------------
; Stop-and-wait transaction. FRAME_BUFFER/body_length must contain the
; request. Returns carry after three failed attempts.

transact:
        ld hl,(MONITOR_CLOCK)
        ld de,LINK_TIMEOUT_TICKS
        add hl,de
        ld (link_timeout_deadline),hl
        ld a,3
        ld (attempts_left),a
transact_attempt:
        call send_frame
        jr c,transact_retry
        call receive_frame
        jr c,transact_retry

        ld a,(p2wp_session_version)
        ld b,a
        ld a,(RX_BUFFER)
        cp b
        jr nz,transact_retry
        ld a,(expected_type)
        ld b,a
        ld a,(RX_BUFFER+2)
        cp b
        jr nz,transact_retry
        ld a,(expected_sequence)
        ld b,a
        ld a,(RX_BUFFER+3)
        cp b
        jr nz,transact_retry
        ld a,(RX_BUFFER+1)
        and P2WP_FLAG_RESPONSE+P2WP_FLAG_ERROR
        cp P2WP_FLAG_RESPONSE
        jr z,transact_success
        cp P2WP_FLAG_RESPONSE+P2WP_FLAG_ERROR
        jr nz,transact_retry
        ld a,(expected_type)
        cp P2WP_TYPE_HELLO
        jr nz,transact_retry
        ld a,(RX_BUFFER+4)
        cp 1
        jr nz,transact_retry
        ld a,(RX_BUFFER+5)
        or a
        jr nz,transact_retry
        ld a,(RX_BUFFER+6)
        cp 1                    ; P2WP_ERROR_UNSUPPORTED_VERSION
        jr nz,transact_retry
        ld a,1
        ld (hello_error_kind),a
        scf
        ret
transact_success:
        or a                    ; clear carry
        ret

transact_retry:
        ld a,(attempts_left)
        dec a
        ld (attempts_left),a
        jr nz,transact_attempt
        scf
        ret

; ---------------------------------------------------------------------------
; Frame transmitter

send_frame:
        ld a,P2WP_DELIMITER
        call send_byte
        ret c

        call crc_reset
        ld hl,FRAME_BUFFER
        ld bc,(body_length)
send_body_loop:
        ld a,b
        or c
        jr z,send_crc
        ld a,(hl)
        inc hl
        push af
        call crc_update_a
        pop af
        call send_escaped
        ret c
        dec bc
        jr send_body_loop

send_crc:
        ld hl,(crc_value)
        ld a,l
        call send_escaped
        ret c
        ld a,h
        call send_escaped
        ret c
        ld a,P2WP_DELIMITER
        jp send_byte

send_escaped:
        cp P2WP_DELIMITER
        jr z,send_escape_pair
        cp P2WP_ESCAPE
        jp nz,send_byte
send_escape_pair:
        push af
        ld a,P2WP_ESCAPE
        call send_byte
        jr c,send_escape_failed
        pop af
        xor P2WP_ESCAPE_XOR
        jp send_byte
send_escape_failed:
        pop af
        scf
        ret

; Wait for TX_READY, then write A. Preserves BC, DE and HL.
send_byte:
        push bc
        push de
        push hl
        push af
        ld bc,0
send_byte_wait:
        call link_timeout_expired
        jr z,send_byte_timeout
        in a,(STATUS_PORT)
        and STATUS_TX_READY
        jr nz,send_byte_ready
        jr send_byte_wait
send_byte_timeout:
        pop af
        pop hl
        pop de
        pop bc
        scf
        ret
send_byte_ready:
        pop af
        out (HOST_TX_PORT),a
        pop hl
        pop de
        pop bc
        or a                    ; clear carry
        ret

; ---------------------------------------------------------------------------
; Frame receiver and validator

receive_frame:
receive_find_start:
        call receive_byte
        ret c
        cp P2WP_DELIMITER
        jr nz,receive_find_start

        ld hl,RX_BUFFER
        ld (rx_pointer),hl
        ld hl,0
        ld (rx_length),hl
        xor a
        ld (rx_escaped),a

receive_frame_loop:
        call receive_byte
        ret c
        cp P2WP_DELIMITER
        jr z,receive_end

        ld b,a
        ld a,(rx_escaped)
        or a
        ld a,b
        jr z,receive_not_escaped
        xor P2WP_ESCAPE_XOR
        ld b,a
        xor a
        ld (rx_escaped),a
        ld a,b
        jr receive_store

receive_not_escaped:
        cp P2WP_ESCAPE
        jr nz,receive_store
        ld a,1
        ld (rx_escaped),a
        jr receive_frame_loop

receive_store:
        ld hl,(rx_pointer)
        ld de,RX_BUFFER_LIMIT
        push hl
        or a
        sbc hl,de
        pop hl
        jr nc,receive_invalid
        ld (hl),a
        inc hl
        ld (rx_pointer),hl
        ld hl,(rx_length)
        inc hl
        ld (rx_length),hl
        jr receive_frame_loop

receive_end:
        ld hl,(rx_length)
        ld a,h
        or l
        jr z,receive_frame_loop ; repeated delimiter is idle fill
        ld a,(rx_escaped)
        or a
        jr nz,receive_invalid
        call validate_received_frame
        ret

receive_invalid:
        scf
        ret

; Wait for RX_READY, then read one byte. Preserves BC, DE and HL.
receive_byte:
        push bc
        push de
        push hl
        ld bc,0
receive_byte_wait:
        call link_timeout_expired
        jr z,receive_byte_timeout
        in a,(STATUS_PORT)
        and STATUS_RX_READY
        jr nz,receive_byte_ready
        jr receive_byte_wait
receive_byte_timeout:
        pop hl
        pop de
        pop bc
        scf
        ret
receive_byte_ready:
        in a,(HOST_RX_PORT)
        pop hl
        pop de
        pop bc
        or a                    ; clear carry
        ret

; Return Z once the transaction's absolute deadline has been reached. The
; signed subtraction remains wrap-safe because the deadline is only 100 ticks
; ahead (well below half of the monitor clock's 16-bit range).
link_timeout_expired:
        push de
        push hl
        ld hl,(MONITOR_CLOCK)
        ld de,(link_timeout_deadline)
        or a
        sbc hl,de
        bit 7,h
        pop hl
        pop de
        ret

validate_received_frame:
        ld hl,(rx_length)
        ld a,h
        or a
        jr nz,received_bad
        ld a,l
        cp 8
        jr c,received_bad

        ld a,(RX_BUFFER+5)
        or a
        jr nz,received_bad
        ld a,(RX_BUFFER+4)
        add a,8
        jr c,received_bad
        ld b,a
        ld a,(rx_length)
        cp b
        jr nz,received_bad

        call crc_reset
        ld hl,RX_BUFFER
        ld bc,(rx_length)
        dec bc
        dec bc
received_crc_loop:
        ld a,b
        or c
        jr z,received_crc_done
        ld a,(hl)
        inc hl
        call crc_update_a
        dec bc
        jr received_crc_loop

received_crc_done:
        ex de,hl                ; DE points at received CRC
        ld hl,(crc_value)
        ld a,(de)
        cp l
        jr nz,received_bad
        inc de
        ld a,(de)
        cp h
        jr nz,received_bad
        or a
        ret
received_bad:
        scf
        ret

; ---------------------------------------------------------------------------
; CRC-16/CCITT-FALSE

crc_reset:
        ld hl,0ffffh
        ld (crc_value),hl
        ret

; Update the CRC with A. Preserves BC, DE and HL.
crc_update_a:
        push bc
        push de
        push hl
        ld hl,(crc_value)
        xor h
        ld h,a
        ld b,8
crc_bit_loop:
        add hl,hl
        jr nc,crc_no_polynomial
        ld a,h
        xor 010h
        ld h,a
        ld a,l
        xor 021h
        ld l,a
crc_no_polynomial:
        djnz crc_bit_loop
        ld (crc_value),hl
        pop hl
        pop de
        pop bc
        ret

; ---------------------------------------------------------------------------
; Response payload validation

validate_hello:
        ld a,(RX_BUFFER+4)
        cp 8
        jp nz,payload_bad
        ld a,(RX_BUFFER+5)
        or a
        jp nz,payload_bad
        ld hl,RX_BUFFER+6
        ld de,hello_response_prefix
        ld b,4
validate_hello_loop:
        ld a,(de)
        cp (hl)
        jp nz,payload_bad
        inc de
        inc hl
        djnz validate_hello_loop
        ld a,(RX_BUFFER+10)
        cp P2WP_MIN_VERSION
        jr c,hello_payload_incompatible
        cp P2WP_MAX_VERSION+1
        jr nc,hello_payload_incompatible
        ld (p2wp_session_version),a
        ld a,(RX_BUFFER+11)
        ld (p2wp_capabilities),a
        and 00eh
        cp 00eh
        jp nz,payload_bad
        ld a,(RX_BUFFER+12)
        cp HOST_MAX_PAYLOAD
        jp c,payload_bad
        or a
        ret

hello_payload_incompatible:
        ld a,1
        ld (hello_error_kind),a
        scf
        ret

hello_response_prefix:
        defb "P2WP"

; Read the compile-time Pico generation and installed firmware version when
; the negotiated peripheral advertises the optional information command.
read_device_info:
        ld a,(p2wp_capabilities)
        and P2WP_CAPABILITY_DEVICE_INFO
        ret z
        ld a,P2WP_TYPE_DEVICE_INFO
        call prepare_request
        call transact
        jr c,read_device_info_failed
        ld a,(RX_BUFFER+4)
        cp 4
        jr nz,read_device_info_bad_payload
        ld a,(RX_BUFFER+5)
        or a
        jr nz,read_device_info_bad_payload
        ld a,(RX_BUFFER+6)
        cp 1
        jr c,read_device_info_bad_payload
        cp 3
        jr nc,read_device_info_bad_payload
        ld (pico_hardware_model),a
        ld hl,RX_BUFFER+7
        ld de,pico_current_version
        ld bc,3
        ldir
        ld a,1
        ld (device_info_valid),a
        call next_sequence
        ret
read_device_info_bad_payload:
        call next_sequence
read_device_info_failed:
        xor a
        ld (device_info_valid),a
        ret

validate_echo:
        ld a,(RX_BUFFER+4)
        ld b,a
        ld a,(line_length)
        cp b
        jr nz,payload_bad
        ld a,(RX_BUFFER+5)
        or a
        jr nz,payload_bad
        ld hl,RX_BUFFER+6
        ld de,LINE_BUFFER
        ld a,(line_length)
        or a
        ret z
        ld b,a
validate_echo_loop:
        ld a,(de)
        cp (hl)
        jr nz,payload_bad
        inc de
        inc hl
        djnz validate_echo_loop
        or a
        ret

payload_bad:
        scf
        ret

; ---------------------------------------------------------------------------
; Single encrypted Wi-Fi profile

; The Pico reports only profile presence, operation state and an error code.
; SSID and password bytes never return over the cartridge link. Sequence 1 is
; reserved for the first profile query after HELLO; all following provisioning
; commands continue monotonically from there.
wifi_profile_startup:
        xor a
        ld (password_length),a
        ld (wifi_profile_mode),a
        call erase_password
        call wifi_profile_read_status
        jp c,wifi_protocol_failed
        ld a,(wifi_profile_state)
        cp WIFI_PROFILE_ABSENT
        jp z,wifi_setup
        cp WIFI_PROFILE_READY
        jp nz,wifi_protocol_failed
        jp wifi_profile_connect

wifi_profile_delete:
        ld a,P2WP_TYPE_WIFI_PROFILE_DELETE
        call prepare_request
        call transact
        jp c,wifi_protocol_failed
        call validate_empty_response
        jp c,wifi_protocol_failed
        call next_sequence
        call wifi_profile_wait
        jp c,wifi_protocol_failed
        ld a,(wifi_profile_state)
        cp WIFI_PROFILE_ABSENT
        jp nz,wifi_protocol_failed
        jp wifi_setup

wifi_profile_connect:
        call clear_screen
        ld hl,wifi_profile_title_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+160
        call write_string
        ld hl,wifi_profile_connecting_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,wifi_profile_automatic_text
        ld de,VIDEO_RAM+480
        call write_string
        ld a,P2WP_TYPE_WIFI_PROFILE_CONNECT
        call prepare_request
        call transact
        jp c,wifi_protocol_failed
        call validate_empty_response
        jp c,wifi_protocol_failed
        call next_sequence
        call wifi_profile_wait
        jp c,wifi_protocol_failed
        ld a,(wifi_profile_error)
        or a
        jr nz,wifi_profile_corrupt
        ld a,(wifi_profile_state)
        cp WIFI_PROFILE_READY
        jp nz,wifi_protocol_failed
        ld a,1
        ld (wifi_profile_mode),a
        jp wifi_profile_connect_poll

wifi_profile_corrupt:
        ld hl,wifi_profile_corrupt_text
        jr wifi_profile_show_failure
wifi_profile_connect_failed:
        ld hl,wifi_profile_connect_failed_text
wifi_profile_show_failure:
        push hl
        call clear_screen
        ld hl,wifi_profile_title_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+160
        call write_string
        pop hl
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,wifi_profile_retry_text
        ld de,VIDEO_RAM+480
        call write_string
wifi_profile_failure_key:
        call read_key
        cp 'O'
        jp z,wifi_profile_connect
        cp 'o'
        jp z,wifi_profile_connect
        cp 'N'
        jp z,wifi_setup
        cp 'n'
        jp z,wifi_setup
        cp 'V'
        jp z,wifi_profile_delete
        cp 'v'
        jp z,wifi_profile_delete
        jr wifi_profile_failure_key

wifi_profile_connect_poll:
        call poll_delay
        ld a,P2WP_TYPE_WIFI_STATUS
        call prepare_request
        call transact
        jp c,wifi_protocol_failed
        ld a,(RX_BUFFER+4)
        cp 1
        jp nz,wifi_protocol_failed
        ld a,(RX_BUFFER+5)
        or a
        jp nz,wifi_protocol_failed
        call next_sequence
        ld a,(RX_BUFFER+6)
        cp WIFI_CONNECTING
        jr z,wifi_profile_connect_poll
        cp WIFI_CONNECTED
        jp z,wifi_connected
        jp wifi_profile_connect_failed

; Query and validate the asynchronous profile state. Returns carry only for a
; framing/protocol failure; profile operation errors remain in the two state
; bytes so a damaged record can be handled as a normal UI outcome.
wifi_profile_read_status:
        ld a,P2WP_TYPE_WIFI_PROFILE_STATUS
        call prepare_request
        call transact
        ret c
        ld a,(RX_BUFFER+4)
        cp 2
        jr nz,wifi_profile_status_bad
        ld a,(RX_BUFFER+5)
        or a
        jr nz,wifi_profile_status_bad
        ld a,(RX_BUFFER+6)
        cp WIFI_PROFILE_BUSY+1
        jr nc,wifi_profile_status_bad
        ld (wifi_profile_state),a
        ld a,(RX_BUFFER+7)
        ld (wifi_profile_error),a
        call next_sequence
        or a
        ret
wifi_profile_status_bad:
        scf
        ret

wifi_profile_wait:
        call poll_delay
        call wifi_profile_read_status
        ret c
        ld a,(wifi_profile_state)
        cp WIFI_PROFILE_BUSY
        jr z,wifi_profile_wait
        or a
        ret

; ---------------------------------------------------------------------------
; Wi-Fi provisioning user interface

wifi_setup:
        call clear_screen
        xor a
        ld (wifi_profile_mode),a
        ld (password_length),a
        ld hl,wifi_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,wifi_scanning_text
        ld de,VIDEO_RAM+160
        call write_string
        ld hl,wifi_poll_text
        ld de,VIDEO_RAM+240
        call write_string
        ld hl,wifi_seen_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,wifi_radio_starting_text
        ld de,VIDEO_RAM+400
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,0
        ld (exchange_count),hl
        ld a,P2WP_TYPE_WIFI_SCAN_START
        call prepare_request
        call transact
        jp c,wifi_protocol_failed
        call validate_empty_response
        jp c,wifi_protocol_failed
        call next_sequence

wifi_scan_poll:
        call poll_delay
        call try_read_key
        cp KEY_STOP_EVENT
        jr nz,wifi_scan_poll_continue
        ld a,(wifi_cancel_enabled)
        or a
        jp nz,wifi_cancel_to_source
wifi_scan_poll_continue:
        ld a,P2WP_TYPE_WIFI_SCAN_STATUS
        call prepare_request
        call transact
        jp c,wifi_protocol_failed
        ld a,(RX_BUFFER+4)
        cp 3
        jp nz,wifi_protocol_failed
        ld a,(RX_BUFFER+5)
        or a
        jp nz,wifi_protocol_failed
        ; Advance the display only after a complete, CRC-valid status response
        ; of the expected type and sequence has arrived from the Pico.
        call show_wifi_poll
        ld a,(RX_BUFFER+7)
        add a,'0'
        ld (VIDEO_RAM+320+24),a
        call show_wifi_phase
        call next_sequence
        ld a,(RX_BUFFER+6)
        cp WIFI_SCAN_RUNNING
        jr z,wifi_scan_poll
        cp WIFI_SCAN_COMPLETE
        jp nz,wifi_scan_failed
        ld a,(RX_BUFFER+7)
        or a
        jp z,wifi_no_networks
        cp 10
        jr c,wifi_count_ok
        ld a,9
wifi_count_ok:
        ld (wifi_network_count),a

        call clear_screen
        ld hl,wifi_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,wifi_list_heading
        ld de,VIDEO_RAM+160
        call write_string
        call wifi_prepare_list_panel
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+960
        call write_string
        ld hl,VIDEO_RAM+240
        ld (list_screen_pointer),hl
        xor a
        ld (wifi_result_index),a

wifi_result_loop:
        ld a,P2WP_TYPE_WIFI_SCAN_RESULT
        call prepare_request
        ld a,(wifi_result_index)
        ld (FRAME_BUFFER+6),a
        ld a,1
        ld (FRAME_BUFFER+4),a
        ld hl,7
        ld (body_length),hl
        call transact
        jp c,wifi_protocol_failed
        call validate_wifi_result
        jp c,wifi_protocol_failed
        call display_wifi_result
        call next_sequence
        ld a,(wifi_result_index)
        inc a
        ld (wifi_result_index),a
        ld b,a
        ld a,(wifi_network_count)
        cp b
        jr nz,wifi_result_loop

wifi_choose_network:
        ld de,VIDEO_RAM+1040
        call clear_line
        ld hl,wifi_select_text
        ld de,VIDEO_RAM+1040
        call write_string
wifi_choose_key:
        call read_key
        cp KEY_STOP_EVENT
        jr nz,wifi_choose_number
        ld a,(wifi_cancel_enabled)
        or a
        jp nz,wifi_cancel_to_source
        jr wifi_choose_key
wifi_choose_number:
        cp '1'
        jr c,wifi_choose_key
        sub '1'
        ld b,a
        ld a,(wifi_network_count)
        cp b
        jr z,wifi_choose_key
        jr c,wifi_choose_key
        ld a,b
        ld (wifi_selected_index),a

        ld e,a
        ld d,0
        ld hl,WIFI_SECURITY_LIST
        add hl,de
        ld a,(hl)
        cp WIFI_SECURITY_OPEN
        jr z,wifi_open_network
        cp WIFI_SECURITY_PSK
        jr z,wifi_read_password
        ld de,VIDEO_RAM+1120
        call clear_line
        ld hl,wifi_unsupported_text
        ld de,VIDEO_RAM+1120
        call write_string
        jr wifi_choose_network

wifi_open_network:
        ld de,VIDEO_RAM+1120
        call clear_line
        ld de,VIDEO_RAM+1200
        call clear_line
        xor a
        ld (password_length),a
        jr wifi_begin_connect

wifi_read_password:
        ; A retry may return here with the previous password still in RAM.
        ; Clear it before accepting a replacement.
        call erase_password
        ld de,VIDEO_RAM+1120
        call clear_line
        ld de,VIDEO_RAM+1200
        call clear_line
        ld de,VIDEO_RAM+1280
        call clear_line
        ld de,VIDEO_RAM+1360
        call clear_line
        ld hl,wifi_password_visibility_text
        ld de,VIDEO_RAM+1120
        call write_string
        ld hl,wifi_blue_blank_text
        ld de,VIDEO_RAM+1200
        call write_string
        call read_password_visibility
        jp c,wifi_cancel_to_source

        ld de,VIDEO_RAM+1120
        call clear_line
        ld de,VIDEO_RAM+1200
        call clear_line
        ld hl,wifi_password_text
        ld de,VIDEO_RAM+1120
        call write_string
        ld hl,wifi_white_blank_text
        ld de,VIDEO_RAM+1200
        call write_string
        call read_password
        jp c,wifi_cancel_to_source
        ld a,(password_length)
        cp 8
        jr nc,wifi_begin_connect
        call erase_password
        ld de,VIDEO_RAM+1280
        call clear_line
        ld hl,wifi_password_short_text
        ld de,VIDEO_RAM+1280
        call write_string
        jr wifi_read_password

; Start a batch of complete connection attempts. The Pico clears its password
; copy after starting each attempt; the cartridge keeps the session copy and
; resends it if authentication or association fails transiently.
wifi_begin_connect:
        ld a,WIFI_CONNECT_ATTEMPTS
        ld (wifi_connect_attempts_left),a
wifi_start_connect:
        ld de,VIDEO_RAM+1360
        call clear_line
        ld a,P2WP_TYPE_WIFI_CONNECT
        call prepare_request
        ld a,(wifi_selected_index)
        ld (FRAME_BUFFER+6),a
        ld a,(password_length)
        ld (FRAME_BUFFER+7),a
        or a
        jr z,wifi_connect_length
        ld c,a
        ld b,0
        ld hl,LINE_BUFFER
        ld de,FRAME_BUFFER+8
        ldir
wifi_connect_length:
        ld a,(password_length)
        add a,2
        ld (FRAME_BUFFER+4),a
        add a,6
        ld l,a
        ld h,0
        ld (body_length),hl
        call transact
        jr c,wifi_connect_request_failed
        call validate_empty_response
        jr c,wifi_connect_request_failed
        call next_sequence
        call clear_password_display

        ld de,VIDEO_RAM+1280
        call clear_line
        ld hl,wifi_connecting_text
        ld de,VIDEO_RAM+1280
        call write_string
        ld a,WIFI_CONNECT_ATTEMPTS+1
        ld b,a
        ld a,(wifi_connect_attempts_left)
        ld c,a
        ld a,b
        sub c
        add a,'0'
        ld (VIDEO_RAM+1280+22),a

wifi_connect_poll:
        call poll_delay
        call try_read_key
        cp KEY_STOP_EVENT
        jr nz,wifi_connect_poll_continue
        ld a,(wifi_cancel_enabled)
        or a
        jp nz,wifi_cancel_to_source
wifi_connect_poll_continue:
        ld a,P2WP_TYPE_WIFI_STATUS
        call prepare_request
        call transact
        jp c,wifi_protocol_failed
        ld a,(RX_BUFFER+4)
        cp 1
        jp nz,wifi_protocol_failed
        ld a,(RX_BUFFER+5)
        or a
        jp nz,wifi_protocol_failed
        call next_sequence
        ld a,(RX_BUFFER+6)
        cp WIFI_CONNECTING
        jr z,wifi_connect_poll
        cp WIFI_CONNECTED
        jp z,wifi_connected
        cp 4
        jp z,wifi_bad_password
        jp wifi_connect_failed

wifi_connect_request_failed:
        call erase_password
        call clear_password_display
        jp wifi_protocol_failed

wifi_connected:
        xor a
        ld (wifi_cancel_enabled),a
        ld a,(wifi_profile_mode)
        or a
        call z,wifi_profile_offer_save
        call erase_password
        call firmware_check_latest
        call teletekst_load_settings
        ld a,(opening_timed_out)
        or a
        jr z,wifi_connected_manual_source
        ld a,(teletekst_auto_start_source)
        cp TELETEKST_AUTOSTART_DISABLED
        jr z,wifi_connected_manual_source
        call teletekst_select_menu_source
        jr c,wifi_connected_manual_source
        ld a,1
        ld (teletekst_auto_page_enabled),a
        jr wifi_connected_source_ready
wifi_connected_manual_source:
        xor a
        ld (teletekst_auto_page_enabled),a
        call teletekst_choose_source
wifi_connected_source_ready:
        ld hl,100
        ld (teletekst_page),hl
        xor a
        ld (teletekst_subpage),a
        ld (teletekst_input_count),a
        ld (teletekst_rotation_paused),a
        ld (teletekst_cycle_started),a
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

; A manually provisioned connection can replace the single encrypted profile
; or remain session-only. The cartridge retains the Wi-Fi password only until
; this choice has completed, then wipes it from the cartridge RAM.
wifi_profile_offer_save:
        call clear_screen
        ld hl,wifi_profile_title_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+160
        call write_string
        ld hl,wifi_profile_save_offer_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,wifi_profile_save_choice_text
        ld de,VIDEO_RAM+480
        call write_string
wifi_profile_save_choice:
        call read_key
        cp 'N'
        ret z
        cp 'n'
        ret z
        cp 'J'
        jr z,wifi_profile_send_save
        cp 'j'
        jr nz,wifi_profile_save_choice

wifi_profile_send_save:
        ld a,P2WP_TYPE_WIFI_PROFILE_SAVE
        call prepare_request
        ld a,(password_length)
        ld (FRAME_BUFFER+6),a
        or a
        jr z,wifi_profile_save_lengths
        ld c,a
        ld b,0
        ld hl,LINE_BUFFER
        ld de,FRAME_BUFFER+7
        ldir
wifi_profile_save_lengths:
        ld a,(password_length)
        inc a
        ld (FRAME_BUFFER+4),a
        add a,6
        ld l,a
        ld h,0
        ld (body_length),hl
        call transact
        jp c,wifi_profile_save_request_failed
        call validate_empty_response
        jp c,wifi_profile_save_request_failed
        call next_sequence
        call wifi_profile_show_saving
        call wifi_profile_wait
        jp c,wifi_protocol_failed
        ld a,(wifi_profile_error)
        or a
        jr nz,wifi_profile_save_failed
        ld a,(wifi_profile_state)
        cp WIFI_PROFILE_READY
        jr nz,wifi_profile_save_failed
        call clear_screen
        ld hl,wifi_profile_title_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,wifi_profile_saved_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,wifi_profile_continue_text
        ld de,VIDEO_RAM+480
        call write_string
        call read_key
        ret

wifi_profile_save_request_failed:
        jp wifi_protocol_failed

wifi_profile_save_failed:
        call clear_screen
        ld hl,wifi_profile_title_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,wifi_profile_save_failed_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,wifi_profile_continue_text
        ld de,VIDEO_RAM+480
        call write_string
        call read_key
        ret

wifi_profile_show_saving:
        call clear_screen
        ld hl,wifi_profile_title_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+160
        call write_string
        ld hl,wifi_profile_encrypting_text
        ld de,VIDEO_RAM+320
        call write_string
        ret

; Ask the Pico to retrieve the newest GitHub release after Wi-Fi is up. This
; optional exchange never prevents normal Teletekst use: old firmware or an
; Internet/API error simply leaves the latest version marked unavailable.
firmware_check_latest:
        xor a
        ld (latest_version_valid),a
        ld (latest_version_error),a
        inc a
        ld (latest_version_checked),a
        ld a,(p2wp_capabilities)
        and P2WP_CAPABILITY_VERSION_CHECK
        ret z

        ld a,P2WP_TYPE_VERSION_CHECK_START
        call prepare_request
        call transact
        ret c
        call validate_empty_response
        jr c,firmware_check_start_bad
        call next_sequence
        ld hl,(MONITOR_CLOCK)
        ld de,1500             ; 30-second overall Internet lookup limit
        add hl,de
        ld (version_check_deadline),hl
        jr firmware_check_poll

firmware_check_start_bad:
        call next_sequence
        ret

firmware_check_poll:
        call poll_delay
        ld a,P2WP_TYPE_VERSION_CHECK_STATUS
        call prepare_request
        call transact
        ret c
        ld a,(RX_BUFFER+4)
        cp 5
        jr nz,firmware_check_bad_payload
        ld a,(RX_BUFFER+5)
        or a
        jr nz,firmware_check_bad_payload
        call next_sequence
        ld a,(RX_BUFFER+6)
        cp VERSION_CHECK_RUNNING
        jr z,firmware_check_running
        cp VERSION_CHECK_COMPLETE
        jr z,firmware_check_complete
        cp VERSION_CHECK_FAILED
        ret nz
        ld a,(RX_BUFFER+7)
        ld (latest_version_error),a
        ret

firmware_check_bad_payload:
        call next_sequence
        ret

firmware_check_running:
        ld hl,(MONITOR_CLOCK)
        ld de,(version_check_deadline)
        or a
        sbc hl,de
        bit 7,h
        jr nz,firmware_check_poll
        ret

firmware_check_complete:
        ld hl,RX_BUFFER+8
        ld de,latest_release_version
        ld bc,3
        ldir
        ld a,1
        ld (latest_version_valid),a
        ret

; Choose the page API once per session, immediately after Wi-Fi has acquired
; an address. The selected source accompanies every subsequent page request.
teletekst_choose_source:
        call clear_screen
        ld hl,source_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,source_title_tag_text
        ld de,MENU_HEADER_RAM+31
        call write_string
        ld hl,opening_blue_rule_text
        ld de,MENU_RULE_RAM
        call write_string
        ld hl,source_intro_text
        ld de,VIDEO_RAM+240
        call write_string
        ld hl,source_white_blank_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,source_nos_text
        ld de,VIDEO_RAM+400
        call write_string
        ld hl,source_p2000t_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,source_archive_text
        ld de,VIDEO_RAM+560
        call write_string
        ld hl,source_custom_text
        ld de,VIDEO_RAM+640
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+720
        call write_string
        ld hl,source_prompt_text
        ld de,VIDEO_RAM+800
        call write_string
        call teletekst_show_auto_start
        ld hl,source_controls_title_text
        ld de,VIDEO_RAM+960
        call write_string
        ld hl,source_control_display_text
        ld de,VIDEO_RAM+1040
        call write_string
        ld hl,source_control_pages_text
        ld de,VIDEO_RAM+1120
        call write_string
        ld hl,source_control_subpage_text
        ld de,VIDEO_RAM+1200
        call write_string
        ld hl,source_control_wifi_text
        ld de,VIDEO_RAM+1280
        call write_string
        ld hl,source_control_stop_text
        ld de,VIDEO_RAM+1360
        call write_string
        call show_source_runtime_info
        ld hl,opening_footer_text
        ld de,VIDEO_RAM+1840
        call write_string
        ld hl,opening_footer_version_text
        ld de,VIDEO_RAM+1840+34
        call write_string
teletekst_choose_source_key:
        call read_key
        cp 'A'
        jp z,teletekst_choose_auto_start
        cp 'a'
        jp z,teletekst_choose_auto_start
        cp 'H'
        jp z,teletekst_show_help_from_source
        cp 'h'
        jp z,teletekst_show_help_from_source
        cp 'W'
        jp z,teletekst_change_wifi
        cp 'w'
        jp z,teletekst_change_wifi
        cp '1'
        jr z,teletekst_choose_source_selected
        cp '0'
        jr z,teletekst_choose_source_custom
        cp '2'
        jr z,teletekst_choose_source_selected
        cp '3'
        jr nz,teletekst_choose_source_key
teletekst_choose_source_selected:
        sub '0'
        call teletekst_select_menu_source
        jp c,teletekst_custom_requires_v4
        ret
teletekst_choose_source_custom:
        ld a,(p2wp_session_version)
        cp 4
        jp c,teletekst_custom_requires_v4
        call teletekst_enter_custom_url
        jp c,teletekst_choose_source
        ld a,TELETEKST_SOURCE_CUSTOM
        ld (teletekst_source),a
        ret

; Convert the source-menu value in A to the P2WP source and prepare fixed or
; persisted custom URLs. Carry means that this choice cannot currently start.
teletekst_select_menu_source:
        cp TELETEKST_MENU_SOURCE_NOS
        jr z,teletekst_select_source_nos
        cp TELETEKST_MENU_SOURCE_P2000T
        jr z,teletekst_select_source_p2000t
        cp TELETEKST_MENU_SOURCE_ARCHIVE
        jr z,teletekst_select_source_archive
        call teletekst_load_custom_url
        ld a,(custom_url_length)
        or a
        jr z,teletekst_select_source_failed
        ld a,TELETEKST_SOURCE_CUSTOM
        jr teletekst_select_source_store
teletekst_select_source_archive:
        ld a,(p2wp_session_version)
        cp 7
        jr c,teletekst_select_source_archive_legacy
        ld a,TELETEKST_SOURCE_ARCHIVE
        jr teletekst_select_source_store
teletekst_select_source_archive_legacy:
        cp 4
        jr c,teletekst_select_source_failed
        ld hl,archive_url_text
        ld de,CUSTOM_URL_BUFFER
        ld bc,archive_url_text_end-archive_url_text
        ldir
        ld a,archive_url_text_end-archive_url_text
        ld (custom_url_length),a
        ld a,TELETEKST_SOURCE_CUSTOM
        jr teletekst_select_source_store
teletekst_select_source_p2000t:
        ld a,TELETEKST_SOURCE_P2000T
        jr teletekst_select_source_store
teletekst_select_source_nos:
        ld a,TELETEKST_SOURCE_NOS
teletekst_select_source_store:
        ld (teletekst_source),a
        or a
        ret
teletekst_select_source_failed:
        scf
        ret

; A on the source menu cycles off -> NOS -> P2000T -> archive -> custom -> off.
; P2WP/6 stores the choice in the Pico's write-minimizing preferences record.
teletekst_choose_auto_start:
        ld a,(p2wp_session_version)
        cp 6
        jp c,teletekst_choose_source_key
        ld a,(teletekst_auto_start_source)
        cp TELETEKST_AUTOSTART_DISABLED
        jr nz,teletekst_choose_auto_start_not_off
        ld a,TELETEKST_MENU_SOURCE_NOS
        jr teletekst_choose_auto_start_store
teletekst_choose_auto_start_not_off:
        cp TELETEKST_MENU_SOURCE_CUSTOM
        jr nz,teletekst_choose_auto_start_increment
        ld a,TELETEKST_AUTOSTART_DISABLED
        jr teletekst_choose_auto_start_store
teletekst_choose_auto_start_increment:
        inc a
        cp 4
        jr c,teletekst_choose_auto_start_store
        xor a
teletekst_choose_auto_start_store:
        ld (teletekst_auto_start_source),a
        call teletekst_save_settings
        call teletekst_show_auto_start
        jp teletekst_choose_source_key

teletekst_custom_requires_v4:
        ld hl,source_custom_v4_text
        ld de,VIDEO_RAM+1520
        call write_string
        jp teletekst_choose_source_key

; Enter or edit an HTTP(S) base URL. P2WP/5 restores the Pico's persisted value;
; older P2WP/4 peripherals retain the value only for this cartridge session.
teletekst_enter_custom_url:
        call teletekst_load_custom_url
        call clear_screen
        ld hl,custom_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,opening_blue_rule_text
        ld de,MENU_RULE_RAM
        call write_string
        ld hl,custom_intro_text
        ld de,VIDEO_RAM+240
        call write_string
        ld hl,custom_memory_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,custom_example_text
        ld de,VIDEO_RAM+400
        call write_string
        ld hl,custom_security_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,custom_input_text
        ld de,VIDEO_RAM+640
        call write_string
        ld hl,custom_field_text
        ld de,VIDEO_RAM+720
        call write_string
        ld hl,custom_field_text
        ld de,VIDEO_RAM+800
        call write_string
        ld hl,custom_field_text
        ld de,VIDEO_RAM+880
        call write_string
        ld hl,custom_controls_text
        ld de,VIDEO_RAM+1040
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+1760
        call write_string
        ld hl,opening_footer_text
        ld de,VIDEO_RAM+1840
        call write_string
        ld hl,opening_footer_version_text
        ld de,VIDEO_RAM+1840+34
        call write_string
        call teletekst_draw_custom_url
teletekst_custom_url_key:
        call read_key
        cp KEY_STOP_EVENT
        jr z,teletekst_custom_url_cancel
        cp 00dh
        jr z,teletekst_custom_url_accept
        cp 008h
        jr z,teletekst_custom_url_backspace
        cp 020h
        jr c,teletekst_custom_url_key
        cp 07fh
        jr nc,teletekst_custom_url_key
        ld c,a
        ld a,(custom_url_length)
        cp MAX_CUSTOM_URL_LENGTH
        jr nc,teletekst_custom_url_key
        call teletekst_custom_video_pointer
        ld a,c
        call ascii_to_display
        ld (de),a
        ld a,(custom_url_length)
        ld e,a
        ld d,0
        ld hl,CUSTOM_URL_BUFFER
        add hl,de
        ld (hl),c
        ld a,(custom_url_length)
        inc a
        ld (custom_url_length),a
        jr teletekst_custom_url_key
teletekst_custom_url_backspace:
        ld a,(custom_url_length)
        or a
        jr z,teletekst_custom_url_key
        dec a
        ld (custom_url_length),a
        call teletekst_custom_video_pointer
        ld a,020h
        ld (de),a
        jr teletekst_custom_url_key
teletekst_custom_url_accept:
        ld a,(custom_url_length)
        or a
        jr z,teletekst_custom_url_key
        call teletekst_save_custom_url
        or a
        ret
teletekst_custom_url_cancel:
        scf
        ret

; P2WP/5 returns one length byte followed by the last valid URL. Missing or
; invalid storage is represented as a zero length and simply leaves the field
; empty. Protocol/transport failures preserve the current session value.
teletekst_load_custom_url:
        ld a,(p2wp_session_version)
        cp 5
        ret c
        xor a
        ld (custom_url_length),a
        ld a,P2WP_TYPE_TELETEKST_CUSTOM_URL_LOAD
        call prepare_request
        call transact
        ret c
        call next_sequence
        ld a,(RX_BUFFER+5)
        or a
        ret nz
        ld a,(RX_BUFFER+4)
        cp 1
        ret c
        cp MAX_CUSTOM_URL_LENGTH+2
        ret nc
        ld b,a
        ld a,(RX_BUFFER+6)
        cp MAX_CUSTOM_URL_LENGTH+1
        ret nc
        inc a
        cp b
        ret nz
        dec a
        or a
        ret z
        ld (custom_url_length),a
        ld c,a
        ld b,0
        ld hl,RX_BUFFER+7
        ld de,CUSTOM_URL_BUFFER
        ldir
        ret

; Queue the accepted URL for Pico flash storage. The Pico compares it with the
; existing record and skips the flash erase/program cycle when it is unchanged.
teletekst_save_custom_url:
        ld a,(p2wp_session_version)
        cp 5
        ret c
        ld a,P2WP_TYPE_TELETEKST_CUSTOM_URL_SAVE
        call prepare_request
        ld a,(custom_url_length)
        ld (FRAME_BUFFER+6),a
        ld c,a
        ld b,0
        ld hl,CUSTOM_URL_BUFFER
        ld de,FRAME_BUFFER+7
        ldir
        ld a,(custom_url_length)
        inc a
        ld (FRAME_BUFFER+4),a
        add a,6
        ld l,a
        ld h,0
        ld (body_length),hl
        call transact
        ret c
        call validate_empty_response
        push af
        call next_sequence
        pop af
        ret

; P2WP/6 persists one source-menu value for unattended startup. Older firmware
; simply leaves auto-start disabled and continues to offer every manual source.
teletekst_load_settings:
        ld a,TELETEKST_AUTOSTART_DISABLED
        ld (teletekst_auto_start_source),a
        ld a,(p2wp_session_version)
        cp 6
        ret c
        ld a,P2WP_TYPE_TELETEKST_SETTINGS_LOAD
        call prepare_request
        call transact
        ret c
        call next_sequence
        ld a,(RX_BUFFER+5)
        or a
        ret nz
        ld a,(RX_BUFFER+4)
        cp 1
        ret nz
        ld a,(RX_BUFFER+6)
        cp TELETEKST_AUTOSTART_DISABLED
        jr z,teletekst_load_settings_store
        cp 4
        ret nc
teletekst_load_settings_store:
        ld (teletekst_auto_start_source),a
        ret

teletekst_save_settings:
        ld a,P2WP_TYPE_TELETEKST_SETTINGS_SAVE
        call prepare_request
        ld a,(teletekst_auto_start_source)
        ld (FRAME_BUFFER+6),a
        ld a,1
        ld (FRAME_BUFFER+4),a
        ld hl,7
        ld (body_length),hl
        call transact
        jr c,teletekst_save_settings_failed
        call validate_empty_response
        push af
        call next_sequence
        pop af
        ret
teletekst_save_settings_failed:
        call next_sequence
        scf
        ret

teletekst_show_auto_start:
        ld hl,source_auto_start_v6_text
        ld a,(p2wp_session_version)
        cp 6
        jr c,teletekst_show_auto_start_write
        ld hl,source_auto_start_off_text
        ld a,(teletekst_auto_start_source)
        cp TELETEKST_AUTOSTART_DISABLED
        jr z,teletekst_show_auto_start_write
        ld hl,source_auto_start_custom_text
        or a
        jr z,teletekst_show_auto_start_write
        ld hl,source_auto_start_nos_text
        dec a
        jr z,teletekst_show_auto_start_write
        ld hl,source_auto_start_p2000t_text
        dec a
        jr z,teletekst_show_auto_start_write
        ld hl,source_auto_start_archive_text
teletekst_show_auto_start_write:
        ld de,VIDEO_RAM+880
        push hl
        call clear_line
        pop hl
        ld de,VIDEO_RAM+880
        jp write_string

; Display the previously entered value and preserve it while sources change.
teletekst_draw_custom_url:
        ld a,(custom_url_length)
        or a
        ret z
        ld b,a
        ld c,0
        ld hl,CUSTOM_URL_BUFFER
        ld de,VIDEO_RAM+720+4
teletekst_draw_custom_url_loop:
        ld a,(hl)
        call ascii_to_display
        ld (de),a
        inc hl
        inc de
        inc c
        ld a,c
        cp 32
        jr nz,teletekst_draw_custom_url_next
        ld c,0
        push hl
        ld hl,48
        add hl,de
        ex de,hl
        pop hl
teletekst_draw_custom_url_next:
        djnz teletekst_draw_custom_url_loop
        ret

; Map URL byte index A to its visible cell across three 32-character rows.
teletekst_custom_video_pointer:
        cp 32
        jr c,teletekst_custom_video_first
        cp 64
        jr c,teletekst_custom_video_second
        sub 64
        ld hl,VIDEO_RAM+880+4
        jr teletekst_custom_video_add
teletekst_custom_video_second:
        sub 32
        ld hl,VIDEO_RAM+800+4
        jr teletekst_custom_video_add
teletekst_custom_video_first:
        ld hl,VIDEO_RAM+720+4
teletekst_custom_video_add:
        ld e,a
        ld d,0
        add hl,de
        ex de,hl
        ret

; STOP returns to source selection without dropping the Wi-Fi connection.
; Keep the selected page number, but start that page at its default subpage on
; the newly selected server.
teletekst_change_source:
        xor a
        ld (teletekst_rotation_enabled),a
        ld (teletekst_auto_retry_pending),a
        ld (teletekst_input_count),a
        ld (teletekst_subpage),a
        ld (teletekst_cycle_started),a
        call MONITOR_CLEAR_KEY
        call teletekst_choose_source
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

; W returns to network discovery without resetting the cartridge or requiring
; the user to power-cycle the Pico. A successful connection starts again on
; page 100 and offers to replace the saved profile when appropriate.
teletekst_change_wifi:
        ld a,1
        ld (wifi_cancel_enabled),a
        xor a
        ld (teletekst_rotation_enabled),a
        ld (teletekst_input_count),a
        call MONITOR_CLEAR_KEY
        jp wifi_setup

wifi_cancel_to_source:
        xor a
        ld (wifi_cancel_enabled),a
        ld (teletekst_auto_page_enabled),a
        ld (teletekst_auto_retry_pending),a
        ld (teletekst_rotation_enabled),a
        ld (teletekst_rotation_paused),a
        ld (teletekst_input_count),a
        ld (teletekst_subpage),a
        ld (teletekst_cycle_started),a
        call erase_password
        call MONITOR_CLEAR_KEY
        call teletekst_choose_source
        ld hl,100
        ld (teletekst_page),hl
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

; A displayed page remains interactive. Three digits select a new page without
; Enter, just like a television Teletekst receiver. The monitor's 20 ms clock
; drives subpage rotation while its interrupt-owned FIFO supplies debounced
; keys without blocking this loop.
teletekst_main_loop:
        call teletekst_clock_update
        call try_read_key
        jp z,teletekst_check_rotation
        cp KEY_STOP_EVENT
        jp z,teletekst_change_source
        cp 'W'
        jp z,teletekst_change_wifi
        cp 'w'
        jp z,teletekst_change_wifi
        cp KEY_START_EVENT
        jp z,teletekst_show_index
        cp 'I'
        jp z,teletekst_show_index
        cp 'i'
        jp z,teletekst_show_index
        cp 'R'
        jp z,teletekst_toggle_reveal
        cp 'r'
        jp z,teletekst_toggle_reveal
        cp '?'
        jp z,teletekst_toggle_reveal
        cp 'Z'
        jp z,teletekst_toggle_zoom
        cp 'z'
        jp z,teletekst_toggle_zoom
        cp 'P'
        jp z,teletekst_previous_page_key
        cp 'p'
        jp z,teletekst_previous_page_key
        cp KEY_LEFT_EVENT
        jp z,teletekst_previous_page_key
        cp 'N'
        jp z,teletekst_following_page
        cp 'n'
        jp z,teletekst_following_page
        cp KEY_RIGHT_EVENT
        jp z,teletekst_following_page
        cp 'V'
        jp z,teletekst_toggle_auto_page
        cp 'v'
        jp z,teletekst_toggle_auto_page
        cp 'A'
        jp z,teletekst_toggle_rotation
        cp 'a'
        jp z,teletekst_toggle_rotation
        cp 'S'
        jp z,teletekst_select_subpage
        cp 's'
        jp z,teletekst_select_subpage
        cp 'H'
        jp z,teletekst_show_help
        cp 'h'
        jp z,teletekst_show_help
        cp 008h
        jp z,teletekst_page_backspace
        cp '0'
        jp c,teletekst_main_loop
        cp '9'+1
        jp nc,teletekst_main_loop
        ld c,a
        ld a,(teletekst_input_count)
        or a
        jr nz,teletekst_store_digit
        ; Remove all four cells in the top-right navigation area before the
        ; first new digit. This clears both the old three-digit page and any
        ; adjacent digit left by a differently aligned provider header.
        call teletekst_clear_header_input
        ld a,c
        cp '1'
        jp c,teletekst_main_loop
        cp '8'+1
        jp nc,teletekst_main_loop
teletekst_store_digit:
        ld a,(teletekst_input_count)
        ld e,a
        ld d,0
        ld hl,TELETEXT_INPUT_BUFFER
        add hl,de
        ld a,c
        ld (hl),a
        ld hl,VIDEO_RAM+36
        add hl,de
        ld (hl),a
        ld a,(teletekst_input_count)
        inc a
        ld (teletekst_input_count),a
        cp 3
        jp nz,teletekst_main_loop

        call teletekst_accept_input
        xor a
        ld (teletekst_input_count),a
        ld (teletekst_subpage),a
        ld (teletekst_cycle_started),a
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

teletekst_page_backspace:
        ld a,(teletekst_input_count)
        or a
        jp z,teletekst_main_loop
        dec a
        ld (teletekst_input_count),a
        ld e,a
        ld d,0
        ld hl,VIDEO_RAM+36
        add hl,de
        ld (hl),020h
        jp teletekst_main_loop

teletekst_show_index:
        ld hl,100
        jr teletekst_navigate_to_hl

teletekst_previous_page_key:
        ld hl,(teletekst_previous_page)
        ld a,h
        or l
        jp z,teletekst_main_loop
        jr teletekst_navigate_to_hl

teletekst_following_page:
        ld hl,(teletekst_next_page)
        ld a,h
        or l
        jp z,teletekst_main_loop
teletekst_navigate_to_hl:
        ld (teletekst_page),hl
        xor a
        ld (teletekst_subpage),a
        ld (teletekst_input_count),a
        ld (teletekst_cycle_started),a
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

; V enables or disables unattended movement to the next available page. Pages
; with subpages finish their current subpage sequence before moving on.
teletekst_toggle_auto_page:
        ld a,(teletekst_auto_page_enabled)
        xor 1
        ld (teletekst_auto_page_enabled),a
        or a
        jr z,teletekst_auto_page_disabled
        call teletekst_schedule_rotation
        call teletekst_draw_auto_page_indicator
        jp teletekst_main_loop
teletekst_auto_page_disabled:
        xor a
        ld (teletekst_auto_retry_pending),a
        call teletekst_restore_auto_page_indicator
        call teletekst_can_rotate
        jp z,teletekst_disable_rotation_and_loop
        call teletekst_schedule_rotation
        jp teletekst_main_loop
teletekst_disable_rotation_and_loop:
        xor a
        ld (teletekst_rotation_enabled),a
        jp teletekst_main_loop

; R reveals SAA5050 concealed text without modifying the provider's cached
; bytes. Z cycles normal, enlarged top half, enlarged bottom half, then normal.
teletekst_toggle_reveal:
        ld a,(teletekst_page_valid)
        or a
        jp z,teletekst_main_loop
        ld a,(teletekst_reveal_enabled)
        xor 1
        ld (teletekst_reveal_enabled),a
        call teletekst_render_screen
        ld a,(teletekst_zoom_state)
        or a
        jr nz,teletekst_toggle_reveal_commit_screen
        call teletekst_commit_reveal
        jp teletekst_restore_pause_and_loop
teletekst_toggle_reveal_commit_screen:
        call teletekst_commit_screen
        jp teletekst_restore_pause_and_loop

teletekst_toggle_zoom:
        ld a,(teletekst_page_valid)
        or a
        jp z,teletekst_main_loop
        ld a,(teletekst_zoom_state)
        inc a
        cp 3
        jr c,teletekst_zoom_store
        xor a
teletekst_zoom_store:
        ld (teletekst_zoom_state),a
        call teletekst_render_screen
        call teletekst_commit_screen
teletekst_restore_pause_and_loop:
        call teletekst_draw_status_indicators
        jp teletekst_main_loop

; A toggles automatic subpage cycling. Resuming starts a fresh ten-second
; interval; pages without a reported successor remain stationary.
teletekst_toggle_rotation:
        ld a,(teletekst_rotation_paused)
        xor 1
        ld (teletekst_rotation_paused),a
        or a
        jr nz,teletekst_pause_rotation
        ld a,(teletekst_pause_saved_cell)
        ld (VIDEO_RAM+39),a
        call teletekst_can_rotate
        jp z,teletekst_main_loop
        ld hl,(MONITOR_CLOCK)
        ld de,TELETEKST_ROTATE_TICKS
        add hl,de
        ld (teletekst_rotation_deadline),hl
        ld a,1
        ld (teletekst_rotation_enabled),a
        jp teletekst_main_loop
teletekst_pause_rotation:
        call teletekst_can_rotate
        jr z,teletekst_pause_rotation_disable
        call teletekst_schedule_rotation
        jr teletekst_pause_rotation_draw
teletekst_pause_rotation_disable:
        xor a
        ld (teletekst_rotation_enabled),a
teletekst_pause_rotation_draw:
        call teletekst_draw_pause_indicator
        jp teletekst_main_loop

; NZ means that a successor is known, or that a known sequence just reached
; its final sentinel and the next automatic request must wrap to subpage zero.
teletekst_can_rotate:
        ld a,(teletekst_auto_page_enabled)
        or a
        ret nz
        ld a,(teletekst_rotation_paused)
        or a
        jr z,teletekst_can_rotate_subpage
        xor a
        ret
teletekst_can_rotate_subpage:
        ld a,(teletekst_next_subpage)
        or a
        ret nz
        ld a,(teletekst_cycle_started)
        or a
        ret

teletekst_schedule_rotation:
        ld hl,(MONITOR_CLOCK)
        ld de,TELETEKST_ROTATE_TICKS
        add hl,de
        ld (teletekst_rotation_deadline),hl
        ld a,1
        ld (teletekst_rotation_enabled),a
        ret

; S followed by two digits selects subpage 00-99. One digit followed by Enter
; selects subpage 0-9. Zero asks the API for its default first subpage. An
; explicit choice also pauses automatic cycling so the selected subpage stays
; on screen until the user presses A.
teletekst_select_subpage:
        xor a
        ld (teletekst_rotation_enabled),a
        call teletekst_clear_header_input
        ld a,'S'
        ld (VIDEO_RAM+36),a
        ld a,':'
        ld (VIDEO_RAM+37),a
teletekst_subpage_first_digit:
        call read_key
        cp '0'
        jr c,teletekst_subpage_first_digit
        cp '9'+1
        jr nc,teletekst_subpage_first_digit
        ld (TELETEXT_SUBPAGE_INPUT),a
        ld (VIDEO_RAM+38),a
teletekst_subpage_second_digit:
        call read_key
        cp 00dh
        jr z,teletekst_subpage_single_digit
        cp '0'
        jr c,teletekst_subpage_second_digit
        cp '9'+1
        jr nc,teletekst_subpage_second_digit
        ld (TELETEXT_SUBPAGE_INPUT+1),a
        ld (VIDEO_RAM+39),a

        ; Convert the two entered digits to 0-99.
        ld a,(TELETEXT_SUBPAGE_INPUT)
        sub '0'
        ld b,a
        or a
        jr z,teletekst_subpage_add_ones
        xor a
teletekst_subpage_tens_loop:
        add a,10
        djnz teletekst_subpage_tens_loop
teletekst_subpage_add_ones:
        ld b,a
        ld a,(TELETEXT_SUBPAGE_INPUT+1)
        sub '0'
        add a,b
        jr teletekst_subpage_selected
teletekst_subpage_single_digit:
        ld a,(TELETEXT_SUBPAGE_INPUT)
        sub '0'
teletekst_subpage_selected:
        ld (teletekst_subpage),a
        ld a,1
        ld (teletekst_rotation_paused),a
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

teletekst_clear_header_input:
        ld hl,VIDEO_RAM+36
        ld b,4
        ld a,020h
teletekst_clear_header_input_loop:
        ld (hl),a
        inc hl
        djnz teletekst_clear_header_input_loop
        ret

; H opens an entirely cartridge-resident help page. Preserve the exact current
; display (without the transient pause marker), then restore it without another
; Internet request after any key is pressed.
teletekst_show_help:
        xor a
        ld (teletekst_help_return_source),a
        ld (teletekst_rotation_enabled),a
        ld a,(teletekst_auto_page_enabled)
        or a
        call nz,teletekst_restore_auto_page_indicator
        ld a,(teletekst_rotation_paused)
        or a
        jr z,teletekst_help_save_screen
        ld a,(teletekst_pause_saved_cell)
        ld (VIDEO_RAM+39),a
        jr teletekst_help_save_screen
teletekst_show_help_from_source:
        ld a,1
        ld (teletekst_help_return_source),a
teletekst_help_save_screen:
        call teletekst_save_current_screen
        call clear_screen
        ld hl,help_header_text
        ld de,VIDEO_RAM
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,help_intro_text
        ld de,VIDEO_RAM+160
        call write_string
        ld hl,help_page_title_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,help_page_entry_text
        ld de,VIDEO_RAM+400
        call write_string
        ld hl,help_index_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,help_browse_text
        ld de,VIDEO_RAM+560
        call write_string
        ld hl,help_auto_page_text
        ld de,VIDEO_RAM+640
        call write_string
        ld hl,help_display_title_text
        ld de,VIDEO_RAM+720
        call write_string
        ld hl,help_reveal_text
        ld de,VIDEO_RAM+800
        call write_string
        ld hl,help_zoom_text
        ld de,VIDEO_RAM+880
        call write_string
        ld hl,help_subpage_title_text
        ld de,VIDEO_RAM+960
        call write_string
        ld hl,help_subpage_select_text
        ld de,VIDEO_RAM+1040
        call write_string
        ld hl,help_pause_text
        ld de,VIDEO_RAM+1120
        call write_string
        ld hl,help_wifi_text
        ld de,VIDEO_RAM+1200
        call write_string
        ld hl,help_source_text
        ld de,VIDEO_RAM+1280
        call write_string
        ld hl,help_help_text
        ld de,VIDEO_RAM+1360
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+1600
        call write_string
        ld hl,help_return_text
        ld de,VIDEO_RAM+1760
        call write_string
        call MONITOR_CLEAR_KEY
        call read_key
        call teletekst_commit_screen

        ld a,(teletekst_help_return_source)
        or a
        jp nz,teletekst_choose_source_key
teletekst_help_resume_rotation:
        call teletekst_can_rotate
        call nz,teletekst_schedule_rotation
        call teletekst_draw_status_indicators
        jp teletekst_main_loop

; Pack the 40 visible cells from each 80-byte P2000T row into the same buffer
; used by the fetch staging path.
teletekst_save_current_screen:
        ld hl,VIDEO_RAM
        ld de,TELETEXT_SCREEN_BUFFER
        ld a,24
teletekst_save_current_row:
        push af
        ld bc,40
        ldir
        ld bc,40
        add hl,bc
        pop af
        dec a
        jr nz,teletekst_save_current_row
        ret

teletekst_check_rotation:
        ld a,(teletekst_input_count)
        or a
        jp nz,teletekst_main_loop
        ld a,(teletekst_rotation_enabled)
        or a
        jp z,teletekst_main_loop
        ld hl,(MONITOR_CLOCK)
        ld de,(teletekst_rotation_deadline)
        or a
        sbc hl,de
        bit 7,h
        jp nz,teletekst_main_loop

        ld a,(teletekst_rotation_paused)
        or a
        jr nz,teletekst_check_auto_page
        ld a,(teletekst_next_subpage)
        or a
        jr z,teletekst_check_auto_page
        ld (teletekst_subpage),a
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        ld a,(teletekst_auto_page_enabled)
        or a
        jp nz,teletekst_auto_page_handle_error
        ; Keep the last complete page visible after a transient rotation
        ; failure. Typing another number remains available immediately.
        xor a
        ld (teletekst_rotation_enabled),a
        jp teletekst_main_loop

teletekst_check_auto_page:
        ld a,(teletekst_auto_page_enabled)
        or a
        jp z,teletekst_check_rotation_wrap
        ld a,(teletekst_auto_retry_pending)
        or a
        jr z,teletekst_auto_page_use_metadata
        xor a
        ld (teletekst_auto_retry_pending),a
        ld hl,(teletekst_page)
        jr teletekst_auto_page_navigate
teletekst_auto_page_use_metadata:
        ld hl,(teletekst_next_page)
        ld a,h
        or l
        jr nz,teletekst_auto_page_navigate
        ld hl,100
teletekst_auto_page_navigate:
        xor a
        ld (teletekst_cycle_started),a
        ld (teletekst_subpage),a
        ld (teletekst_input_count),a
        ld (teletekst_page),hl
        call teletekst_fetch_page
        jp nc,teletekst_main_loop

teletekst_auto_page_handle_error:
        ; Keep unattended mode alive. Content failures skip to the following
        ; numeric page; transport failures retry the same page after a delay.
        ; Page 100 remains the health sentinel and always shows a real error.
        ld hl,(teletekst_page)
        ld de,100
        or a
        sbc hl,de
        jr z,teletekst_auto_page_terminal_error
        ld a,(teletekst_error_code)
        cp TELETEKST_ERROR_HTTP_STATUS
        jr z,teletekst_auto_page_skip
        cp TELETEKST_ERROR_TOO_LARGE
        jr z,teletekst_auto_page_skip
        cp TELETEKST_ERROR_INVALID_DATA
        jr z,teletekst_auto_page_skip
        cp TELETEKST_ERROR_PAGE_NOT_FOUND
        jr z,teletekst_auto_page_skip
        jr teletekst_auto_page_retry
teletekst_auto_page_skip:
        ld hl,(teletekst_page)
        inc hl
        push hl
        ld de,900
        or a
        sbc hl,de
        pop hl
        jr c,teletekst_auto_page_store_retry
        ld hl,100
        jr teletekst_auto_page_store_retry
teletekst_auto_page_retry:
        ld hl,(teletekst_page)
teletekst_auto_page_store_retry:
        ld (teletekst_page),hl
        ld a,1
        ld (teletekst_auto_retry_pending),a
        ld (teletekst_page_valid),a
        call teletekst_schedule_rotation
        jp teletekst_main_loop
teletekst_auto_page_terminal_error:
        xor a
        ld (teletekst_auto_page_enabled),a
        ld (teletekst_auto_retry_pending),a
        call show_teletekst_error
        jp teletekst_main_loop
teletekst_check_rotation_wrap:
        xor a
        ld (teletekst_subpage),a
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        xor a
        ld (teletekst_rotation_enabled),a
        jp teletekst_main_loop

; Convert the three ASCII digits into the selected 16-bit page number.
teletekst_accept_input:
        ld hl,0
        ld a,(TELETEXT_INPUT_BUFFER)
        sub '0'
        ld b,a
        ld de,100
teletekst_hundreds_loop:
        add hl,de
        djnz teletekst_hundreds_loop
        ld a,(TELETEXT_INPUT_BUFFER+1)
        sub '0'
        ld b,a
        or a
        jr z,teletekst_ones
        ld de,10
teletekst_tens_loop:
        add hl,de
        djnz teletekst_tens_loop
teletekst_ones:
        ld a,(TELETEXT_INPUT_BUFFER+2)
        sub '0'
        ld e,a
        ld d,0
        add hl,de
        ld (teletekst_page),hl
        ret

; Start an asynchronous API request, poll it, then retrieve four validated
; chunks of six 40-byte rows. Carry reports a fetch/protocol error.
teletekst_fetch_page:
        call teletekst_indicator_begin
        xor a
        ld (teletekst_http_result),a
        ld (teletekst_lwip_error),a
        ld (teletekst_http_status),a
        ld (teletekst_http_status+1),a
        ld a,P2WP_TYPE_TELETEKST_FETCH_START
        call prepare_request
        ld hl,(teletekst_page)
        ld (FRAME_BUFFER+6),hl
        ld a,(teletekst_subpage)
        ld (FRAME_BUFFER+8),a
        ld a,(teletekst_source)
        ld (FRAME_BUFFER+9),a
        cp TELETEKST_SOURCE_CUSTOM
        jr nz,teletekst_fetch_builtin_length
        ld a,(p2wp_session_version)
        cp 4
        jp c,teletekst_fetch_failed
        ld a,(custom_url_length)
        ld (FRAME_BUFFER+10),a
        ld c,a
        ld b,0
        ld hl,CUSTOM_URL_BUFFER
        ld de,FRAME_BUFFER+11
        ldir
        ld a,(custom_url_length)
        add a,5
        ld (FRAME_BUFFER+4),a
        add a,6
        ld l,a
        ld h,0
        ld (body_length),hl
        jr teletekst_fetch_request_ready
teletekst_fetch_builtin_length:
        ld a,4
        ld (FRAME_BUFFER+4),a
        ld hl,10
        ld (body_length),hl
teletekst_fetch_request_ready:
        call transact
        jp c,teletekst_fetch_failed
        call validate_empty_response
        jp c,teletekst_fetch_failed
        call next_sequence

teletekst_fetch_poll:
        call poll_delay
        call teletekst_indicator_next
        ld a,P2WP_TYPE_TELETEKST_FETCH_STATUS
        call prepare_request
        call transact
        jp c,teletekst_fetch_failed
        ld a,(RX_BUFFER+5)
        or a
        jp nz,teletekst_fetch_failed
        ld a,(RX_BUFFER+4)
        ld (teletekst_status_length),a
        ld b,a
        ld a,(p2wp_session_version)
        cp 4
        ld a,b
        jr nc,teletekst_fetch_poll_v4_length
        ld a,(p2wp_session_version)
        cp 3
        ld a,b
        jr z,teletekst_fetch_poll_v3_length
        cp 5
        jr z,teletekst_fetch_poll_length_ok
        cp 9
        jr z,teletekst_fetch_poll_length_ok
        cp 13
        jp nz,teletekst_fetch_failed
        jr teletekst_fetch_poll_length_ok
teletekst_fetch_poll_v3_length:
        cp 13
        jp nz,teletekst_fetch_failed
        jr teletekst_fetch_poll_length_ok
teletekst_fetch_poll_v4_length:
        ld a,(p2wp_session_version)
        cp 7
        ld a,b
        jr c,teletekst_fetch_poll_v4_legacy_length
        cp 21
        jp nz,teletekst_fetch_failed
        jr teletekst_fetch_poll_length_ok
teletekst_fetch_poll_v4_legacy_length:
        cp 17
        jp nz,teletekst_fetch_failed
teletekst_fetch_poll_length_ok:
        call next_sequence
        ld a,(RX_BUFFER+6)
        cp TELETEKST_CONNECTING
        jr z,teletekst_fetch_poll
        cp TELETEKST_RECEIVING
        jr z,teletekst_fetch_poll
        cp TELETEKST_COMPLETE
        jr z,teletekst_fetch_rows
        cp TELETEKST_FAILED
        jp nz,teletekst_fetch_failed
        ld a,(RX_BUFFER+7)
        ld (teletekst_error_code),a
        ld a,(p2wp_session_version)
        cp 7
        jr c,teletekst_fetch_error_details_ready
        ld a,(RX_BUFFER+23)
        ld (teletekst_http_result),a
        ld a,(RX_BUFFER+24)
        ld (teletekst_lwip_error),a
        ld hl,(RX_BUFFER+25)
        ld (teletekst_http_status),hl
teletekst_fetch_error_details_ready:
        xor a
        ld (teletekst_page_valid),a
        call teletekst_indicator_restore
        scf
        ret

teletekst_fetch_rows:
        ld a,(RX_BUFFER+10)
        ld (teletekst_next_subpage),a
        ld hl,0
        ld (teletekst_previous_page),hl
        ld (teletekst_next_page),hl
        ld a,(teletekst_status_length)
        cp 17
        jr c,teletekst_fetch_rows_navigation_ready
        ld hl,(RX_BUFFER+19)
        ld (teletekst_previous_page),hl
        ld hl,(RX_BUFFER+21)
        ld (teletekst_next_page),hl
teletekst_fetch_rows_navigation_ready:
        xor a
        ld (teletekst_clock_valid),a
        ld (teletekst_clock_has_date),a
        ld a,(teletekst_status_length)
        cp 5
        jr z,teletekst_fetch_rows_no_clock
        ld a,(RX_BUFFER+14)
        or a
        jr z,teletekst_fetch_rows_no_clock
        ld a,(RX_BUFFER+11)
        ld (teletekst_clock_hours),a
        ld a,(RX_BUFFER+12)
        ld (teletekst_clock_minutes),a
        ld a,(RX_BUFFER+13)
        ld (teletekst_clock_seconds),a
        ld a,(teletekst_status_length)
        cp 13
        jr c,teletekst_fetch_rows_clock_ready
        ld a,(RX_BUFFER+15)
        ld (teletekst_clock_day),a
        ld a,(RX_BUFFER+16)
        ld (teletekst_clock_month),a
        ld a,(RX_BUFFER+17)
        ld (teletekst_clock_year),a
        ld a,(RX_BUFFER+18)
        ld (teletekst_clock_weekday),a
        ld a,1
        ld (teletekst_clock_has_date),a
teletekst_fetch_rows_clock_ready:
        ld hl,(MONITOR_CLOCK)
        ld (teletekst_clock_last_tick),hl
        ld a,1
        ld (teletekst_clock_valid),a
        xor a
        ld (teletekst_clock_blink_phase),a
teletekst_fetch_rows_no_clock:
        ld hl,TELETEXT_RAW_SCREEN_BUFFER
        ld (teletekst_screen_pointer),hl
        xor a
        ld (teletekst_chunk),a

teletekst_chunk_loop:
        ld a,P2WP_TYPE_TELETEKST_FETCH_ROWS
        call prepare_request
        ld a,(teletekst_chunk)
        ld (FRAME_BUFFER+6),a
        ld a,1
        ld (FRAME_BUFFER+4),a
        ld hl,7
        ld (body_length),hl
        call transact
        jp c,teletekst_fetch_failed
        ld a,(RX_BUFFER+4)
        cp TELETEKST_CHUNK_SIZE
        jp nz,teletekst_fetch_failed
        ld a,(RX_BUFFER+5)
        or a
        jp nz,teletekst_fetch_failed
        call teletekst_copy_chunk
        call next_sequence
        ld a,(teletekst_chunk)
        inc a
        ld (teletekst_chunk),a
        cp TELETEKST_CHUNK_COUNT
        jr nz,teletekst_chunk_loop

        call teletekst_draw_clock_buffer
        xor a
        ld (teletekst_reveal_enabled),a
        ld (teletekst_zoom_state),a
        inc a
        ld (teletekst_page_valid),a
        xor a
        ld (teletekst_auto_retry_pending),a
        call teletekst_render_screen
        call teletekst_commit_screen
        ld a,(teletekst_next_subpage)
        or a
        jr z,teletekst_fetch_rotation_check
        ld a,1
        ld (teletekst_cycle_started),a
teletekst_fetch_rotation_check:
        call teletekst_can_rotate
        jr z,teletekst_fetch_rotation_disabled
teletekst_fetch_schedule_rotation:
        call teletekst_schedule_rotation
        call teletekst_draw_status_indicators
        or a
        ret
teletekst_fetch_rotation_disabled:
        xor a
        ld (teletekst_rotation_enabled),a
        call teletekst_draw_status_indicators
        or a
        ret

teletekst_draw_status_indicators:
        ld a,(teletekst_rotation_paused)
        or a
        call nz,teletekst_draw_pause_indicator
        ld a,(teletekst_auto_page_enabled)
        or a
        call nz,teletekst_draw_auto_page_indicator
        ret

; Save the cell hidden by the pause marker so resuming can restore the exact
; provider/error header byte rather than assuming it was a space or digit.
teletekst_draw_pause_indicator:
        ld a,(VIDEO_RAM+39)
        ld (teletekst_pause_saved_cell),a
        ld a,'A'
        ld (VIDEO_RAM+39),a
        ret

teletekst_draw_auto_page_indicator:
        ld a,(VIDEO_RAM+35)
        ld (teletekst_auto_page_saved_cell),a
        ld a,'V'
        ld (VIDEO_RAM+35),a
        ret

teletekst_restore_auto_page_indicator:
        ld a,(teletekst_auto_page_saved_cell)
        ld (VIDEO_RAM+35),a
        ret

teletekst_fetch_failed:
        xor a
        ld (teletekst_page_valid),a
        ld a,0ffh
        ld (teletekst_error_code),a
        call teletekst_indicator_restore
        scf
        ret

; Stage six packed rows in RAM. The complete screen is committed only after
; all four chunks validate, leaving the previous page intact while loading.
teletekst_copy_chunk:
        ld hl,RX_BUFFER+6
        ld de,(teletekst_screen_pointer)
        ld bc,TELETEKST_CHUNK_SIZE
        ldir
        ld (teletekst_screen_pointer),de
        ret

; Rebuild the display staging buffer from the untouched provider bytes. Reveal
; replaces conceal controls with the active foreground control. Zoom uses the
; SAA5050 double-height mode and places twelve source rows in alternate P2000T
; memory rows, which the hardware expands into 24 visible rows.
teletekst_render_screen:
        ld a,(teletekst_zoom_state)
        or a
        jr nz,teletekst_render_zoom
        ld hl,TELETEXT_RAW_SCREEN_BUFFER
        ld de,TELETEXT_SCREEN_BUFFER
        ld a,24
teletekst_render_normal_row:
        push af
        ld a,SAA5050_ALPHA_WHITE
        ld (teletekst_render_colour),a
        ld b,40
        call teletekst_render_copy_row
        pop af
        dec a
        jr nz,teletekst_render_normal_row
        ret

teletekst_render_zoom:
        push af
        ld hl,TELETEXT_SCREEN_BUFFER
        ld (hl),020h
        ld de,TELETEXT_SCREEN_BUFFER+1
        ld bc,TELETEKST_CHUNK_SIZE*TELETEKST_CHUNK_COUNT-1
        ldir
        pop af
        ld hl,TELETEXT_RAW_SCREEN_BUFFER
        cp 2
        jr nz,teletekst_render_zoom_source_ready
        ld de,480
        add hl,de
teletekst_render_zoom_source_ready:
        ld de,TELETEXT_SCREEN_BUFFER
        ld a,12
teletekst_render_zoom_row:
        push af
        ld a,00dh
        ld (de),a
        inc de
        ld a,SAA5050_ALPHA_WHITE
        ld (teletekst_render_colour),a
        ld b,39
        call teletekst_render_copy_row
        inc hl
        push hl
        ld hl,40
        add hl,de
        ex de,hl
        pop hl
        pop af
        dec a
        jr nz,teletekst_render_zoom_row
        ret

teletekst_render_copy_row:
        ld a,(hl)
        inc hl
        ld c,a
        and 07fh
        cp 1
        jr c,teletekst_render_check_graphics
        cp 8
        jr nc,teletekst_render_check_graphics
        ld (teletekst_render_colour),a
        jr teletekst_render_store
teletekst_render_check_graphics:
        cp 011h
        jr c,teletekst_render_check_conceal
        cp 018h
        jr nc,teletekst_render_check_conceal
        ld (teletekst_render_colour),a
        jr teletekst_render_store
teletekst_render_check_conceal:
        cp 018h
        jr nz,teletekst_render_store
        ld a,(teletekst_reveal_enabled)
        or a
        jr z,teletekst_render_store
        ld a,(teletekst_render_colour)
        ld c,a
teletekst_render_store:
        ld a,c
        ld (de),a
        inc de
        djnz teletekst_render_copy_row
        ret

; Reveal changes only the conceal controls in a normal-size page. Updating
; those few video bytes in place avoids blanking and copying the complete
; screen, so the key has no visible flicker on physical or emulated hardware.
; The untouched raw buffer remains authoritative when conceal is restored.
teletekst_commit_reveal:
        ld hl,TELETEXT_RAW_SCREEN_BUFFER
        ld de,VIDEO_RAM
        ld a,24
teletekst_commit_reveal_row:
        push af
        ld c,SAA5050_ALPHA_WHITE
        ld b,40
teletekst_commit_reveal_column:
        ld a,(hl)
        and 07fh
        cp 1
        jr c,teletekst_commit_reveal_check_graphics
        cp 8
        jr nc,teletekst_commit_reveal_check_graphics
        ld c,a
        jr teletekst_commit_reveal_next
teletekst_commit_reveal_check_graphics:
        cp 011h
        jr c,teletekst_commit_reveal_check_conceal
        cp 018h
        jr nc,teletekst_commit_reveal_check_conceal
        ld c,a
        jr teletekst_commit_reveal_next
teletekst_commit_reveal_check_conceal:
        cp 018h
        jr nz,teletekst_commit_reveal_next
        ld a,(teletekst_reveal_enabled)
        or a
        jr nz,teletekst_commit_reveal_store_colour
        ld a,(hl)
        jr teletekst_commit_reveal_store
teletekst_commit_reveal_store_colour:
        ld a,c
teletekst_commit_reveal_store:
        ld (de),a
teletekst_commit_reveal_next:
        inc hl
        inc de
        djnz teletekst_commit_reveal_column
        push hl
        ld hl,40
        add hl,de
        ex de,hl
        pop hl
        pop af
        dec a
        jr nz,teletekst_commit_reveal_row
        ret

; Copy a complete staged page into the visible 40 columns of the P2000T's
; 80-byte video rows. The copy takes longer than vertical blank, so blank the
; display for one complete field: switch it off just after one retrace and
; restore it just after the next. Bit 7 of port 30h is video disable; the lower
; seven scroll bits remain zero, matching this cartridge's physical layout.
teletekst_commit_screen:
        call wait_for_vsync
        ld a,080h
        out (VIDEO_CONTROL_PORT),a
        ld hl,TELETEXT_SCREEN_BUFFER
        ld de,VIDEO_RAM
        ld a,24
teletekst_commit_row:
        push af
        ld bc,40
        ldir
        push hl
        ld hl,40
        add hl,de
        ex de,hl
        pop hl
        pop af
        dec a
        jr nz,teletekst_commit_row
        call wait_for_vsync
        xor a
        out (VIDEO_CONTROL_PORT),a
        ret

; The Pico supplies Dutch local seconds since midnight after a successful NTP
; sync. The monitor's 20 ms tick advances that value between page fetches, so
; the top-row clock remains live without repeated network traffic.
teletekst_clock_update:
        ld a,(teletekst_clock_valid)
        or a
        ret z
        ld hl,(MONITOR_CLOCK)
        ld de,(teletekst_clock_last_tick)
        or a
        sbc hl,de
        ld a,h
        or a
        jr nz,teletekst_clock_tick
        ld a,l
        cp TELETEKST_BLINK_TICKS
        ret c
teletekst_clock_tick:
        ld hl,(MONITOR_CLOCK)
        ld (teletekst_clock_last_tick),hl
        ld a,(teletekst_clock_blink_phase)
        xor 1
        ld (teletekst_clock_blink_phase),a
        jr nz,teletekst_clock_saved
        ld a,(teletekst_clock_seconds)
        inc a
        cp 60
        jr c,teletekst_clock_store_seconds
        xor a
        ld (teletekst_clock_seconds),a
        ld a,(teletekst_clock_minutes)
        inc a
        cp 60
        jr c,teletekst_clock_store_minutes
        xor a
        ld (teletekst_clock_minutes),a
        ld a,(teletekst_clock_hours)
        inc a
        cp 24
        jr c,teletekst_clock_store_hours
        xor a
        ld (teletekst_clock_hours),a
        ld a,(teletekst_clock_has_date)
        or a
        jr z,teletekst_clock_saved
        call teletekst_clock_increment_date
        jr teletekst_clock_saved
teletekst_clock_store_hours:
        ld (teletekst_clock_hours),a
        jr teletekst_clock_saved
teletekst_clock_store_minutes:
        ld (teletekst_clock_minutes),a
        jr teletekst_clock_saved
teletekst_clock_store_seconds:
        ld (teletekst_clock_seconds),a
teletekst_clock_saved:
        ld a,(teletekst_rotation_paused)
        or a
        ret nz
        ld a,(teletekst_input_count)
        or a
        ret nz
        ld a,(teletekst_page_valid)
        or a
        jp z,teletekst_write_clock_right
        ld a,(teletekst_zoom_state)
        or a
        ret nz
        ld de,TELETEXT_RAW_SCREEN_BUFFER+1
        call teletekst_write_clock
        ld de,TELETEXT_SCREEN_BUFFER+1
        call teletekst_write_clock
        ld de,VIDEO_RAM+1
        jp teletekst_write_clock

; Error pages keep their title at the left and right-align the live clock.
; DE points at the first clock glyph; teletekst_write_clock uses the cell just
; before it for the yellow spacing attribute.
teletekst_write_clock_right:
        ld a,(teletekst_clock_valid)
        or a
        ret z
        ld a,(teletekst_clock_has_date)
        or a
        jr z,teletekst_write_clock_right_time_only
        ld de,VIDEO_RAM+22
        ld a,(teletekst_source)
        cp TELETEKST_SOURCE_P2000T
        jr nz,teletekst_write_clock_right_ready
        ld de,VIDEO_RAM+25
        jr teletekst_write_clock_right_ready
teletekst_write_clock_right_time_only:
        ld de,VIDEO_RAM+32
        ld a,(teletekst_source)
        cp TELETEKST_SOURCE_P2000T
        jr nz,teletekst_write_clock_right_ready
        ld de,VIDEO_RAM+35
teletekst_write_clock_right_ready:
        jp teletekst_write_clock

; Advance the locally cached calendar date after the clock crosses midnight.
teletekst_clock_increment_date:
        ld a,(teletekst_clock_weekday)
        inc a
        cp 7
        jr c,teletekst_clock_store_weekday
        xor a
teletekst_clock_store_weekday:
        ld (teletekst_clock_weekday),a
        ld a,(teletekst_clock_month)
        dec a
        ld e,a
        ld d,0
        ld hl,teletekst_month_days
        add hl,de
        ld b,(hl)
        ld a,(teletekst_clock_month)
        cp 2
        jr nz,teletekst_clock_date_length_ready
        ld a,(teletekst_clock_year)
        cp 100
        jr z,teletekst_clock_date_length_ready
        cp 200
        jr z,teletekst_clock_date_length_ready
        and 3
        jr nz,teletekst_clock_date_length_ready
        inc b
teletekst_clock_date_length_ready:
        ld a,(teletekst_clock_day)
        inc a
        cp b
        jr c,teletekst_clock_store_day
        jr z,teletekst_clock_store_day
        ld a,1
        ld (teletekst_clock_day),a
        ld a,(teletekst_clock_month)
        inc a
        cp 13
        jr c,teletekst_clock_store_month
        ld a,1
        ld (teletekst_clock_month),a
        ld a,(teletekst_clock_year)
        inc a
        ld (teletekst_clock_year),a
        ret
teletekst_clock_store_month:
        ld (teletekst_clock_month),a
        ret
teletekst_clock_store_day:
        ld (teletekst_clock_day),a
        ret

teletekst_month_days:
        defb 31,28,31,30,31,30,31,31,30,31,30,31

; Render the current clock into the staged page before its atomic display
; commit. This is the common top-banner treatment for both content sources.
teletekst_draw_clock_buffer:
        ld a,(teletekst_clock_valid)
        or a
        ret z
        ld de,TELETEXT_RAW_SCREEN_BUFFER+1
        ; fall through
teletekst_write_clock:
        ld a,SAA5050_ALPHA_YELLOW
        dec de
        ld (de),a
        inc de
        ; fall through
teletekst_write_clock_style:
        ld a,(teletekst_clock_has_date)
        or a
        jr z,teletekst_write_clock_hours
        ld a,(teletekst_clock_weekday)
        add a,a
        ld c,a
        ld b,0
        ld hl,teletekst_weekday_names
        add hl,bc
        ld bc,2
        ldir
        ld a,' '
        ld (de),a
        inc de
        ld a,(teletekst_clock_day)
        call teletekst_write_two_digits
        ld a,'.'
        ld (de),a
        inc de
        ld a,(teletekst_clock_month)
        dec a
        ld c,a
        add a,a
        add a,c
        ld c,a
        ld b,0
        ld hl,teletekst_month_names
        add hl,bc
        ld bc,3
        ldir
        ld a,' '
        ld (de),a
        inc de
teletekst_write_clock_hours:
        ld a,(teletekst_clock_hours)
        call teletekst_write_two_digits
        ld a,(teletekst_source)
        cp TELETEKST_SOURCE_P2000T
        jr nz,teletekst_write_clock_with_seconds
        ld a,(teletekst_clock_blink_phase)
        or a
        ld a,' '
        jr nz,teletekst_write_clock_blink_colon
        ld a,':'
teletekst_write_clock_blink_colon:
        ld (de),a
        inc de
        ld a,(teletekst_clock_minutes)
        jp teletekst_write_two_digits
teletekst_write_clock_with_seconds:
        ld a,':'
        ld (de),a
        inc de
        ld a,(teletekst_clock_minutes)
        call teletekst_write_two_digits
        ld a,':'
        ld (de),a
        inc de
        ld a,(teletekst_clock_seconds)
        ; fall through
teletekst_write_two_digits:
        ld b,0
teletekst_clock_tens:
        cp 10
        jr c,teletekst_clock_tens_done
        sub 10
        inc b
        jr teletekst_clock_tens
teletekst_clock_tens_done:
        push af
        ld a,b
        add a,'0'
        ld (de),a
        inc de
        pop af
        add a,'0'
        ld (de),a
        inc de
        ret

teletekst_month_names:
        defb "jan","feb","mrt","apr","mei","jun"
        defb "jul","aug","sep","okt","nov","dec"

teletekst_weekday_names:
        defb "zo","ma","di","wo","do","vr","za"

; The monitor ROM increments this clock from the video/CTC interrupt every
; vertical retrace. Only the low byte is needed to detect the next field.
wait_for_vsync:
        ld hl,MONITOR_CLOCK
        ld a,(hl)
wait_for_vsync_tick:
        cp (hl)
        jr z,wait_for_vsync_tick
        ret

; A graphics colour control must occupy column zero before an SAA5050 mosaic
; can be shown. Put the rotating block in column one, the leftmost physically
; possible mosaic position, then restore the normal graphics and alpha modes.
teletekst_indicator_begin:
        ld hl,VIDEO_RAM
        ld de,TELETEXT_INDICATOR_SAVED
        ld bc,4
        ldir
        xor a
        ld (teletekst_indicator_phase),a
        jp teletekst_indicator_draw

teletekst_indicator_next:
        ld a,(teletekst_indicator_phase)
        inc a
        cp 6
        jr c,teletekst_indicator_phase_ready
        xor a
teletekst_indicator_phase_ready:
        ld (teletekst_indicator_phase),a

teletekst_indicator_draw:
        ld e,a
        ld d,0
        ld hl,teletekst_indicator_frames
        add hl,de
        add hl,de
        add hl,de
        add hl,de
        ld de,VIDEO_RAM
        ld bc,4
        ldir
        ret

teletekst_indicator_restore:
        ld hl,TELETEXT_INDICATOR_SAVED
        ld de,VIDEO_RAM
        ld bc,4
        ldir
        ret

show_teletekst_error:
        ld a,(teletekst_error_code)
        cp TELETEKST_ERROR_PAGE_NOT_FOUND
        jp z,show_teletekst_not_found
        call clear_screen
        ld hl,teletekst_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,teletekst_unavailable_text
        ld de,VIDEO_RAM+160
        call write_string
        call show_selected_page
        ld hl,teletekst_error_text
        ld de,VIDEO_RAM+320
        call write_string
        ld de,VIDEO_RAM+320+10
        ld a,(teletekst_error_code)
        call write_hex_byte
        ld hl,teletekst_error_prefix_text
        ld de,VIDEO_RAM+400
        call write_string
        call teletekst_error_description
        ld de,VIDEO_RAM+400+6
        call write_string
        ld a,(p2wp_session_version)
        cp 7
        jr c,show_teletekst_error_no_details
        ld hl,teletekst_error_detail_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,(teletekst_http_status)
        ld de,VIDEO_RAM+480+13
        call write_page_number
        ld a,(teletekst_lwip_error)
        ld de,VIDEO_RAM+480+22
        call write_hex_byte
        ld a,(teletekst_http_result)
        ld de,VIDEO_RAM+480+29
        call write_hex_byte
show_teletekst_error_no_details:
        ld hl,teletekst_error_retry_text
        ld de,VIDEO_RAM+640
        call write_string
        xor a
        ld (teletekst_rotation_enabled),a
        call teletekst_draw_status_indicators
        ret

; Return a short Dutch explanation for every stable fetch error code.
teletekst_error_description:
        ld a,(teletekst_error_code)
        cp TELETEKST_ERROR_NOT_CONNECTED
        ld hl,teletekst_error_not_connected_text
        ret z
        cp TELETEKST_ERROR_TLS_CONFIG
        ld hl,teletekst_error_tls_config_text
        ret z
        cp TELETEKST_ERROR_REQUEST_START
        ld hl,teletekst_error_request_start_text
        ret z
        cp TELETEKST_ERROR_NETWORK
        ld hl,teletekst_error_network_text
        ret z
        cp TELETEKST_ERROR_HTTP_STATUS
        ld hl,teletekst_error_http_text
        ret z
        cp TELETEKST_ERROR_TOO_LARGE
        ld hl,teletekst_error_too_large_text
        ret z
        cp TELETEKST_ERROR_INVALID_DATA
        ld hl,teletekst_error_invalid_data_text
        ret z
        cp TELETEKST_ERROR_DNS
        ld hl,teletekst_error_dns_text
        ret z
        cp TELETEKST_ERROR_CONNECT
        ld hl,teletekst_error_connect_text
        ret z
        cp TELETEKST_ERROR_CONNECTION_CLOSED
        ld hl,teletekst_error_closed_text
        ret z
        cp TELETEKST_ERROR_TIMEOUT
        ld hl,teletekst_error_timeout_text
        ret z
        cp TELETEKST_ERROR_OUT_OF_MEMORY
        ld hl,teletekst_error_memory_text
        ret z
        cp TELETEKST_ERROR_CONTENT_LENGTH
        ld hl,teletekst_error_content_length_text
        ret z
        cp TELETEKST_ERROR_LOCAL_ABORT
        ld hl,teletekst_error_abort_text
        ret z
        ld hl,teletekst_error_unknown_text
        ret

; A missing page is an ordinary navigation result, not a protocol failure.
; Present it as a centered red Teletext panel and leave the normal three-digit
; page-entry loop active so the user can immediately choose another page.
show_teletekst_not_found:
        call wait_for_vsync
        ld a,080h
        out (VIDEO_CONTROL_PORT),a
        call clear_screen
        ld hl,page_not_found_header_text
        ld de,VIDEO_RAM
        call write_string
        ld hl,opening_p2000t_row_1
        ld de,VIDEO_RAM+80
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_2
        ld de,VIDEO_RAM+160
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_3
        ld de,VIDEO_RAM+240
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_4
        ld de,VIDEO_RAM+320
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_5
        ld de,VIDEO_RAM+400
        call write_blue_mosaic_pattern
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,page_not_found_masthead_text
        ld de,VIDEO_RAM+560
        call write_string

        ld hl,page_not_found_blank_text
        ld de,VIDEO_RAM+720+2
        call write_string
        ld hl,page_not_found_title_text
        ld de,VIDEO_RAM+800+2
        call write_string
        ld hl,page_not_found_blank_text
        ld de,VIDEO_RAM+880+2
        call write_string
        ld hl,page_not_found_page_text
        ld de,VIDEO_RAM+960+2
        call write_string
        ld de,VIDEO_RAM+960+23
        ld hl,(teletekst_page)
        call write_page_number
        ld hl,page_not_found_message_text
        ld de,VIDEO_RAM+1040+2
        call write_string
        ld hl,page_not_found_blank_text
        ld de,VIDEO_RAM+1120+2
        call write_string
        ld hl,page_not_found_prompt_text
        ld de,VIDEO_RAM+1200+2
        call write_string
        ld hl,page_not_found_continue_text
        ld de,VIDEO_RAM+1280+2
        call write_string
        ld hl,page_not_found_blank_text
        ld de,VIDEO_RAM+1360+2
        call write_string

        call teletekst_write_clock_right

        call wait_for_vsync
        xor a
        out (VIDEO_CONTROL_PORT),a
        ld (teletekst_rotation_enabled),a
        call teletekst_draw_status_indicators
        ret

show_selected_page:
        ld hl,teletekst_page_text
        ld de,VIDEO_RAM+240
        call write_string
        ld de,VIDEO_RAM+240+8
        ld hl,(teletekst_page)
        call write_page_number
        ret

write_page_number:
        ld bc,100
        xor a
write_page_hundreds:
        or a
        sbc hl,bc
        jr c,write_page_hundreds_done
        inc a
        jr write_page_hundreds
write_page_hundreds_done:
        add hl,bc
        add a,'0'
        ld (de),a
        inc de
        ld bc,10
        xor a
write_page_tens:
        or a
        sbc hl,bc
        jr c,write_page_tens_done
        inc a
        jr write_page_tens
write_page_tens_done:
        add hl,bc
        add a,'0'
        ld (de),a
        inc de
        ld a,l
        add a,'0'
        ld (de),a
        ret

wifi_bad_password:
        ld hl,wifi_bad_password_text
        jr wifi_connection_retry

wifi_scan_failed:
        ld hl,wifi_scan_failed_text
        jr wifi_show_error
wifi_no_networks:
        ld hl,wifi_no_networks_text
        jr wifi_show_error
wifi_connect_failed:
        ld hl,wifi_connect_failed_text
wifi_connection_retry:
        push hl
        ld a,(wifi_connect_attempts_left)
        dec a
        ld (wifi_connect_attempts_left),a
        jr z,wifi_connection_retry_exhausted
        pop hl
        jp wifi_start_connect
wifi_connection_retry_exhausted:
        pop hl
        jr wifi_connection_recovery
wifi_protocol_failed:
        call erase_password
        ld hl,wifi_protocol_failed_text
wifi_show_error:
        push hl
        ld de,VIDEO_RAM+1280
        call clear_line
        pop hl
        ld de,VIDEO_RAM+1280
        call write_string
        jp fatal_loop

; Authentication and association failures are recoverable. R resends the
; password still held in session RAM; P wipes it and returns to entry. P is
; deliberately ignored for an open network.
wifi_connection_recovery:
        push hl
        ld de,VIDEO_RAM+1280
        call clear_line
        ld de,VIDEO_RAM+1360
        call clear_line
        pop hl
        ld de,VIDEO_RAM+1280
        call write_string
        ld hl,wifi_retry_text
        ld de,VIDEO_RAM+1360
        call write_string
wifi_connection_recovery_key:
        call read_key
        cp 'O'
        jp z,wifi_begin_connect
        cp 'o'
        jp z,wifi_begin_connect
        cp 'W'
        jr z,wifi_connection_reenter
        cp 'w'
        jr nz,wifi_connection_recovery_key
wifi_connection_reenter:
        ld a,(wifi_selected_index)
        ld e,a
        ld d,0
        ld hl,WIFI_SECURITY_LIST
        add hl,de
        ld a,(hl)
        cp WIFI_SECURITY_PSK
        jr nz,wifi_connection_recovery_key
        jp wifi_read_password

; Initialize a zero-payload request of type A and its transaction validators.
prepare_request:
        push af
        ld hl,request_header
        ld de,FRAME_BUFFER
        ld bc,6
        ldir
        pop af
        push af
        ld a,(p2wp_session_version)
        ld (FRAME_BUFFER),a
        pop af
        ld (FRAME_BUFFER+2),a
        ld (expected_type),a
        ld a,(sequence)
        ld (FRAME_BUFFER+3),a
        ld (expected_sequence),a
        ld hl,6
        ld (body_length),hl
        ret

request_header:
        defb P2WP_BOOTSTRAP_VERSION,0,0,0,0,0

next_sequence:
        ld a,(sequence)
        inc a
        ld (sequence),a
        ret

validate_empty_response:
        ld a,(RX_BUFFER+4)
        ld b,a
        ld a,(RX_BUFFER+5)
        or b
        ret z
        scf
        ret

validate_wifi_result:
        ld a,(RX_BUFFER+5)
        or a
        jr nz,wifi_result_bad
        ld a,(RX_BUFFER+4)
        cp 4
        jr c,wifi_result_bad
        cp 37
        jr nc,wifi_result_bad
        ld b,a
        ld a,(wifi_result_index)
        ld c,a
        ld a,(RX_BUFFER+6)
        cp c
        jr nz,wifi_result_bad
        ld a,(RX_BUFFER+9)
        cp 33
        jr nc,wifi_result_bad
        add a,4
        cp b
        jr nz,wifi_result_bad
        or a
        ret
wifi_result_bad:
        scf
        ret

display_wifi_result:
        ; Remember the security type for selection/connect handling.
        ld a,(wifi_result_index)
        ld e,a
        ld d,0
        ld hl,WIFI_SECURITY_LIST
        add hl,de
        ld a,(RX_BUFFER+8)
        ld (hl),a

        ld de,(list_screen_pointer)
        ; Page-100-style network row. Establish the selection number as true
        ; white-on-blue, then return to blue-on-white for signal and SSID text.
        ; This uses spacing controls instead of the P2000T's unreliable inverse
        ; colour interpretation.
        ld a,004h
        ld (de),a
        inc de
        ld a,01dh
        ld (de),a
        inc de
        ld a,007h
        ld (de),a
        inc de
        ld a,(wifi_result_index)
        add a,'1'
        ld (de),a
        inc de
        ld a,01dh
        ld (de),a
        inc de
        ld a,004h
        ld (de),a
        inc de
        ld a,' '
        ld (de),a
        inc de

        ; Four strength bars based on RSSI: -75, -67 and -55 dBm.
        ld a,(RX_BUFFER+7)
        ld b,1
        cp 0b5h
        jr c,wifi_strength_ready
        inc b
        cp 0bdh
        jr c,wifi_strength_ready
        inc b
        cp 0c9h
        jr c,wifi_strength_ready
        inc b
wifi_strength_ready:
        ld c,4
wifi_strength_loop:
        ld a,b
        or a
        ld a,'.'
        jr z,wifi_strength_store
        ld a,DISPLAY_HASH
        dec b
wifi_strength_store:
        ld (de),a
        inc de
        dec c
        jr nz,wifi_strength_loop

        ld a,(RX_BUFFER+8)
        or a
        jr z,wifi_security_open
        cp WIFI_SECURITY_PSK
        ld a,'*'
        jr z,wifi_security_store
        ld a,'!'
        jr wifi_security_store
wifi_security_open:
        ld a,' '
wifi_security_store:
        ld (de),a
        inc de
        ld a,' '
        ld (de),a
        inc de
        ld a,(RX_BUFFER+9)
        ; Thirteen prefix cells leave 27 columns for the displayed SSID. The
        ; complete value remains cached on the Pico for connection requests.
        cp 28
        jr c,wifi_ssid_length_ready
        ld a,27
wifi_ssid_length_ready:
        ld b,a
        ld hl,RX_BUFFER+10
        call write_bytes

        ld hl,(list_screen_pointer)
        ld bc,80
        add hl,bc
        ld (list_screen_pointer),hl
        ret

; Give all nine result rows a white background before individual SSIDs arrive,
; so a short scan result still reads as one continuous page-100-style panel.
wifi_prepare_list_panel:
        ld de,VIDEO_RAM+240
        ld a,9
wifi_prepare_list_panel_row:
        push af
        push de
        ld hl,wifi_white_blank_text
        call write_string
        pop de
        ld hl,80
        add hl,de
        ex de,hl
        pop af
        dec a
        jr nz,wifi_prepare_list_panel_row
        ret

; A short pause avoids flooding the parallel link while Wi-Fi work proceeds.
poll_delay:
        ld bc,0
poll_delay_loop:
        dec bc
        ld a,b
        or c
        jr nz,poll_delay_loop
        ret

; Select whether password entry is visible. This still consumes translated
; keys from the monitor's FIFO through read_key; it does not scan the keyboard.
read_password_visibility:
        call read_key
        cp KEY_STOP_EVENT
        jr z,read_password_cancel
        cp 'J'
        jr z,read_password_visible
        cp 'j'
        jr z,read_password_visible
        cp 'N'
        jr z,read_password_masked
        cp 'n'
        jr nz,read_password_visibility
read_password_masked:
        xor a
        ld (password_visible),a
        ret
read_password_visible:
        ld a,1
        ld (password_visible),a
        or a
        ret
read_password_cancel:
        scf
        ret

; Read a WPA password. Three page-style attribute cells plus the eleven-cell
; prompt leave 26 cells on the first row; characters 27-63 continue after the
; three attributes on the next row. Display either the entered byte or a mask.
read_password:
        ld hl,LINE_BUFFER
        ld de,VIDEO_RAM+1120+14
        xor a
        ld (password_length),a
read_password_key:
        call read_key
        cp KEY_STOP_EVENT
        jr z,read_password_cancel
        cp 00dh
        jr z,read_password_done
        cp 008h
        jr z,read_password_backspace
        cp 020h
        jr c,read_password_key
        cp 07fh
        jr nc,read_password_key
        ld c,a
        ld a,(password_length)
        cp MAX_PASSWORD_LENGTH
        jr nc,read_password_key
        cp 26
        jr nz,read_password_store
        ld de,VIDEO_RAM+1200+3
read_password_store:
        ld a,c
        ld (hl),a
        inc hl
        ld a,(password_visible)
        or a
        ld a,c
        jr nz,read_password_display
        ld a,'*'
read_password_display:
        call ascii_to_display
        ld (de),a
        inc de
        ld a,(password_length)
        inc a
        ld (password_length),a
        jr read_password_key
read_password_backspace:
        ld a,(password_length)
        or a
        jr z,read_password_key
        cp 26
        jr nz,read_password_backspace_positioned
        ld de,VIDEO_RAM+1120+14+26
read_password_backspace_positioned:
        dec a
        ld (password_length),a
        dec hl
        dec de
        ld a,' '
        ld (de),a
        jr read_password_key
read_password_done:
        or a
        ret

erase_password:
        ld b,MAX_PASSWORD_LENGTH
        ld hl,LINE_BUFFER
        ld de,FRAME_BUFFER+8
        xor a
erase_password_loop:
        ld (hl),a
        ld (de),a
        inc hl
        inc de
        djnz erase_password_loop
        ld (password_length),a
        ret

clear_password_display:
        ld de,VIDEO_RAM+1120
        call clear_line
        ld de,VIDEO_RAM+1200
        jp clear_line

; ---------------------------------------------------------------------------
; Interactive keyboard input

; Read a line of up to MAX_LINE_LENGTH characters into LINE_BUFFER. Enter
; sends the line and Backspace edits it. Empty lines are valid ECHO payloads.
read_line:
        ld hl,LINE_BUFFER
        ld de,VIDEO_RAM+160+6
        xor a
        ld (line_length),a
read_line_key:
        call read_key
        cp 00dh
        ret z
        cp 008h
        jr z,read_line_backspace
        cp 020h
        jr c,read_line_key
        cp 07fh
        jr nc,read_line_key
        ld c,a
        ld a,(line_length)
        cp MAX_LINE_LENGTH
        jr nc,read_line_key
        ld a,c
        ld (hl),a
        call ascii_to_display
        ld (de),a
        inc hl
        inc de
        ld a,(line_length)
        inc a
        ld (line_length),a
        jr read_line_key

read_line_backspace:
        ld a,(line_length)
        or a
        jr z,read_line_key
        dec a
        ld (line_length),a
        dec hl
        dec de
        ld a,020h
        ld (de),a
        jr read_line_key

; Return the next translated key in A. BC, DE and HL are preserved. The
; monitor's blocking read routine consumes its interrupt-driven 12-byte FIFO
; and returns keycodes 0-71, or 72-143 when Shift is active.
read_key:
        push bc
        push de
        push hl
read_key_wait_press:
        call MONITOR_READ_KEY
        jr c,read_key_stop
        call translate_key
        or a
        jr z,read_key_wait_press
        pop hl
        pop de
        pop bc
        ret
read_key_stop:
        ld a,KEY_STOP_EVENT
        pop hl
        pop de
        pop bc
        ret

; Nonblocking form used by the Teletekst page loop. The monitor's 0029h entry
; returns Z when its interrupt-filled FIFO is empty and Carry when the separate
; STOP key was used. STOP has no character code, so expose it internally as
; KEY_STOP_EVENT. Otherwise return NZ with the translated byte in A.
try_read_key:
        push bc
        push de
        push hl
        call MONITOR_KEY_AVAILABLE
        jr z,try_read_key_none
        jr c,try_read_key_stop
        call MONITOR_READ_KEY
        call translate_key
        or a
        jr try_read_key_done
try_read_key_stop:
        ld a,KEY_STOP_EVENT
        or a
        jr try_read_key_done
try_read_key_none:
        xor a
try_read_key_done:
        pop hl
        pop de
        pop bc
        ret

translate_key:
        ld e,a
        ld hl,keyboard_unshifted
        ld d,0
        add hl,de
        ld a,(hl)
        ret

; Monitor-keycode-to-ASCII tables. Zero entries are keys that the echo console
; does not use. The shifted table follows the unshifted table directly because
; the monitor represents Shift by adding 72 to the keycode.
keyboard_unshifted:
        defb KEY_LEFT_EVENT,'6',0,'q','3','5','7','4'
        defb 0,'h','z','s','d','g','j','f'
        defb 0,' ','0','0','#','0',',',KEY_RIGHT_EVENT
        defb 0,'n','<','x','c','b','m','v'
        defb 0,'y','a','w','e','t','u','r'
        defb 0,'9','+','-',008h,'0','1','-'
        defb '9','o','8','7',00dh,'p','8','@'
        defb '3','.', '2','1',0,'/','k','2'
        defb '6','l','5','4',0,';','i',':'

keyboard_shifted_table:
        defb 0,'&',0,'Q',0,'%',0,'$'
        defb 0,'H','Z','S','D','G','J','F'
        defb 0,' ',0,0,0,0,0,0
        defb 0,'N','>','X','C','B','M','V'
        defb 0,'Y','A','W','E','T','U','R'
        defb 0,')',0,0,008h,'=','!','_'
        defb 0,'O',0,0,00dh,'P','(',0
        defb KEY_START_EVENT,0,0,0,0,'?','K','"'
        defb 0,'L',0,0,0,'+','I','*'

; ---------------------------------------------------------------------------
; Minimal screen output

; A P2WP/2 peripheral remains usable, but lacks the P2WP/3 date-status and
; P2WP/4 custom-source/navigation contracts and P2WP/5 persisted URL storage.
; Explain the fallback once, then let the user continue normally.
show_protocol_legacy_warning:
        call clear_screen
        ld hl,protocol_legacy_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,opening_blue_rule_text
        ld de,MENU_RULE_RAM
        call write_string
        ld hl,protocol_legacy_mode_text
        ld de,VIDEO_RAM+240
        call write_string
        ld hl,protocol_legacy_found_text
        ld de,VIDEO_RAM+400
        call write_string
        ld hl,protocol_legacy_available_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,protocol_legacy_update_text
        ld de,VIDEO_RAM+640
        call write_string
        ld hl,protocol_continue_text
        ld de,VIDEO_RAM+800
        call write_string
        ld hl,opening_footer_text
        ld de,VIDEO_RAM+1840
        call write_string
        ld hl,opening_footer_version_text
        ld de,VIDEO_RAM+1840+34
        call write_string
        call MONITOR_CLEAR_KEY
        call MONITOR_READ_KEY
        ret

; A syntactically valid HELLO with no common revision must not be presented as
; an Internet failure. Stop on a dedicated, actionable compatibility screen.
show_protocol_incompatible:
        call clear_screen
        ld hl,protocol_incompatible_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,opening_blue_rule_text
        ld de,MENU_RULE_RAM
        call write_string
        ld hl,protocol_incompatible_shared_text
        ld de,VIDEO_RAM+320
        call write_string
        ld hl,protocol_incompatible_range_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,protocol_incompatible_update_text
        ld de,VIDEO_RAM+640
        call write_string
        ld hl,opening_footer_text
        ld de,VIDEO_RAM+1840
        call write_string
        ld hl,opening_footer_version_text
        ld de,VIDEO_RAM+1840+34
        call write_string
protocol_incompatible_loop:
        halt
        jr protocol_incompatible_loop

; Page-100-inspired SAA5050 splash in the classic blue, white and black NOS
; palette. P2000T is drawn as a centered blue mosaic badge with a white
; border; the seven-row centrepiece uses the native mosaic forms from the NOS
; page 100 masthead.
show_opening_screen:
        call clear_screen
        ld hl,opening_blue_blank_text
        ld de,VIDEO_RAM
        call write_string
        ld hl,opening_blue_blank_text
        ld de,VIDEO_RAM+80
        call write_string
        ld hl,opening_header_text
        ld de,VIDEO_RAM+160
        call write_string

        ld hl,opening_p2000t_logo_rows
        ld de,VIDEO_RAM+240
        ld a,7
        call write_packed_rows

        ld hl,opening_nos_logo_rows
        ld de,VIDEO_RAM+800
        ld a,7
        call write_packed_rows

        ld hl,opening_live_text
        ld de,VIDEO_RAM+1360
        call write_string
        ld hl,opening_hardware_text
        ld de,VIDEO_RAM+1440
        call write_string
        ld hl,opening_service_text
        ld de,VIDEO_RAM+1520
        call write_string
        ld hl,opening_service_detail_text
        ld de,VIDEO_RAM+1600
        call write_string

        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+1680
        call write_string

        ld hl,opening_start_text
        ld de,VIDEO_RAM+1760
        call write_string
        ld hl,opening_footer_text
        ld de,VIDEO_RAM+1840
        call write_string
        ld hl,opening_footer_version_text
        ld de,VIDEO_RAM+1840+34
        jp write_string

; Once Wi-Fi is connected, use the two spare source-menu rows below the
; function-key list for the installed/latest versions and Pico generation.
show_source_runtime_info:
        ld hl,source_versions_text
        ld de,VIDEO_RAM+1440
        call write_string
        ld hl,cartridge_release_version
        call write_release_version
        ld hl,source_pico_separator_text
        call write_string
        ld a,(device_info_valid)
        or a
        jr z,show_source_pico_unknown
        ld hl,pico_current_version
        call write_release_version
        jr show_source_latest
show_source_pico_unknown:
        ld hl,opening_unknown_text
        call write_string

show_source_latest:
        ld hl,source_latest_text
        ld de,VIDEO_RAM+1520
        call write_string
        ld a,(latest_version_valid)
        or a
        jr z,show_source_latest_unavailable
        ld hl,latest_release_version
        call write_release_version
        ret
show_source_latest_unavailable:
        ld hl,opening_unavailable_text
        jp write_string

; Render vMAJOR.MINOR.PATCH from three consecutive uint8 values at HL.
write_release_version:
        ld a,'v'
        ld (de),a
        inc de
        ld a,(hl)
        inc hl
        call write_decimal_byte
        ld a,'.'
        ld (de),a
        inc de
        ld a,(hl)
        inc hl
        call write_decimal_byte
        ld a,'.'
        ld (de),a
        inc de
        ld a,(hl)
        ; fall through

; Write an unsigned byte in decimal without leading zeroes.
write_decimal_byte:
        push bc
        ld b,0
write_decimal_hundreds:
        cp 100
        jr c,write_decimal_tens_begin
        sub 100
        inc b
        jr write_decimal_hundreds
write_decimal_tens_begin:
        ld c,0
write_decimal_tens:
        cp 10
        jr c,write_decimal_ready
        sub 10
        inc c
        jr write_decimal_tens
write_decimal_ready:
        push af
        ld a,b
        or a
        jr z,write_decimal_no_hundreds
        add a,'0'
        ld (de),a
        inc de
write_decimal_no_hundreds:
        ld a,b
        or c
        jr z,write_decimal_no_tens
        ld a,c
        add a,'0'
        ld (de),a
        inc de
write_decimal_no_tens:
        pop af
        add a,'0'
        ld (de),a
        inc de
        pop bc
        ret

; Wait nonblockingly so the start prompt can blink every 500 ms using the
; monitor's interrupt-driven 20 ms clock. Any regular key or STOP continues.
wait_for_opening_key:
        xor a
        ld (opening_timed_out),a
        ld hl,(MONITOR_CLOCK)
        ld (opening_countdown_last_tick),hl
        ld de,TELETEKST_AUTOSTART_TICKS
        add hl,de
        ld (opening_timeout_deadline),hl
        ld a,60
        ld (opening_countdown_seconds),a
        call opening_draw_countdown
        ld a,1
        ld (opening_blink_visible),a
        ld a,(MONITOR_CLOCK)
        ld (opening_blink_last_tick),a
opening_wait_key:
        call MONITOR_KEY_AVAILABLE
        ret c
        jr z,opening_blink_update
        call MONITOR_READ_KEY
        ret
opening_blink_update:
        ld hl,(MONITOR_CLOCK)
        ld de,(opening_timeout_deadline)
        or a
        sbc hl,de
        bit 7,h
        jr nz,opening_blink_continue
        xor a
        ld (opening_countdown_seconds),a
        call opening_draw_countdown
        ld a,1
        ld (opening_timed_out),a
        ret
opening_blink_continue:
        ld hl,(MONITOR_CLOCK)
        ld de,(opening_countdown_last_tick)
        or a
        sbc hl,de
        ld a,h
        or a
        jr nz,opening_countdown_tick
        ld a,l
        cp TELETEKST_CLOCK_TICKS
        jr c,opening_blink_check
opening_countdown_tick:
        ld hl,(opening_countdown_last_tick)
        ld de,TELETEKST_CLOCK_TICKS
        add hl,de
        ld (opening_countdown_last_tick),hl
        ld a,(opening_countdown_seconds)
        or a
        jr z,opening_blink_check
        dec a
        ld (opening_countdown_seconds),a
        call opening_draw_countdown
opening_blink_check:
        ld a,(MONITOR_CLOCK)
        ld b,a
        ld a,(opening_blink_last_tick)
        ld c,a
        ld a,b
        sub c
        cp TELETEKST_BLINK_TICKS
        jr c,opening_wait_key
        ld a,b
        ld (opening_blink_last_tick),a
        ld a,(opening_blink_visible)
        xor 1
        ld (opening_blink_visible),a
        or a
        jr z,opening_blink_hide
        ld hl,opening_prompt_text
        ld de,VIDEO_RAM+1760+4
        call write_string
        jr opening_wait_key
opening_blink_hide:
        ld hl,VIDEO_RAM+1760+4
        ld b,17
        ld a,020h
opening_blink_hide_loop:
        ld (hl),a
        inc hl
        djnz opening_blink_hide_loop
        jp opening_wait_key

opening_draw_countdown:
        ld a,(opening_countdown_seconds)
        ld de,VIDEO_RAM+1760+33
        jp teletekst_write_two_digits

; Establish a blue background and white separated-graphics foreground, then
; turn the readable '#' ROM patterns into full SAA5050 mosaic cells.
write_blue_mosaic_pattern:
        ld a,004h
        ld (de),a
        inc de
        ld a,01dh
        ld (de),a
        inc de
        ld a,017h
        ld (de),a
        inc de
write_blue_mosaic_pattern_loop:
        ld a,(hl)
        or a
        ret z
        cp '#'
        jr nz,write_blue_mosaic_pattern_store
        ld a,07fh
write_blue_mosaic_pattern_store:
        ld (de),a
        inc hl
        inc de
        jr write_blue_mosaic_pattern_loop

; Copy A packed 40-byte rows from ROM to the visible half of 80-byte video
; rows. Used for the native page-100 mosaic centrepiece.
write_packed_rows:
        push af
        ld bc,40
        ldir
        push hl
        ld hl,40
        add hl,de
        ex de,hl
        pop hl
        pop af
        dec a
        jr nz,write_packed_rows
        ret

clear_screen:
        ld hl,VIDEO_RAM
        ld (hl),020h
        ld de,VIDEO_RAM+1
        ; The T model stores 80 bytes per screen row even though only the
        ; selected 40-column window is visible: 24 * 80 = 1920 bytes.
        ld bc,1919
        ldir
        ret

; Convert ASCII punctuation that differs in the P2000T's Viewdata character
; set. Protocol/password buffers retain ASCII; only screen bytes pass here.
; Appendix D assigns 23h to pound and 5Fh to hash.
ascii_to_display:
        cp '#'
        ret nz
        ld a,DISPLAY_HASH
        ret

; HL points to a zero-terminated ASCII/control string, DE to video memory.
write_string:
        ld a,(hl)
        or a
        ret z
        call ascii_to_display
        ld (de),a
        inc hl
        inc de
        jr write_string

; Write B bytes from HL to video memory at DE.
write_bytes:
        ld a,b
        or a
        ret z
write_bytes_loop:
        ld a,(hl)
        call ascii_to_display
        ld (de),a
        inc hl
        inc de
        djnz write_bytes_loop
        ret

; Fill one 40-column screen row at DE with spaces.
clear_line:
        ld b,40
        ld a,020h
clear_line_loop:
        ld (de),a
        inc de
        djnz clear_line_loop
        ret

show_exchange_count:
        ld hl,count_text
        ld de,VIDEO_RAM+320
        call write_string
        ld de,VIDEO_RAM+320+14
        ld hl,(exchange_count)
        ld a,h
        call write_hex_byte
        ld a,l
        call write_hex_byte
        ret

; Show a positive end-to-end link heartbeat during a wireless scan. The
; counter and spinner advance only for acknowledged WIFI_SCAN_STATUS frames,
; so neither the local delay loop nor an unresponsive Pico can animate them.
show_wifi_poll:
        ld hl,(exchange_count)
        inc hl
        ld (exchange_count),hl
        push hl
        ld de,VIDEO_RAM+240+23
        ld a,h
        call write_hex_byte
        ld a,l
        call write_hex_byte
        pop hl
        ld a,l
        and 003h
        ld e,a
        ld d,0
        ld hl,wifi_spinner_chars
        add hl,de
        ld a,(hl)
        ld (VIDEO_RAM+240+28),a
        ret

; Display the radio bring-up phase returned by the Pico separately from the
; acknowledged local-link heartbeat.
show_wifi_phase:
        ld de,VIDEO_RAM+400
        call clear_line
        ld a,(RX_BUFFER+8)
        cp WIFI_INIT_STARTING
        jr z,show_wifi_phase_starting
        cp WIFI_INIT_READY
        jr z,show_wifi_phase_ready
        ld hl,wifi_radio_failed_text
        jr show_wifi_phase_write
show_wifi_phase_starting:
        ld hl,wifi_radio_starting_text
        jr show_wifi_phase_write
show_wifi_phase_ready:
        ld hl,wifi_radio_ready_text
show_wifi_phase_write:
        ld de,VIDEO_RAM+400
        jp write_string

write_hex_byte:
        push af
        rrca
        rrca
        rrca
        rrca
        call write_hex_nibble
        pop af
write_hex_nibble:
        and 00fh
        add a,'0'
        cp '9'+1
        jr c,write_hex_digit
        add a,7
write_hex_digit:
        ld (de),a
        inc de
        ret

hello_wait_text:
        defb 007h,"       WACHTEN OP PICO W...",0
hello_ok_text:
        defb "PICO VERBONDEN - TYP EEN REGEL",0
hello_fail_text:
        defb "PICO W NIET GEVONDEN OF REAGEERT NIET",0
echo_fail_text:
        defb "ECHO MISLUKT NA 3 POGINGEN",0
prompt_text:
        defb "VERSTUUR> ",0
response_text:
        defb "PICO> ",0
count_text:
        defb "WISSELINGEN:  ",0
wifi_title_text:
        defb 004h,01dh,007h," P2000T  WIFI-INSTELLING"
        defs 40-($-wifi_title_text),020h
        defb 0
wifi_scanning_text:
        defb 004h,01dh,007h,"   WIFI-NETWERKEN ZOEKEN...",0
wifi_poll_text:
        defb 007h,01dh,004h," LINK ACTIEF - POLL 0000 |",0
wifi_spinner_chars:
        defb '|','/','-',05dh
wifi_seen_text:
        defb 007h,01dh,004h," NETWERKEN GEVONDEN: 0",0
wifi_radio_starting_text:
        defb 004h,01dh,007h,"   RADIO: STARTEN",0
wifi_radio_ready_text:
        defb 004h,01dh,007h,"   RADIO: GEREED / SCAN ACTIEF",0
wifi_radio_failed_text:
        defb 004h,01dh,007h,"   RADIO: STARTEN MISLUKT",0
wifi_list_heading:
        defb 004h,01dh,007h,"N  SIGNAAL B  NETWERK",0
wifi_select_text:
        defb 004h,01dh,007h," KIES NETWERK (1-9): ",0
wifi_password_visibility_text:
        defb 004h,01dh,007h," WACHTWOORD TONEN BIJ INVOER? (J/N)",0
wifi_password_text:
        defb 007h,01dh,004h,"WACHTWOORD>",0
wifi_white_blank_text:
        defb 007h,01dh,004h,0
wifi_blue_blank_text:
        defb 004h,01dh,007h,0
wifi_password_short_text:
        defb 004h,01dh,007h," WACHTWOORD: MINSTENS 8 TEKENS",0
wifi_connecting_text:
        defb 004h,01dh,007h,"VERBINDEN - POGING 0/3...",0
wifi_connected_text:
        defb 007h,01dh,004h," WIFI VERBONDEN / IP-ADRES ONTVANGEN",0
wifi_bad_password_text:
        defb 007h,01dh,004h," AANMELDEN MISLUKT / FOUT WACHTWOORD",0
wifi_retry_text:
        defb 004h,01dh,007h," (O) OPNIEUW  (W) NIEUW WACHTWOORD",0
wifi_unsupported_text:
        defb 007h,01dh,004h," NETWERKBEVEILIGING NIET ONDERSTEUND",0
wifi_scan_failed_text:
        defb 007h,01dh,004h," ZOEKEN NAAR WIFI MISLUKT",0
wifi_no_networks_text:
        defb 007h,01dh,004h," GEEN WIFI-NETWERKEN GEVONDEN",0
wifi_connect_failed_text:
        defb 007h,01dh,004h," WIFI-VERBINDING MISLUKT",0
wifi_protocol_failed_text:
        defb 007h,01dh,004h," PICO WIFI-PROTOCOLFOUT",0
wifi_profile_title_text:
        defb 004h,01dh,007h," P2000T  BEWAARD WIFI-PROFIEL"
        defs 40-($-wifi_profile_title_text),020h
        defb 0
wifi_profile_connecting_text:
        defb 004h,01dh,007h," VERBINDEN MET BEWAARD NETWERK...",0
wifi_profile_automatic_text:
        defb 007h,01dh,004h," AUTOMATISCH / GEEN WACHTWOORD NODIG",0
wifi_profile_corrupt_text:
        defb 007h,01dh,004h," BEWAARD PROFIEL IS BESCHADIGD",0
wifi_profile_retry_text:
        defb 004h,01dh,007h," (O) OPNIEUW (N) NIEUW (V) VERWIJDER",0
wifi_profile_connect_failed_text:
        defb 007h,01dh,004h," BEWAARD NETWERK VERBINDEN MISLUKT",0
wifi_profile_save_offer_text:
        defb 007h,01dh,004h," WIFI-PROFIEL BEWAREN OF VERVANGEN?",0
wifi_profile_save_choice_text:
        defb 004h,01dh,007h," (J) BEWAREN  (N) ALLEEN DEZE KEER",0
wifi_profile_encrypting_text:
        defb 004h,01dh,007h," WIFI-PROFIEL BEWAREN...",0
wifi_profile_saved_text:
        defb 007h,01dh,004h," VERSLEUTELD WIFI-PROFIEL BEWAARD",0
wifi_profile_save_failed_text:
        defb 007h,01dh,004h," PROFIEL KON NIET WORDEN BEWAARD",0
wifi_profile_continue_text:
        defb 004h,01dh,007h," DRUK OP EEN TOETS OM DOOR TE GAAN",0
source_title_text:
        defb 004h,01dh,007h," P2000T TELETEKST"
        defs 40-($-source_title_text),020h
        defb 0
source_title_tag_text:
        defb "BRONKEUZE",0
source_intro_text:
        defb 004h,01dh,007h,"      KIES UW TELETEKSTBRON",0
source_white_blank_text:
        defb 007h,01dh,004h
        defs 37,020h
        defb 0
source_custom_text:
        defb 007h,01dh,004h,"  0 - EIGEN SERVER"
        defs 40-($-source_custom_text),020h
        defb 0
source_nos_text:
        defb 007h,01dh,004h,"  1 - NOS TELETEKST"
        defs 40-($-source_nos_text),020h
        defb 0
source_p2000t_text:
        defb 007h,01dh,004h,"  2 - P2000T TELETEKST"
        defs 40-($-source_p2000t_text),020h
        defb 0
source_archive_text:
        defb 007h,01dh,004h,"  3 - TELETEKSTARCHIEF.NL"
        defs 40-($-source_archive_text),020h
        defb 0
source_prompt_text:
        defb 004h,01dh,007h,"        KIES BRON (0-3)",0
source_auto_start_off_text:
        defb 007h,01dh,004h," A AUTOSTART NA 60S: UIT",0
source_auto_start_nos_text:
        defb 007h,01dh,004h," A AUTOSTART NA 60S: NOS",0
source_auto_start_p2000t_text:
        defb 007h,01dh,004h," A AUTOSTART NA 60S: P2000T",0
source_auto_start_archive_text:
        defb 007h,01dh,004h," A AUTOSTART NA 60S: ARCHIEF",0
source_auto_start_custom_text:
        defb 007h,01dh,004h," A AUTOSTART NA 60S: EIGEN",0
source_auto_start_v6_text:
        defb 007h,01dh,004h," AUTOSTART VEREIST P2WP/6",0
source_controls_title_text:
        defb 004h,01dh,007h,"      BEDIENING OP DE PAGINA",0
source_control_display_text:
        defb 007h,01dh,004h," START/I INDEX ?/R ONTHUL Z ZOOM"
        defs 40-($-source_control_display_text),020h
        defb 0
source_control_pages_text:
        defb 007h,01dh,004h,"  <-/P VORIGE     ->/N VOLGENDE"
        defs 40-($-source_control_pages_text),020h
        defb 0
source_control_subpage_text:
        defb 007h,01dh,004h,"  A PAUZE/DOORGAAN  S SUBPAGINA"
        defs 40-($-source_control_subpage_text),020h
        defb 0
source_control_wifi_text:
        defb 007h,01dh,004h,"  V AUTO-PAGINA  W WIFI  H HULP"
        defs 40-($-source_control_wifi_text),020h
        defb 0
source_control_stop_text:
        defb 007h,01dh,004h,"  STOP - ANDERE TELETEKSTBRON"
        defs 40-($-source_control_stop_text),020h
        defb 0
source_custom_v4_text:
        defb 001h,"       EIGEN SERVER VEREIST P2WP/4",0
custom_title_text:
        defb 004h,01dh,007h," P2000T  EIGEN TELETEKSTSERVER"
        defs 40-($-custom_title_text),020h
        defb 0
custom_intro_text:
        defb 007h,01dh,004h," BASISADRES VAN UW EIGEN SERVER",0
custom_memory_text:
        defb 007h,01dh,004h," PICO ONTHOUDT ALLEEN EEN NIEUW ADRES",0
custom_example_text:
        defb 007h,01dh,004h," VOORBEELD  http://terra:8080",0
custom_security_text:
        defb 003h,01dh,004h," HTTPS: CERTIFICAATCONTROLE STAAT UIT",0
custom_input_text:
        defb 004h,01dh,007h," SERVERADRES                 MAX. 96",0
custom_field_text:
        defb 007h,01dh,004h," "
        defs 36,020h
        defb 0
custom_controls_text:
        defb 007h,01dh,004h," ENTER OPSLAAN BS WIS STOP TERUG",0
source_versions_text:
        defb 007h,01dh,004h,"CARTRIDGE: ",0
source_pico_separator_text:
        defb " / PICO ",0
source_latest_text:
        defb 004h,01dh,007h,"LAATSTE VERSIE ONLINE: ",0
teletekst_title_text:
        defb 004h,01dh,007h," P2000T  TELETEKST VIA PICO W"
        defs 40-($-teletekst_title_text),020h
        defb 0
teletekst_page_text:
        defb "PAGINA: ",0
teletekst_unavailable_text:
        defb "TELETEKSTPAGINA NIET BESCHIKBAAR",0
teletekst_error_text:
        defb "FOUTCODE: 00",0
teletekst_error_prefix_text:
        defb "FOUT: ",0
teletekst_error_detail_text:
        defb "DETAIL: HTTP 000 LWIP 00 NET 00",0
teletekst_error_retry_text:
        defb "PROBEER OPNIEUW OF KIES EEN ANDERE BRON",0
teletekst_error_not_connected_text:
        defb "GEEN WIFI-VERBINDING",0
teletekst_error_tls_config_text:
        defb "TLS-CONFIGURATIE MISLUKT",0
teletekst_error_request_start_text:
        defb "AANVRAAG KON NIET STARTEN",0
teletekst_error_network_text:
        defb "ONBEKENDE NETWERKFOUT",0
teletekst_error_http_text:
        defb "HTTP-SERVERFOUT",0
teletekst_error_too_large_text:
        defb "ANTWOORD TE GROOT",0
teletekst_error_invalid_data_text:
        defb "ONGELDIGE PAGINADATA",0
teletekst_error_dns_text:
        defb "DNS-NAAM NIET GEVONDEN",0
teletekst_error_connect_text:
        defb "VERBINDING OF TLS MISLUKT",0
teletekst_error_closed_text:
        defb "VERBINDING AFGEBROKEN",0
teletekst_error_timeout_text:
        defb "SERVER REAGEERT NIET",0
teletekst_error_memory_text:
        defb "TE WEINIG PICO-GEHEUGEN",0
teletekst_error_content_length_text:
        defb "ONVOLLEDIG ANTWOORD",0
teletekst_error_abort_text:
        defb "AANVRAAG AFGEBROKEN",0
teletekst_error_unknown_text:
        defb "ONBEKENDE FOUT",0
page_not_found_header_text:
        defb 004h,01dh,007h," P2000T  TELETEKST"
        defs 40-($-page_not_found_header_text),020h
        defb 0
page_not_found_masthead_text:
        defb 004h,01dh,007h,"       P2000T TELETEKSTDIENST",0
page_not_found_blank_text:
        defb 001h,01dh,007h,"                                 ",0
page_not_found_title_text:
        defb 001h,01dh,007h,"       PAGINA NIET GEVONDEN     ",0
page_not_found_page_text:
        defb 001h,01dh,007h,"           PAGINA 000           ",0
page_not_found_message_text:
        defb 001h,01dh,007h,"    DEZE PAGINA BESTAAT NIET    ",0
page_not_found_prompt_text:
        defb 001h,01dh,007h,"    TYP EEN NIEUW PAGINANUMMER  ",0
page_not_found_continue_text:
        defb 001h,01dh,007h,"          OM DOOR TE GAAN       ",0
teletekst_indicator_frames:
        defb SAA5050_GRAPHICS_WHITE,021h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; top left
        defb SAA5050_GRAPHICS_WHITE,022h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; top right
        defb SAA5050_GRAPHICS_WHITE,028h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; middle right
        defb SAA5050_GRAPHICS_WHITE,060h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; bottom right
        defb SAA5050_GRAPHICS_WHITE,030h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; bottom left
        defb SAA5050_GRAPHICS_WHITE,024h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; middle left

opening_header_text:
        defb 004h,01dh,007h,"    P2000T  INTERNET TELETEKST"
        defs 40-($-opening_header_text),020h
        defb 0
opening_blue_blank_text:
        defb 004h,01dh
        defs 38,020h
        defb 0
opening_blue_rule_text:
        defb 014h
        defs 39,073h
        defb 0
opening_live_text:
        defb 007h,01dh,004h,"     UW VENSTER OP DE WERELD",0
opening_hardware_text:
        defb 007h,01dh,004h,"     NOS EN P2000T TELETEKST",0
opening_service_text:
        defb 004h,01dh,007h,"  ORIGINEEL SAA5050-MOZAIEKBEELD",0
opening_service_detail_text:
        defb 004h,01dh,007h,"  KLASSIEK BEELD, ACTUEEL NIEUWS",0
opening_start_text:
        defb 007h,"   DRUK OP EEN TOETS  AUTO-MODE 60",0
opening_prompt_text:
        defb "DRUK OP EEN TOETS",0
opening_footer_text:
        defb 004h,01dh,007h,"P2000T Teletekst Cartridge"
        defs 40-($-opening_footer_text),020h
        defb 0
opening_footer_version_text:
        defb "v0.5.0",0
cartridge_release_version:
        defb CARTRIDGE_VERSION_MAJOR,CARTRIDGE_VERSION_MINOR,CARTRIDGE_VERSION_PATCH
opening_unknown_text:
        defb "ONBEKEND",0
opening_unavailable_text:
        defb "NIET BESCHIKBAAR",0
protocol_legacy_title_text:
        defb 004h,01dh,007h,"        PROTOCOLWAARSCHUWING"
        defs 40-($-protocol_legacy_title_text),020h
        defb 0
protocol_legacy_mode_text:
        defb 004h,01dh,007h,"   COMPATIBILITEITSMODUS ACTIEF",0
protocol_legacy_found_text:
        defb 007h,01dh,004h,"     P2WP/2 VERBINDING GEVONDEN",0
protocol_legacy_available_text:
        defb 007h,01dh,004h,"    TELETEKST BLIJFT BESCHIKBAAR",0
protocol_legacy_update_text:
        defb 004h,01dh,007h,"     UPDATE INTERFACE VOOR P2WP/7",0
protocol_continue_text:
        defb 007h,"   DRUK OP EEN TOETS OM DOOR TE GAAN",0

protocol_incompatible_title_text:
        defb 004h,01dh,007h,"      PROTOCOL NIET COMPATIBEL"
        defs 40-($-protocol_incompatible_title_text),020h
        defb 0
protocol_incompatible_shared_text:
        defb 007h,01dh,004h,"      GEEN GEDEELDE P2WP-VERSIE",0
protocol_incompatible_range_text:
        defb 007h,01dh,004h,"    CARTRIDGE: P2WP/2 TOT P2WP/7",0
protocol_incompatible_update_text:
        defb 004h,01dh,007h,"     UPDATE CARTRIDGE OF INTERFACE",0

; Cartridge-resident Dutch help page. Each text row includes its SAA5050
; foreground/background controls and remains within the 40-cell display width.
help_header_text:
        defb 004h,01dh,007h," P2000T  HULP"
        defs 40-($-help_header_text),020h
        defb 0
help_intro_text:
        defb 007h,01dh,004h,"       BEDIENING VAN DE CARTRIDGE",0
help_page_title_text:
        defb 004h,01dh,007h," PAGINA EN VERBINDING",0
help_page_entry_text:
        defb 007h,01dh,004h," 100-899  TYP DRIE CIJFERS",0
help_index_text:
        defb 007h,01dh,004h," START/I  INDEXPAGINA 100",0
help_browse_text:
        defb 007h,01dh,004h," <-/P ->/N VORIGE / VOLGENDE PAGINA",0
help_auto_page_text:
        defb 007h,01dh,004h," V        AUTO VOLGENDE PAGINA",0
help_display_title_text:
        defb 004h,01dh,007h," WEERGAVE",0
help_reveal_text:
        defb 007h,01dh,004h," ?/R      VERBORGEN TEKST ONTHULLEN",0
help_zoom_text:
        defb 007h,01dh,004h," Z        BOVEN / ONDER / NORMAAL",0
help_source_text:
        defb 007h,01dh,004h," STOP     ANDERE BRON / INVOER TERUG",0
help_wifi_text:
        defb 007h,01dh,004h," W        KIES EEN ANDER WIFI-NETWERK",0
help_subpage_title_text:
        defb 004h,01dh,007h," SUBPAGINA'S",0
help_subpage_select_text:
        defb 007h,01dh,004h," S        KIES EEN SUBPAGINA",0
help_pause_text:
        defb 007h,01dh,004h," A        SUBPAGINA PAUZE / DOOR",0
help_help_text:
        defb 007h,01dh,004h," H        DEZE HULPPAGINA",0
help_return_text:
        defb 004h,01dh,007h,"     DRUK EEN TOETS OM TERUG TE GAAN",0

archive_url_text:
        defb "https://teletekstarchief.nl"
archive_url_text_end:

opening_p2000t_row_1:
        defb "     ","###"," ","###"," ","###"," ","###"," ","###"," ","###",0
opening_p2000t_row_2:
        defb "     ","# #"," ","  #"," ","# #"," ","# #"," ","# #"," "," # ",0
opening_p2000t_row_3:
        defb "     ","###"," ","###"," ","# #"," ","# #"," ","# #"," "," # ",0
opening_p2000t_row_4:
        defb "     ","#  "," ","#  "," ","# #"," ","# #"," ","# #"," "," # ",0
opening_p2000t_row_5:
        defb "     ","#  "," ","###"," ","###"," ","###"," ","###"," "," # ",0

; Seven packed rows of native partial mosaics. White foreground subcells trace
; a one-subcell contour around blue negative-space letterforms, so the border
; follows P2000T instead of filling a rectangular character-cell background.
; The final row also starts the upper Teletekst frame, joining both wordmarks.
opening_p2000t_logo_rows:
        defb 004h,01dh,017h,020h,020h,020h,020h,020h,020h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,020h,020h,020h,020h,020h,020h,020h
        defb 004h,01dh,017h,020h,020h,020h,020h,020h,020h,035h,020h,020h,06ah,035h,020h,020h,06ah,035h,020h,020h,06ah,035h,020h,020h,06ah,035h,020h,020h,06ah,035h,020h,020h,06ah,020h,020h,020h,020h,020h,020h,020h
        defb 004h,01dh,017h,020h,020h,020h,020h,020h,020h,035h,06ah,035h,06ah,07fh,07fh,035h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,07fh,035h,06ah,023h,020h,020h,020h,020h,020h,020h,020h
        defb 004h,01dh,017h,020h,020h,020h,020h,020h,020h,035h,020h,020h,06ah,035h,020h,020h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,07fh,035h,06ah,020h,020h,020h,020h,020h,020h,020h,020h
        defb 004h,01dh,017h,020h,020h,020h,020h,020h,020h,035h,06ah,07fh,07fh,035h,06ah,07fh,07fh,035h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,035h,06ah,07fh,035h,06ah,020h,020h,020h,020h,020h,020h,020h,020h
        defb 004h,01dh,017h,020h,020h,020h,020h,020h,020h,035h,06ah,07fh,07fh,035h,020h,020h,06ah,035h,020h,020h,06ah,035h,020h,020h,06ah,035h,020h,020h,06ah,07fh,035h,06ah,020h,020h,020h,020h,020h,020h,020h,020h
        defb 004h,01dh,017h,03ch,02ch,02ch,02ch,02ch,02ch,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02fh,02ch,02ch,02ch,02ch,02ch,02ch,034h,020h

; Seven display-ready continuation rows derived from the stable blue-and-white
; NOS page 100 masthead. Its upper frame begins in the shared row above.
opening_nos_logo_rows:
        defb 004h,01dh,017h,035h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,020h,035h,020h
        defb 004h,01dh,017h,075h,070h,070h,030h,020h,060h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,030h,020h,060h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,070h,020h,020h,070h,070h,070h,035h,020h
        defb 004h,01dh,020h,020h,020h,017h,035h,020h,06ah,020h,060h,070h,035h,020h,07fh,07fh,020h,060h,070h,035h,020h,06ah,020h,060h,070h,035h,020h,035h,020h,035h,020h,070h,07ah,020h,020h,035h,020h,020h,020h,020h
        defb 004h,01dh,020h,020h,020h,017h,035h,020h,06ah,020h,02ah,02fh,035h,020h,07fh,07fh,020h,02ah,02fh,035h,020h,06ah,020h,02ah,02fh,035h,020h,065h,070h,035h,020h,02fh,06fh,020h,020h,035h,020h,020h,020h,020h
        defb 004h,01dh,020h,020h,020h,017h,035h,020h,06ah,020h,068h,07ch,035h,020h,07fh,07fh,020h,068h,07ch,035h,020h,06ah,020h,068h,07ch,035h,020h,034h,020h,07dh,07ch,020h,06ah,020h,020h,035h,020h,020h,020h,020h
        defb 004h,01dh,020h,020h,020h,017h,035h,020h,06ah,020h,022h,023h,035h,020h,023h,06bh,020h,022h,023h,035h,020h,06ah,020h,022h,023h,035h,020h,035h,020h,037h,023h,020h,06ah,020h,020h,035h,020h,020h,020h,020h
        defb 017h,07ch,07ch,07ch,07ch,070h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,073h,071h,078h,07ch,07ch,07ch

; ---------------------------------------------------------------------------
; RAM variables (addresses are allocated by the assembler in ROM, so these
; labels contain initial storage only if used directly). Runtime variables are
; kept in the fixed RAM block below through absolute symbols.

sequence:               equ 07400h
expected_type:          equ 07401h
expected_sequence:      equ 07402h
attempts_left:          equ 07403h
rx_escaped:             equ 07404h
body_length:            equ 07406h
rx_pointer:             equ 07408h
rx_length:              equ 0740ah
crc_value:              equ 0740ch
exchange_count:         equ 0740eh
line_length:            equ 07410h
wifi_connect_attempts_left: equ 07411h
list_screen_pointer:    equ 07412h
password_length:       equ 07414h
wifi_network_count:    equ 07415h
wifi_result_index:     equ 07416h
wifi_selected_index:   equ 07417h
password_visible:      equ 07418h
teletekst_source:      equ 07419h
LINE_BUFFER:            equ 07420h
WIFI_SECURITY_LIST:     equ 07450h
teletekst_page:         equ 07460h
teletekst_subpage:      equ 07462h
teletekst_next_subpage: equ 07463h
teletekst_chunk:        equ 07464h
teletekst_input_count:  equ 07465h
teletekst_rotation_enabled: equ 07466h
teletekst_rotation_deadline: equ 07467h
teletekst_screen_pointer: equ 07469h
teletekst_error_code:   equ 0746bh
TELETEXT_INPUT_BUFFER:  equ 0746ch
teletekst_indicator_phase: equ 0746fh
TELETEXT_INDICATOR_SAVED: equ 07470h
wifi_profile_state:      equ 07474h
wifi_profile_error:      equ 07475h
wifi_profile_mode:       equ 07476h
link_timeout_deadline:    equ 07477h
teletekst_rotation_paused: equ 07479h
TELETEXT_SUBPAGE_INPUT:   equ 0747ah
teletekst_pause_saved_cell: equ 0747ch
teletekst_help_return_source: equ 0747dh
teletekst_clock_hours:   equ 0747eh
teletekst_clock_minutes: equ 0747fh
teletekst_clock_seconds: equ 07480h
teletekst_clock_last_tick: equ 07481h
teletekst_clock_valid:   equ 07483h
teletekst_clock_day:     equ 07484h
teletekst_clock_month:   equ 07485h
teletekst_clock_year:    equ 07486h
teletekst_clock_weekday: equ 07487h
teletekst_clock_blink_phase: equ 07488h
opening_blink_last_tick: equ 07489h
opening_blink_visible:  equ 0748ah
p2wp_session_version:  equ 0748bh
hello_error_kind:      equ 0748ch
teletekst_status_length: equ 0748dh
teletekst_clock_has_date: equ 0748eh
p2wp_capabilities:       equ 0748fh
device_info_valid:       equ 07490h
pico_hardware_model:     equ 07491h
pico_current_version:    equ 07492h
latest_version_checked:  equ 07495h
latest_version_valid:    equ 07496h
latest_version_error:    equ 07497h
latest_release_version:  equ 07498h
version_check_deadline:  equ 0749bh
teletekst_cycle_started: equ 0749dh
opening_timed_out:       equ 0749eh
opening_timeout_deadline: equ 0749fh
teletekst_auto_start_source: equ 074a1h
teletekst_auto_page_enabled: equ 074a2h
wifi_cancel_enabled:     equ 074a3h
opening_countdown_last_tick: equ 074a4h
opening_countdown_seconds: equ 074a6h
teletekst_auto_retry_pending: equ 074a7h
teletekst_http_result: equ 074a8h
teletekst_lwip_error: equ 074a9h
teletekst_http_status: equ 074aah
TELETEXT_SCREEN_BUFFER: equ 07500h
custom_url_length:       equ 078c0h
teletekst_previous_page: equ 078c1h
teletekst_next_page:     equ 078c3h
teletekst_reveal_enabled: equ 078c5h
teletekst_zoom_state:    equ 078c6h
teletekst_page_valid:    equ 078c7h
teletekst_render_colour: equ 078c8h
teletekst_auto_page_saved_cell: equ 078c9h
TELETEXT_RAW_SCREEN_BUFFER: equ 07900h
CUSTOM_URL_BUFFER:      equ 07d00h

        defs 05000h-$,0ffh
