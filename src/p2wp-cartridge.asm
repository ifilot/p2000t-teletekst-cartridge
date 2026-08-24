; P2WP/2 Wi-Fi and Teletekst client for a Philips P2000T cartridge.
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

P2WP_VERSION:          equ 002h
CARTRIDGE_VERSION_MAJOR: equ 0
CARTRIDGE_VERSION_MINOR: equ 2
CARTRIDGE_VERSION_PATCH: equ 1
P2WP_FLAG_RESPONSE:    equ 001h
P2WP_FLAG_ERROR:       equ 002h
P2WP_TYPE_HELLO:       equ 001h
P2WP_TYPE_ECHO:        equ 002h
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
SAA5050_GRAPHICS_WHITE: equ 017h
SAA5050_CONTIGUOUS_GRAPHICS: equ 019h
KEY_STOP_EVENT:        equ 080h
HOST_MAX_PAYLOAD:      equ 240
MAX_LINE_LENGTH:       equ 32
MAX_PASSWORD_LENGTH:   equ 63

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
TELETEKST_ERROR_PAGE_NOT_FOUND: equ 8
TELETEKST_CHUNK_COUNT: equ 4
TELETEKST_CHUNK_SIZE:  equ 240
TELETEKST_ROTATE_TICKS: equ 500
TELETEKST_SOURCE_NOS:   equ 0
TELETEKST_SOURCE_P2000T: equ 1
WIFI_SECURITY_OPEN:    equ 0
WIFI_SECURITY_PSK:     equ 1

; ---------------------------------------------------------------------------
; Cartridge entry point (1010h)

start:
        ld sp,STACK_TOP
        ; The monitor initialized its 20 ms keyboard interrupt before entering
        ; the cartridge. Keep interrupts enabled so its key FIFO stays active.
        ei
        call show_opening_screen
        ; Discard the key that may have launched the cartridge, then leave the
        ; completed splash visible until the user deliberately continues.
        call MONITOR_CLEAR_KEY
        call MONITOR_READ_KEY
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
        ; Pico W radio initialization loads CYW43 firmware before the request
        ; loop starts and can outlast three local-link attempts. Session setup
        ; therefore waits for the Pico instead of treating startup as fatal.
        jr c,hello_retry
        call validate_hello
        jr c,hello_failed

        jp wifi_profile_startup

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
        defb P2WP_VERSION,0,P2WP_TYPE_HELLO,0,8,0
        defb "P2WP",P2WP_VERSION,P2WP_VERSION,HOST_MAX_PAYLOAD,0
hello_template_end:

echo_header:
        defb P2WP_VERSION,0,P2WP_TYPE_ECHO,0,0,0
echo_header_end:

; ---------------------------------------------------------------------------
; Stop-and-wait transaction. FRAME_BUFFER/body_length must contain the
; request. Returns carry after three failed attempts.

transact:
        ld a,3
        ld (attempts_left),a
transact_attempt:
        call send_frame
        jr c,transact_retry
        call receive_frame
        jr c,transact_retry

        ld a,(RX_BUFFER)
        cp P2WP_VERSION
        jr nz,transact_retry
        ld a,(RX_BUFFER+1)
        and P2WP_FLAG_RESPONSE+P2WP_FLAG_ERROR
        cp P2WP_FLAG_RESPONSE
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
        in a,(STATUS_PORT)
        and STATUS_TX_READY
        jr nz,send_byte_ready
        dec bc
        ld a,b
        or c
        jr nz,send_byte_wait
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
        in a,(STATUS_PORT)
        and STATUS_RX_READY
        jr nz,receive_byte_ready
        dec bc
        ld a,b
        or c
        jr nz,receive_byte_wait
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
        jr nz,payload_bad
        ld a,(RX_BUFFER+5)
        or a
        jr nz,payload_bad
        ld hl,RX_BUFFER+6
        ld de,hello_response_prefix
        ld b,5
validate_hello_loop:
        ld a,(de)
        cp (hl)
        jr nz,payload_bad
        inc de
        inc hl
        djnz validate_hello_loop
        ld a,(RX_BUFFER+11)
        and 00eh
        cp 00eh
        jr nz,payload_bad
        ld a,(RX_BUFFER+12)
        cp HOST_MAX_PAYLOAD
        jr c,payload_bad
        or a
        ret

hello_response_prefix:
        defb "P2WP",P2WP_VERSION

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
        ld a,1
        ld (sequence),a
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
        cp 'R'
        jp z,wifi_profile_connect
        cp 'r'
        jp z,wifi_profile_connect
        cp 'N'
        jp z,wifi_setup
        cp 'n'
        jp z,wifi_setup
        cp 'D'
        jp z,wifi_profile_delete
        cp 'd'
        jr nz,wifi_profile_failure_key

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
        ld (VIDEO_RAM+320+20),a
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
        ld (VIDEO_RAM+1280+24),a

wifi_connect_poll:
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
        ld a,(wifi_profile_mode)
        or a
        call z,wifi_profile_offer_save
        call erase_password
        call teletekst_choose_source
        ld hl,100
        ld (teletekst_page),hl
        xor a
        ld (teletekst_subpage),a
        ld (teletekst_input_count),a
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
        cp 'Y'
        jr z,wifi_profile_send_save
        cp 'y'
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

; Choose the page API once per session, immediately after Wi-Fi has acquired
; an address. The selected source accompanies every subsequent page request.
teletekst_choose_source:
        call clear_screen
        ld hl,source_title_text
        ld de,MENU_HEADER_RAM
        call write_string
        ld hl,opening_blue_rule_text
        ld de,MENU_RULE_RAM
        call write_string
        ld hl,source_intro_text
        ld de,VIDEO_RAM+240
        call write_string
        ld hl,source_nos_text
        ld de,VIDEO_RAM+400
        call write_string
        ld hl,source_p2000t_text
        ld de,VIDEO_RAM+480
        call write_string
        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+640
        call write_string
        ld hl,source_prompt_text
        ld de,VIDEO_RAM+720
        call write_string
teletekst_choose_source_key:
        call read_key
        cp '1'
        jr z,teletekst_choose_source_nos
        cp '2'
        jr nz,teletekst_choose_source_key
        ld a,TELETEKST_SOURCE_P2000T
        jr teletekst_choose_source_store
teletekst_choose_source_nos:
        ld a,TELETEKST_SOURCE_NOS
teletekst_choose_source_store:
        ld (teletekst_source),a
        ret

; STOP returns to source selection without dropping the Wi-Fi connection.
; Keep the selected page number, but start that page at its default subpage on
; the newly selected server.
teletekst_change_source:
        xor a
        ld (teletekst_rotation_enabled),a
        ld (teletekst_input_count),a
        ld (teletekst_subpage),a
        call MONITOR_CLEAR_KEY
        call teletekst_choose_source
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

; A displayed page remains interactive. Three digits select a new page without
; Enter, just like a television Teletekst receiver. The monitor's 20 ms clock
; drives subpage rotation while its interrupt-owned FIFO supplies debounced
; keys without blocking this loop.
teletekst_main_loop:
        call try_read_key
        jp z,teletekst_check_rotation
        cp KEY_STOP_EVENT
        jp z,teletekst_change_source
        cp '0'
        jp c,teletekst_main_loop
        cp '9'+1
        jp nc,teletekst_main_loop
        ld c,a
        ld a,(teletekst_input_count)
        or a
        jr nz,teletekst_store_digit
        ld a,c
        cp '1'
        jr c,teletekst_main_loop
        cp '8'+1
        jr nc,teletekst_main_loop
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
        jr nz,teletekst_main_loop

        call teletekst_accept_input
        xor a
        ld (teletekst_input_count),a
        ld (teletekst_subpage),a
        call teletekst_fetch_page
        jr nc,teletekst_main_loop
        call show_teletekst_error
        jp teletekst_main_loop

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
        jp nz,teletekst_main_loop

        ld a,(teletekst_next_subpage)
        ld (teletekst_subpage),a
        call teletekst_fetch_page
        jp nc,teletekst_main_loop
        ; Keep the last complete page visible after a transient rotation
        ; failure. Typing another number remains available immediately.
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
        ld a,P2WP_TYPE_TELETEKST_FETCH_START
        call prepare_request
        ld hl,(teletekst_page)
        ld (FRAME_BUFFER+6),hl
        ld a,(teletekst_subpage)
        ld (FRAME_BUFFER+8),a
        ld a,(teletekst_source)
        ld (FRAME_BUFFER+9),a
        ld a,4
        ld (FRAME_BUFFER+4),a
        ld hl,10
        ld (body_length),hl
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
        ld a,(RX_BUFFER+4)
        cp 5
        jp nz,teletekst_fetch_failed
        ld a,(RX_BUFFER+5)
        or a
        jp nz,teletekst_fetch_failed
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
        call teletekst_indicator_restore
        scf
        ret

teletekst_fetch_rows:
        ld a,(RX_BUFFER+10)
        ld (teletekst_next_subpage),a
        ld hl,TELETEXT_SCREEN_BUFFER
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

        call teletekst_commit_screen
        ld hl,(MONITOR_CLOCK)
        ld de,TELETEKST_ROTATE_TICKS
        add hl,de
        ld (teletekst_rotation_deadline),hl
        ld a,1
        ld (teletekst_rotation_enabled),a
        or a
        ret

teletekst_fetch_failed:
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
        ld de,VIDEO_RAM+320+12
        ld a,(teletekst_error_code)
        call write_hex_byte
        xor a
        ld (teletekst_rotation_enabled),a
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
        ld de,VIDEO_RAM+960+22
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

        call wait_for_vsync
        xor a
        out (VIDEO_CONTROL_PORT),a
        ld (teletekst_rotation_enabled),a
        ret

show_selected_page:
        ld hl,teletekst_page_text
        ld de,VIDEO_RAM+240
        call write_string
        ld de,VIDEO_RAM+240+6
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
        cp 'R'
        jp z,wifi_begin_connect
        cp 'r'
        jp z,wifi_begin_connect
        cp 'P'
        jr z,wifi_connection_reenter
        cp 'p'
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
        ld (FRAME_BUFFER+2),a
        ld (expected_type),a
        ld a,(sequence)
        ld (FRAME_BUFFER+3),a
        ld (expected_sequence),a
        ld hl,6
        ld (body_length),hl
        ret

request_header:
        defb P2WP_VERSION,0,0,0,0,0

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
        cp 'Y'
        jr z,read_password_visible
        cp 'y'
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
        ret

; Read a WPA password. Three page-style attribute cells plus the ten-character
; prompt leave 27 cells on the first row; characters 28-63 continue after the
; three attributes on the next row. Display either the entered byte or a mask.
read_password:
        ld hl,LINE_BUFFER
        ld de,VIDEO_RAM+1120+13
        xor a
        ld (password_length),a
read_password_key:
        call read_key
        cp 00dh
        ret z
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
        cp 27
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
        cp 27
        jr nz,read_password_backspace_positioned
        ld de,VIDEO_RAM+1120+13+27
read_password_backspace_positioned:
        dec a
        ld (password_length),a
        dec hl
        dec de
        ld a,' '
        ld (de),a
        jr read_password_key

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
        call translate_key
        or a
        jr z,read_key_wait_press
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
        defb 0,'6',0,'q','3','5','7','4'
        defb 0,'h','z','s','d','g','j','f'
        defb 0,' ',0,0,'#',0,',',0
        defb 0,'n','<','x','c','b','m','v'
        defb 0,'y','a','w','e','t','u','r'
        defb 0,'9','+','-',008h,'0','1','-'
        defb 0,'o',0,0,00dh,'p','8','@'
        defb 0,'.',0,0,0,'/','k','2'
        defb 0,'l',0,0,0,';','i',':'

keyboard_shifted_table:
        defb 0,'&',0,'Q',0,'%',0,'$'
        defb 0,'H','Z','S','D','G','J','F'
        defb 0,' ',0,0,0,0,0,0
        defb 0,'N','>','X','C','B','M','V'
        defb 0,'Y','A','W','E','T','U','R'
        defb 0,')',0,0,008h,'=','!','_'
        defb 0,'O',0,0,00dh,'P','(',0
        defb 0,0,0,0,0,'?','K','"'
        defb 0,'L',0,0,0,'+','I','*'

; ---------------------------------------------------------------------------
; Minimal screen output

; Page-100-inspired SAA5050 splash in the classic blue, white and black NOS
; palette. P2000T is drawn as white mosaics on blue; the seven-row centrepiece
; uses the native mosaic forms from the NOS page 100 masthead.
show_opening_screen:
        call clear_screen
        ld hl,opening_header_text
        ld de,MENU_HEADER_RAM
        call write_string

        ld hl,opening_p2000t_row_1
        ld de,VIDEO_RAM+160
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_2
        ld de,VIDEO_RAM+240
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_3
        ld de,VIDEO_RAM+320
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_4
        ld de,VIDEO_RAM+400
        call write_blue_mosaic_pattern
        ld hl,opening_p2000t_row_5
        ld de,VIDEO_RAM+480
        call write_blue_mosaic_pattern

        ld hl,opening_blue_rule_text
        ld de,VIDEO_RAM+560
        call write_string

        ld hl,opening_nos_logo_rows
        ld de,VIDEO_RAM+640
        ld a,7
        call write_packed_rows

        ld hl,opening_live_text
        ld de,VIDEO_RAM+1200
        call write_string
        ld hl,opening_hardware_text
        ld de,VIDEO_RAM+1280
        call write_string

        ld hl,opening_blue_rule_text
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
        jp write_string

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
        ld de,VIDEO_RAM+320+11
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
        defb 007h,"       WAITING FOR PICO W...",0
hello_ok_text:
        defb "PICO HELLO OK - TYPE A LINE",0
hello_fail_text:
        defb "HELLO FAILED AFTER 3 ATTEMPTS",0
echo_fail_text:
        defb "ECHO FAILED AFTER 3 ATTEMPTS",0
prompt_text:
        defb "SEND> ",0
response_text:
        defb "PICO> ",0
count_text:
        defb "EXCHANGES:  ",0
wifi_title_text:
        defb 004h,01dh,007h," P2000T  WIFI SETUP"
        defs 40-($-wifi_title_text),020h
        defb 0
wifi_scanning_text:
        defb 004h,01dh,007h,"   SEARCHING FOR WIFI NETWORKS...",0
wifi_poll_text:
        defb 007h,01dh,004h," LINK ACTIVE - POLL 0000 |",0
wifi_spinner_chars:
        defb '|','/','-',05dh
wifi_seen_text:
        defb 007h,01dh,004h," NETWORKS FOUND: 0",0
wifi_radio_starting_text:
        defb 004h,01dh,007h,"   RADIO: INITIALIZING",0
wifi_radio_ready_text:
        defb 004h,01dh,007h,"   RADIO: READY / SCAN ACTIVE",0
wifi_radio_failed_text:
        defb 004h,01dh,007h,"   RADIO: INITIALIZATION FAILED",0
wifi_list_heading:
        defb 004h,01dh,007h,"N  SIGNAL S  NETWORK",0
wifi_select_text:
        defb 004h,01dh,007h," SELECT NETWORK (1-9): ",0
wifi_password_visibility_text:
        defb 004h,01dh,007h," SHOW PASSWORD WHILE TYPING? (Y/N)",0
wifi_password_text:
        defb 007h,01dh,004h,"PASSWORD> ",0
wifi_white_blank_text:
        defb 007h,01dh,004h,0
wifi_blue_blank_text:
        defb 004h,01dh,007h,0
wifi_password_short_text:
        defb 004h,01dh,007h," PASSWORD MUST CONTAIN 8+ CHARACTERS",0
wifi_connecting_text:
        defb 004h,01dh,007h,"CONNECTING - ATTEMPT 0/3...",0
wifi_connected_text:
        defb 007h,01dh,004h," WIFI CONNECTED / IP ADDRESS ACQUIRED",0
wifi_bad_password_text:
        defb 007h,01dh,004h," AUTH FAILED / CHECK PASSWORD",0
wifi_retry_text:
        defb 004h,01dh,007h," (R) RETRY  (P) RE-ENTER PASSWORD",0
wifi_unsupported_text:
        defb 007h,01dh,004h," NETWORK SECURITY IS NOT SUPPORTED",0
wifi_scan_failed_text:
        defb 007h,01dh,004h," WIRELESS SCAN FAILED",0
wifi_no_networks_text:
        defb 007h,01dh,004h," NO WIRELESS NETWORKS FOUND",0
wifi_connect_failed_text:
        defb 007h,01dh,004h," WIRELESS CONNECTION FAILED",0
wifi_protocol_failed_text:
        defb 007h,01dh,004h," PICO WIFI PROTOCOL ERROR",0
wifi_profile_title_text:
        defb 004h,01dh,007h," P2000T  SAVED WIFI PROFILE"
        defs 40-($-wifi_profile_title_text),020h
        defb 0
wifi_profile_connecting_text:
        defb 004h,01dh,007h," CONNECTING TO SAVED NETWORK...",0
wifi_profile_automatic_text:
        defb 007h,01dh,004h," AUTOMATIC LOGIN / NO PASSWORD NEEDED",0
wifi_profile_corrupt_text:
        defb 007h,01dh,004h," SAVED PROFILE IS DAMAGED",0
wifi_profile_retry_text:
        defb 004h,01dh,007h," (R) RETRY  (N) NEW  (D) DELETE",0
wifi_profile_connect_failed_text:
        defb 007h,01dh,004h," SAVED NETWORK CONNECTION FAILED",0
wifi_profile_save_offer_text:
        defb 007h,01dh,004h," SAVE OR REPLACE THE WIFI PROFILE?",0
wifi_profile_save_choice_text:
        defb 004h,01dh,007h," (Y) REMEMBER  (N) SESSION ONLY",0
wifi_profile_encrypting_text:
        defb 004h,01dh,007h," SAVING WIFI PROFILE...",0
wifi_profile_saved_text:
        defb 007h,01dh,004h," ENCRYPTED WIFI PROFILE SAVED",0
wifi_profile_save_failed_text:
        defb 007h,01dh,004h," PROFILE COULD NOT BE SAVED",0
wifi_profile_continue_text:
        defb 004h,01dh,007h," PRESS ANY KEY TO CONTINUE",0
source_title_text:
        defb 004h,01dh,007h," P2000T  TELETEKST PROVIDER"
        defs 40-($-source_title_text),020h
        defb 0
source_intro_text:
        defb 004h,01dh,007h,"        SELECT ONLINE SERVICE",0
source_nos_text:
        defb 007h,01dh,004h," (1) NOS TELETEKST",0
source_p2000t_text:
        defb 007h,01dh,004h," (2) P2000T TELETEKST",0
source_prompt_text:
        defb 004h,01dh,007h," SELECT SOURCE (1-2): ",0
teletekst_title_text:
        defb 004h,01dh,007h," P2000T  TELETEKST VIA PICO W"
        defs 40-($-teletekst_title_text),020h
        defb 0
teletekst_page_text:
        defb "PAGE: ",0
teletekst_unavailable_text:
        defb "TELETEKST PAGE UNAVAILABLE",0
teletekst_error_text:
        defb "ERROR CODE: 00",0
page_not_found_header_text:
        defb 004h,01dh,007h," P2000T  TELETEKST"
        defs 37-($-page_not_found_header_text),020h
        defb "404",0
page_not_found_masthead_text:
        defb 004h,01dh,007h,"       P2000T TELETEKST SERVICE",0
page_not_found_blank_text:
        defb 001h,01dh,007h,"                                 ",0
page_not_found_title_text:
        defb 001h,01dh,007h,"         PAGE NOT FOUND          ",0
page_not_found_page_text:
        defb 001h,01dh,007h,"            PAGE 000             ",0
page_not_found_message_text:
        defb 001h,01dh,007h,"      THIS PAGE DOES NOT EXIST   ",0
page_not_found_prompt_text:
        defb 001h,01dh,007h,"     TYPE A NEW PAGE NUMBER      ",0
page_not_found_continue_text:
        defb 001h,01dh,007h,"         TO CONTINUE             ",0
teletekst_indicator_frames:
        defb SAA5050_GRAPHICS_WHITE,021h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; top left
        defb SAA5050_GRAPHICS_WHITE,022h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; top right
        defb SAA5050_GRAPHICS_WHITE,028h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; middle right
        defb SAA5050_GRAPHICS_WHITE,060h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; bottom right
        defb SAA5050_GRAPHICS_WHITE,030h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; bottom left
        defb SAA5050_GRAPHICS_WHITE,024h,SAA5050_CONTIGUOUS_GRAPHICS,SAA5050_ALPHA_WHITE ; middle left

opening_header_text:
        defb 004h,01dh,007h," P2000T  INTERNET TELETEKST"
        defs 40-($-opening_header_text),020h
        defb 0
opening_blue_rule_text:
        defb 014h
        defs 39,073h
        defb 0
opening_live_text:
        defb 007h,01dh,004h,"       LIVE TELETEKST VIA PICO W",0
opening_hardware_text:
        defb 007h,01dh,004h,"     ORIGINAL SAA5050 MOSAIC VIDEO",0
opening_service_text:
        defb 004h,01dh,007h,"       P2000T INTERNET SERVICE",0
opening_service_detail_text:
        defb 004h,01dh,007h,"      CLASSIC SCREEN / LIVE DATA",0
opening_start_text:
        defb 007h,"       PRESS ANY KEY TO START",0
opening_footer_text:
        defb 004h,01dh,007h," P2000T PICO W P2WP/2 v0.2.1"
        defs 40-($-opening_footer_text),020h
        defb 0
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

; Seven compiled rows from the stable blue-and-white NOS page 100 masthead.
; They are already valid SAA5050 control and separated-mosaic bytes.
opening_nos_logo_rows:
        defb 004h,01dh,017h,03ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,02ch,034h,020h
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
TELETEXT_SCREEN_BUFFER: equ 07500h

        defs 05000h-$,0ffh
