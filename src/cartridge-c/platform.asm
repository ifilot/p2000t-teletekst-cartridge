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

; The P2000T monitor's blocking keyboard routine returns its key code in A.
; The sccz80 ABI returns an unsigned char in HL.
_platform_read_key:
    call $0026
    jr nc,platform_read_key_ready
    ld a,$fe
platform_read_key_ready:
    ld l,a
    ld h,0
    ret

; Return 0 when the monitor FIFO is empty, 1 for a regular key and 2 for STOP.
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

; The monitor updates this 16-bit, 20 ms tick count from its interrupt handler.
_platform_clock:
    ld hl,($6010)
    ret

_platform_link_status:
    in a,($42)
    ld l,a
    ld h,0
    ret

_platform_link_receive:
    in a,($41)
    ld l,a
    ld h,0
    ret

; __z88dk_fastcall supplies the byte in L.
_platform_link_send:
    ld a,l
    out ($40),a
    ret

; Return the first visible byte of screen row A in HL. The P2000T uses an
; 80-byte stride even though this cartridge displays only the first 40 bytes.
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

; sccz80 passes row at SP+2.
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

; SDCC packs the byte arguments: row is at SP+2, column at SP+3 and text at
; SP+4. IX is pushed before the arguments are addressed, adding two bytes.
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

; SDCC packs row at SP+2, column at SP+3 and value at SP+4. Decimal conversion
; is deliberately limited to uint8_t and uses subtraction, avoiding stdio and
; the general-purpose division/modulo runtime.
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

; Fixed-width hexadecimal byte, using the same packed SDCC byte arguments.
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
platform_hex_digit:
    and $0f
    add a,'0'
    cp '9'+1
    ret c
    add a,'A'-'9'-1
    ret

; Fixed-width three digit number (page and HTTP status are both <= 999).
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

; SDCC packs row at SP+2, column at SP+3, data at SP+4 and length at SP+6.
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

; SDCC packs row at SP+2, column at SP+3, data at SP+4 and length at SP+6.
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

; Synchronise full-page changes with the monitor's 20 ms video tick. Blanking
; during the copy prevents a half-old/half-new Teletext frame from being shown.
platform_wait_vsync:
    ld hl,$6010
    ld a,(hl)
platform_wait_vsync_tick:
    cp (hl)
    jr z,platform_wait_vsync_tick
    ret

; screen is at SP+2. Copy 40 visible bytes, then skip the hidden half-row.
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

; Compact formatter for viewer_state_t's clock fields. SDCC passes the state
; pointer at SP+2 and the destination at SP+4. Return the byte count in HL.
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
    ld a,(ix+25)             ; clock_has_date
    or a
    jr z,platform_clock_time
    ld a,(ix+23)             ; weekday * 2
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
    ld a,(ix+20)
    call platform_clock_two_digits
    ld a,'.'
    call platform_clock_put
    ld a,(ix+21)             ; (month - 1) * 3
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
    ld a,(ix+17)
    call platform_clock_two_digits
    ld a,':'
    ld c,(ix+0)              ; P2000T source has the blinking colon
    dec c
    jr nz,platform_clock_colon
    ld c,(ix+26)
    dec c
    jr nz,platform_clock_colon
    ld a,' '
platform_clock_colon:
    call platform_clock_put
    ld a,(ix+18)
    call platform_clock_two_digits
    ld a,(ix+0)
    dec a
    jr z,platform_clock_done
    ld a,':'
    call platform_clock_put
    ld a,(ix+19)
    call platform_clock_two_digits
platform_clock_done:
    ld l,b
    ld h,0
    pop ix
    ret

platform_clock_copy2:
    ld a,(hl)
    call platform_clock_put
    inc hl
    ld a,(hl)
    jr platform_clock_put

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
platform_clock_put:
    ld (de),a
    inc de
    inc b
    ret

platform_weekdays:
    defb "zomadiwodovrza"
platform_months:
    defb "janfebmrtaprmeijunjulaugsepoktnovdec"

; Advance viewer_state_t's cached clock at each half-second boundary. HL is a
; fastcall state pointer; return one only when the displayed clock changed.
_platform_advance_clock:
    push ix
    push hl
    pop ix
    ld a,(ix+24)
    or a
    jp z,platform_clock_unchanged
    ld hl,($6010)
    ld e,(ix+27)
    ld d,(ix+28)
    or a
    sbc hl,de
    bit 7,h
    jp nz,platform_clock_unchanged
    ld hl,($6010)
    ld de,25
    add hl,de
    ld (ix+27),l
    ld (ix+28),h
    ld a,(ix+26)
    xor 1
    ld (ix+26),a
    jr nz,platform_clock_changed
    inc (ix+19)
    ld a,(ix+19)
    cp 60
    jr c,platform_clock_changed
    ld (ix+19),0
    inc (ix+18)
    ld a,(ix+18)
    cp 60
    jr c,platform_clock_changed
    ld (ix+18),0
    inc (ix+17)
    ld a,(ix+17)
    cp 24
    jr c,platform_clock_changed
    ld (ix+17),0
    ld a,(ix+25)
    or a
    jr z,platform_clock_changed
    inc (ix+23)
    ld a,(ix+23)
    cp 7
    jr c,platform_clock_weekday_ready
    ld (ix+23),0
platform_clock_weekday_ready:
    ld a,(ix+21)
    dec a
    ld l,a
    ld h,0
    ld de,platform_month_days
    add hl,de
    ld c,(hl)
    ld a,(ix+21)
    cp 2
    jr nz,platform_clock_days_ready
    ld a,(ix+22)
    and 3
    jr nz,platform_clock_days_ready
    inc c
platform_clock_days_ready:
    inc (ix+20)
    ld a,c
    cp (ix+20)
    jr nc,platform_clock_changed
    ld (ix+20),1
    inc (ix+21)
    ld a,(ix+21)
    cp 13
    jr c,platform_clock_changed
    ld (ix+21),1
    inc (ix+22)
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

; Update only conceal controls on a normal-size page, avoiding a full blanked
; commit (and therefore visible flicker) for the reveal key.
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

; Render a raw 40x24 SAA5050 page, replacing conceal controls when requested.
; Zoom creates the monitor's double-height 12-row layout. SDCC packs raw at
; SP+2, display at SP+4, zoom at SP+6 and reveal at SP+7.
_platform_render_page:
    push ix
    ld ix,0
    add ix,sp
    ld l,(ix+4)
    ld h,(ix+5)
    ld e,(ix+6)
    ld d,(ix+7)
    ld a,(ix+8)
    or a
    jr nz,platform_render_zoom
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

platform_render_zoom:
    push hl
    push de
    ex de,hl
    ld (hl),' '
    push hl
    pop de
    inc de
    ld bc,959
    ldir
    pop de
    pop hl
    ld a,(ix+8)
    cp 2
    jr nz,platform_render_zoom_source
    ld bc,480
    add hl,bc
platform_render_zoom_source:
    ld a,12
platform_render_zoom_row:
    push af
    ld a,$0d
    ld (de),a
    inc de
    ld c,$07
    ld b,39
    call platform_render_copy
    inc hl
    push hl
    ld hl,40
    add hl,de
    ex de,hl
    pop hl
    pop af
    dec a
    jr nz,platform_render_zoom_row
    pop ix
    ret

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
    ld a,(ix+9)
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

; Cartridge applications do not return to a caller.  Keeping this terminal
; loop in assembly also avoids pulling exit handling into the C program.
_platform_halt:
    halt
    jr _platform_halt

; The classic embedded CRT aliases this symbol even with stdio disabled.
; P2000T output always uses the explicit video-RAM routines above.
fputc_cons_native:
_fputc_cons_native:
    ret
