; @file platform.asm
; @brief Compact P2000T monitor, video, clock, and cartridge-port routines.
;
; SPDX-License-Identifier: GPL-3.0-only
; This file is part of the P2000T Teletekst cartridge and is licensed under
; version 3 of the GNU General Public License. See the repository LICENSE.

SECTION code_user

PUBLIC _platform_read_key
PUBLIC _platform_key_status
PUBLIC _platform_halt
PUBLIC _platform_clock
PUBLIC _platform_link_status
PUBLIC _platform_link_receive
PUBLIC _platform_link_send
PUBLIC _platform_clear_screen
PUBLIC _platform_clear_line
PUBLIC _platform_write_text
PUBLIC _platform_write_u8
PUBLIC _platform_write_hex
PUBLIC _platform_write_page
PUBLIC _platform_write_bytes
PUBLIC _platform_read_bytes
PUBLIC _platform_present_screen
PUBLIC _platform_format_clock
PUBLIC _platform_advance_clock
PUBLIC _platform_commit_reveal
PUBLIC _platform_render_page
PUBLIC fputc_cons_native
PUBLIC _fputc_cons_native

; @brief Blocks in the monitor until a regular key or STOP is available.
; @return HL contains the monitor key code; STOP is normalized to 0xfe.
; @note The monitor returns its key code in A; the C ABI returns uint8_t in HL.
_platform_read_key:
    call $0026
    jr nc,platform_read_key_ready
    ld a,$fe
platform_read_key_ready:
    ld l,a
    ld h,0
    ret

; @brief Queries the monitor keyboard FIFO without consuming a key.
; @return HL is 0 when empty, 1 for a regular key, or 2 for STOP.
_platform_key_status:
    call $0029
    jr c,platform_key_status_stop
    jr z,platform_key_status_empty
    ld l,1
    jr platform_key_status_ready
platform_key_status_stop:
    ld l,2
    jr platform_key_status_ready
platform_key_status_empty:
    ld l,0
platform_key_status_ready:
    ld h,0
    ret

; @brief Reads the monitor's interrupt-driven 20 ms tick counter.
; @return HL contains the wrapping 16-bit tick count.
_platform_clock:
    ld hl,($6010)
    ret

; @brief Reads the Pico link status port.
; @return HL contains the unsigned status byte read from port 0x42.
_platform_link_status:
    in a,($42)
    ld l,a
    ld h,0
    ret

; @brief Reads one byte from the Pico link receive port.
; @return HL contains the unsigned byte read from port 0x41.
_platform_link_receive:
    in a,($41)
    ld l,a
    ld h,0
    ret

; @brief Writes one byte to the Pico link transmit port.
; @param[in] L Byte supplied by the __z88dk_fastcall ABI.
; @return No value; AF is clobbered.
_platform_link_send:
    ld a,l
    out ($40),a
    ret

; @brief Resolves a screen-row number to its first visible video-RAM byte.
; @param[in] A Zero-based row number.
; @return HL points at the row in video RAM; DE and flags are clobbered.
; @note Rows have an 80-byte stride although only 40 bytes are visible.
platform_row_address:
    ld l,a
    ld h,0
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl                 ; row * 16
    ld d,h
    ld e,l
    add hl,hl
    add hl,hl                 ; row * 64
    add hl,de                 ; row * 80
    ld de,$5000
    add hl,de
    ret

; @brief Replaces all 40 visible bytes in each of the 24 rows with spaces.
; @return No value; AF, BC, DE, and HL are clobbered.
_platform_clear_screen:
    ld hl,$5000
    ld c,24
platform_clear_screen_row:
    ld b,40
    ld a,' '
platform_clear_screen_byte:
    ld (hl),a
    inc hl
    djnz platform_clear_screen_byte
    ld de,40
    add hl,de
    dec c
    jr nz,platform_clear_screen_row
    ret

; @brief Replaces one visible screen row with spaces.
; @param[in] SP+2 row: zero-based screen row supplied by SDCC.
; @return No value; AF, B, and HL are clobbered.
_platform_clear_line:
    ld hl,2
    add hl,sp
    ld a,(hl)
    call platform_row_address
    ld b,40
    ld a,' '
platform_clear_line_byte:
    ld (hl),a
    inc hl
    djnz platform_clear_line_byte
    ret

; @brief Writes a null-terminated string, clipped at the visible row edge.
; @param[in] SP+2 row: zero-based screen row.
; @param[in] SP+3 column: zero-based starting column.
; @param[in] SP+4 text: pointer to the source string.
; @return No value; IX is restored and other working registers are clobbered.
; @note Pushing IX moves the accessed argument offsets two bytes higher.
_platform_write_text:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+4)
    call platform_row_address
    ld c,(ix+5)
    ld b,0
    add hl,bc
    ld a,40
    sub c
    ld b,a
    ld e,(ix+6)
    ld d,(ix+7)
    ld a,b
    or a
    jr z,platform_write_text_done
platform_write_text_byte:
    ld a,(de)
    or a
    jr z,platform_write_text_done
    ld (hl),a
    inc hl
    inc de
    djnz platform_write_text_byte
platform_write_text_done:
    pop ix
    ret

; @brief Writes an unsigned byte in compact decimal notation.
; @param[in] SP+2 row: zero-based screen row.
; @param[in] SP+3 column: zero-based starting column.
; @param[in] SP+4 value: byte to format.
; @return No value; IX is restored and other working registers are clobbered.
; @note Subtraction avoids pulling division, modulo, or stdio into the ROM.
_platform_write_u8:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+4)
    call platform_row_address
    ld c,(ix+5)
    ld b,0
    add hl,bc
    ld a,(ix+6)
    ld d,0                    ; whether a hundreds digit was emitted
    ld c,'0'
platform_write_u8_hundreds:
    cp 100
    jr c,platform_write_u8_hundreds_done
    sub 100
    inc c
    jr platform_write_u8_hundreds
platform_write_u8_hundreds_done:
    ld e,a
    ld a,c
    cp '0'
    jr z,platform_write_u8_tens_start
    ld (hl),a
    inc hl
    inc d
platform_write_u8_tens_start:
    ld a,e
    ld c,'0'
platform_write_u8_tens:
    cp 10
    jr c,platform_write_u8_tens_done
    sub 10
    inc c
    jr platform_write_u8_tens
platform_write_u8_tens_done:
    ld e,a
    ld a,d
    or a
    jr nz,platform_write_u8_emit_tens
    ld a,c
    cp '0'
    jr z,platform_write_u8_ones
platform_write_u8_emit_tens:
    ld (hl),c
    inc hl
platform_write_u8_ones:
    ld a,e
    add a,'0'
    ld (hl),a
    pop ix
    ret

; @brief Writes one byte as two fixed-width uppercase hexadecimal digits.
; @param[in] SP+2 row: zero-based screen row.
; @param[in] SP+3 column: zero-based starting column.
; @param[in] SP+4 value: byte to format.
; @return No value; IX is restored and other working registers are clobbered.
_platform_write_hex:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+4)
    call platform_row_address
    ld c,(ix+5)
    ld b,0
    add hl,bc
    ld a,(ix+6)
    ld c,a
    rrca
    rrca
    rrca
    rrca
    call platform_hex_digit
    ld (hl),a
    inc hl
    ld a,c
    call platform_hex_digit
    ld (hl),a
    pop ix
    ret
; @brief Converts the low nibble of A to an uppercase hexadecimal digit.
; @param[in] A Byte whose low nibble is converted.
; @return A contains the ASCII digit; flags are clobbered.
platform_hex_digit:
    and $0f
    add a,'0'
    cp '9'+1
    ret c
    add a,'A'-'9'-1
    ret

; @brief Writes a 16-bit value as three fixed-width decimal digits.
; @param[in] SP+2 row: zero-based screen row.
; @param[in] SP+3 column: zero-based starting column.
; @param[in] SP+4 value: number in the supported range zero through 999.
; @return No value; IX is restored and other working registers are clobbered.
_platform_write_page:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+4)
    call platform_row_address
    ld c,(ix+5)
    ld b,0
    add hl,bc
    ld e,(ix+6)
    ld d,(ix+7)
    ld b,'0'
platform_page_hundreds:
    ld a,d
    or a
    jr nz,platform_page_sub_hundred
    ld a,e
    cp 100
    jr c,platform_page_tens_begin
platform_page_sub_hundred:
    ld a,e
    sub 100
    ld e,a
    ld a,d
    sbc a,0
    ld d,a
    inc b
    jr platform_page_hundreds
platform_page_tens_begin:
    ld (hl),b
    inc hl
    ld b,'0'
platform_page_tens:
    ld a,e
    cp 10
    jr c,platform_page_ones
    sub 10
    ld e,a
    inc b
    jr platform_page_tens
platform_page_ones:
    ld (hl),b
    inc hl
    ld a,e
    add a,'0'
    ld (hl),a
    pop ix
    ret

; @brief Copies display bytes into one row and clips them at column 40.
; @param[in] SP+2 row: zero-based screen row.
; @param[in] SP+3 column: zero-based starting column.
; @param[in] SP+4 data: pointer to source bytes.
; @param[in] SP+6 length: requested byte count.
; @return No value; IX is restored and other working registers are clobbered.
_platform_write_bytes:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+4)
    call platform_row_address
    ld c,(ix+5)
    ld b,0
    add hl,bc
    ld b,(ix+8)
    ld a,40
    sub c
    cp b
    jr nc,platform_write_bytes_count_ready
    ld b,a
platform_write_bytes_count_ready:
    ld e,(ix+6)
    ld d,(ix+7)
platform_write_bytes_byte:
    ld a,b
    or a
    jr z,platform_write_bytes_done
    ld a,(de)
    ld (hl),a
    inc de
    inc hl
    djnz platform_write_bytes_byte
platform_write_bytes_done:
    pop ix
    ret

; @brief Copies visible row bytes to a caller buffer, clipped at column 40.
; @param[in] SP+2 row: zero-based screen row.
; @param[in] SP+3 column: zero-based starting column.
; @param[out] SP+4 data: pointer to the destination buffer.
; @param[in] SP+6 length: requested byte count.
; @return No value; IX is restored and other working registers are clobbered.
_platform_read_bytes:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+4)
    call platform_row_address
    ld c,(ix+5)
    ld b,0
    add hl,bc
    ld b,(ix+8)
    ld a,40
    sub c
    cp b
    jr nc,platform_read_bytes_count_ready
    ld b,a
platform_read_bytes_count_ready:
    ld e,(ix+6)
    ld d,(ix+7)
platform_read_bytes_byte:
    ld a,b
    or a
    jr z,platform_read_bytes_done
    ld a,(hl)
    ld (de),a
    inc hl
    inc de
    djnz platform_read_bytes_byte
platform_read_bytes_done:
    pop ix
    ret

; @brief Waits until the monitor's 20 ms video tick changes.
; @return No value; A and HL are clobbered.
platform_wait_vsync:
    ld hl,$6010
    ld a,(hl)
platform_wait_vsync_tick:
    cp (hl)
    jr z,platform_wait_vsync_tick
    ret

; @brief Atomically presents a packed 40-by-24 display buffer.
; @param[in] SP+2 screen: pointer to the packed source buffer.
; @return No value; working registers are clobbered.
; @note Video output is blanked during the synchronized copy to prevent tears.
_platform_present_screen:
    call platform_wait_vsync
    ld a,$80
    out ($30),a
    pop bc                     ; return address
    pop hl                     ; packed 40x24 source
    push hl
    push bc
    ld de,$5000
    ld a,24
platform_present_screen_row:
    ld bc,40
    ldir
    ex de,hl
    ld bc,40
    add hl,bc
    ex de,hl
    dec a
    jr nz,platform_present_screen_row
    call platform_wait_vsync
    xor a
    out ($30),a
    ret

; @brief Formats the date/time fields stored in viewer_state_t.
; @param[in] SP+2 state: pointer to the viewer state byte layout.
; @param[out] SP+4 out: destination display-byte buffer.
; @return HL contains the number of bytes written.
; @note IX is restored; the other working registers are clobbered.
_platform_format_clock:
    push ix
    ld ix,0
    add ix,sp
    ld e,(ix+6)
    ld d,(ix+7)
    ld l,(ix+4)
    ld h,(ix+5)
    push hl
    pop ix
    ld b,0
    ld a,(ix+24)             ; clock_has_date
    or a
    jr z,platform_clock_time
    ld a,(ix+22)             ; weekday * 2
    add a,a
    ld l,a
    ld h,0
    push de
    ld de,platform_weekdays
    add hl,de
    pop de
    call platform_clock_copy2
    ld a,' '
    call platform_clock_put
    ld a,(ix+19)
    call platform_clock_two_digits
    ld a,'.'
    call platform_clock_put
    ld a,(ix+20)             ; (month - 1) * 3
    dec a
    ld l,a
    add a,a
    add a,l
    ld l,a
    ld h,0
    push de
    ld de,platform_months
    add hl,de
    pop de
    ld a,(hl)
    call platform_clock_put
    inc hl
    ld a,(hl)
    call platform_clock_put
    inc hl
    ld a,(hl)
    call platform_clock_put
    ld a,' '
    call platform_clock_put
platform_clock_time:
    ld a,(ix+16)
    call platform_clock_two_digits
    ld a,':'
    ld c,(ix+0)              ; P2000T source has the blinking colon
    dec c
    jr nz,platform_clock_colon
    ld c,(ix+25)
    dec c
    jr nz,platform_clock_colon
    ld a,' '
platform_clock_colon:
    call platform_clock_put
    ld a,(ix+17)
    call platform_clock_two_digits
    ld a,(ix+0)
    dec a
    jr z,platform_clock_done
    ld a,':'
    call platform_clock_put
    ld a,(ix+18)
    call platform_clock_two_digits
platform_clock_done:
    ld l,b
    ld h,0
    pop ix
    ret

; @brief Copies two lookup-table bytes into the clock output stream.
; @param[in] HL Source address; DE destination; B current output length.
; @return HL and DE advance by two and B increases by two.
platform_clock_copy2:
    ld a,(hl)
    call platform_clock_put
    inc hl
    ld a,(hl)
    jr platform_clock_put

; @brief Emits A as exactly two decimal digits into the clock output stream.
; @param[in] A Value from zero through 99; DE destination; B output length.
; @return DE advances by two and B increases by two; AF and C are clobbered.
platform_clock_two_digits:
    ld c,'0'
platform_clock_tens:
    cp 10
    jr c,platform_clock_digits
    sub 10
    inc c
    jr platform_clock_tens
platform_clock_digits:
    push af
    ld a,c
    call platform_clock_put
    pop af
    add a,'0'
; @brief Appends one byte to the clock output stream.
; @param[in] A Byte to write; DE destination; B current output length.
; @return DE advances by one and B increases by one.
platform_clock_put:
    ld (de),a
    inc de
    inc b
    ret

platform_weekdays:
    defb "zomadiwodovrza"
platform_months:
    defb "janfebmrtaprmeijunjulaugsepoktnovdec"

; @brief Advances viewer_state_t's cached clock at each half-second boundary.
; @param[in,out] HL Fastcall pointer to mutable viewer state.
; @return HL is one when the displayed clock changed, otherwise zero.
; @note IX is restored; AF, BC, and DE are clobbered.
_platform_advance_clock:
    push ix
    push hl
    pop ix
    ld a,(ix+23)
    or a
    jp z,platform_clock_unchanged
    ld hl,($6010)
    ld e,(ix+26)
    ld d,(ix+27)
    or a
    sbc hl,de
    bit 7,h
    jp nz,platform_clock_unchanged
    ld hl,($6010)
    ld de,25
    add hl,de
    ld (ix+26),l
    ld (ix+27),h
    ld a,(ix+25)
    xor 1
    ld (ix+25),a
    jr nz,platform_clock_changed
    inc (ix+18)
    ld a,(ix+18)
    cp 60
    jr c,platform_clock_changed
    ld (ix+18),0
    inc (ix+17)
    ld a,(ix+17)
    cp 60
    jr c,platform_clock_changed
    ld (ix+17),0
    inc (ix+16)
    ld a,(ix+16)
    cp 24
    jr c,platform_clock_changed
    ld (ix+16),0
    ld a,(ix+24)
    or a
    jr z,platform_clock_changed
    inc (ix+22)
    ld a,(ix+22)
    cp 7
    jr c,platform_clock_weekday_ready
    ld (ix+22),0
platform_clock_weekday_ready:
    ld a,(ix+20)
    dec a
    ld l,a
    ld h,0
    ld de,platform_month_days
    add hl,de
    ld c,(hl)
    ld a,(ix+20)
    cp 2
    jr nz,platform_clock_days_ready
    ld a,(ix+21)
    and 3
    jr nz,platform_clock_days_ready
    inc c
platform_clock_days_ready:
    inc (ix+19)
    ld a,c
    cp (ix+19)
    jr nc,platform_clock_changed
    ld (ix+19),1
    inc (ix+20)
    ld a,(ix+20)
    cp 13
    jr c,platform_clock_changed
    ld (ix+20),1
    inc (ix+21)
platform_clock_changed:
    ld hl,1
    pop ix
    ret
platform_clock_unchanged:
    ld hl,0
    pop ix
    ret

platform_month_days:
    defb 31,28,31,30,31,30,31,31,30,31,30,31

; @brief Updates only conceal controls in an already-visible normal page.
; @param[in] SP+2 screen: pointer to the raw 40-by-24 page buffer.
; @param[in] SP+4 reveal: nonzero to reveal concealed text.
; @return No value; IX is restored and other working registers are clobbered.
; @note Avoids the visible flicker of a full blanked-screen commit.
_platform_commit_reveal:
    push ix
    ld ix,0
    add ix,sp
    ld l,(ix+4)
    ld h,(ix+5)
    ld a,(ix+6)
    ex af,af'
    ld de,$5000
    ld a,24
platform_reveal_row:
    push af
    ld c,$07
    ld b,40
platform_reveal_column:
    ld a,(hl)
    and $7f
    cp 1
    jr c,platform_reveal_conceal
    cp 8
    jr c,platform_reveal_colour
    cp $11
    jr c,platform_reveal_conceal
    cp $18
    jr nc,platform_reveal_conceal
platform_reveal_colour:
    ld c,a
    jr platform_reveal_next
platform_reveal_conceal:
    cp $18
    jr nz,platform_reveal_next
    ex af,af'
    or a
    jr z,platform_reveal_hidden
    ex af,af'
    ld a,c
    jr platform_reveal_store
platform_reveal_hidden:
    ex af,af'
    ld a,(hl)
platform_reveal_store:
    ld (de),a
platform_reveal_next:
    inc hl
    inc de
    djnz platform_reveal_column
    push hl
    ld hl,40
    add hl,de
    ex de,hl
    pop hl
    pop af
    dec a
    jr nz,platform_reveal_row
    pop ix
    ret

; @brief Renders raw SAA5050 bytes and optionally replaces conceal controls.
; @param[in] SP+2 raw: pointer to the raw 40-by-24 page.
; @param[out] SP+4 display: pointer to the packed destination buffer.
; @param[in] SP+6 reveal: nonzero to render concealed text visibly.
; @return No value; IX is restored and other working registers are clobbered.
_platform_render_page:
    push ix
    ld ix,0
    add ix,sp
    ld l,(ix+4)
    ld h,(ix+5)
    ld e,(ix+6)
    ld d,(ix+7)
    ld a,24
platform_render_normal_row:
    push af
    ld c,$07
    ld b,40
    call platform_render_copy
    pop af
    dec a
    jr nz,platform_render_normal_row
    pop ix
    ret

; @brief Renders the current 40-byte row into the packed display buffer.
; @param[in,out] HL Raw source pointer and DE display destination pointer.
; @param[in] IX Viewer arguments; C current SAA5050 foreground colour.
; @return HL and DE advance by 40; B reaches zero; AF and C are clobbered.
platform_render_copy:
    ld a,(hl)
    inc hl
    push af
    and $7f
    cp 1
    jr c,platform_render_conceal
    cp 8
    jr c,platform_render_colour
    cp $11
    jr c,platform_render_conceal
    cp $18
    jr nc,platform_render_conceal
platform_render_colour:
    ld c,a
    jr platform_render_original
platform_render_conceal:
    cp $18
    jr nz,platform_render_original
    ld a,(ix+8)
    or a
    jr z,platform_render_original
    pop af
    ld a,c
    jr platform_render_store
platform_render_original:
    pop af
platform_render_store:
    ld (de),a
    inc de
    djnz platform_render_copy
    ret

; @brief Enters the cartridge's terminal halt loop.
; @return Never returns.
; @note Assembly implementation avoids linking C exit handling.
_platform_halt:
    halt
    jr _platform_halt

; @brief Satisfies the classic CRT's unused console-output hook.
; @param[in] ABI-specific character argument, intentionally ignored.
; @return No value; all registers are preserved.
; @note P2000T output always uses the explicit video-RAM routines above.
fputc_cons_native:
_fputc_cons_native:
    ret
