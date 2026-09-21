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
PUBLIC _platform_write_bytes
PUBLIC _platform_read_bytes
PUBLIC _platform_present_screen
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

; text is at SP+2, column at SP+4 and row at SP+6. IX is pushed before the
; arguments are addressed, shifting those offsets by two bytes.
_platform_write_text:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+8)
    call platform_row_address
    ld c,(ix+6)
    ld b,0
    add hl,bc
    ld a,40
    sub c
    ld b,a
    ld e,(ix+4)
    ld d,(ix+5)
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

; value is at SP+2, column at SP+4 and row at SP+6. Decimal conversion is
; deliberately limited to uint8_t and uses subtraction, avoiding stdio and
; the general-purpose division/modulo runtime.
_platform_write_u8:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+8)
    call platform_row_address
    ld c,(ix+6)
    ld b,0
    add hl,bc
    ld a,(ix+4)
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

; length is at SP+2, data at SP+4, column at SP+6 and row at SP+8.
_platform_write_bytes:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+10)
    call platform_row_address
    ld c,(ix+8)
    ld b,0
    add hl,bc
    ld b,(ix+4)
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

; length is at SP+2, data at SP+4, column at SP+6 and row at SP+8.
_platform_read_bytes:
    push ix
    ld ix,0
    add ix,sp
    ld a,(ix+10)
    call platform_row_address
    ld c,(ix+8)
    ld b,0
    add hl,bc
    ld b,(ix+4)
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

; screen is at SP+2. Copy 40 visible bytes, then skip the hidden half-row.
_platform_present_screen:
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
