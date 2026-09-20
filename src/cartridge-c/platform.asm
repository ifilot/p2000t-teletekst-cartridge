SECTION code_user

PUBLIC _platform_read_key
PUBLIC _platform_halt
PUBLIC _platform_clock
PUBLIC _platform_link_status
PUBLIC _platform_link_receive
PUBLIC _platform_link_send
PUBLIC fputc_cons_native
PUBLIC _fputc_cons_native

; The P2000T monitor's blocking keyboard routine returns its key code in A.
; The sccz80 ABI returns an unsigned char in HL.
_platform_read_key:
    call $0026
    ld l,a
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

; Cartridge applications do not return to a caller.  Keeping this terminal
; loop in assembly also avoids pulling exit handling into the C program.
_platform_halt:
    halt
    jr _platform_halt

; The embedded classic CRT expects a native console symbol even when stdio is
; unused.  P2000T output goes through the explicit video-RAM API instead.
fputc_cons_native:
_fputc_cons_native:
    ret
