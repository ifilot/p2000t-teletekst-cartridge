; @file lz4_z80.asm
; @brief Raw LZ4 decoder adapted for the SDCC cartridge calling convention.
;
; This BSD-2-Clause source is linked into the GPL-3.0-only P2000T Teletekst
; cartridge. Its permissive upstream license and copyright remain intact.
; SPDX-License-Identifier: BSD-2-Clause
;
; Raw LZ4 decompressor by Piotr Drapich, adapted as an SDCC-callable routine.
; Copyright (c) 2013-2015 Piotr Drapich. All rights reserved.
; BSD 2-Clause license; source:
; https://github.com/p2000t/software/blob/main/cartridges/games/source/libs/LZ4_Z80.asm

SECTION code_user
PUBLIC _lz4_decompress

; void lz4_decompress(const uint8_t *source, uint8_t *destination,
;                     uint16_t compressed_size)
_lz4_decompress:
    push ix
    ld ix,0
    add ix,sp
    ld l,(ix+4)
    ld h,(ix+5)
    ld e,(ix+6)
    ld d,(ix+7)
    ld c,(ix+8)
    ld b,(ix+9)
    push hl
    add hl,bc
    ld b,h
    ld c,l
    pop hl
    push bc
    pop ix
    ld b,0
lz4_token:
    ld a,(hl)
    inc hl
    ld c,a
    cp $10
    jr c,lz4_matches
    ex af,af'
    ld a,c
    and $f0
    rlca
    rlca
    rlca
    rlca
    cp $0f
    jr nz,lz4_copy_literals
lz4_literal_length:
    ld c,(hl)
    inc hl
    add a,c
    jr nc,lz4_literal_no_carry
    ccf
    inc b
lz4_literal_no_carry:
    inc c
    jr z,lz4_literal_length
lz4_copy_literals:
    ld c,a
    ldir
    ex af,af'
lz4_matches:
    ex af,af'
    defb $dd
    ld a,l
    cp l
    jr nz,lz4_offset
    defb $dd
    ld a,h
    cp h
    jr z,lz4_done
lz4_offset:
    ex af,af'
    and $0f
    add a,4
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    push hl
    ld h,d
    ld l,e
    sbc hl,bc
    ld b,0
    ld c,a
    cp $13
    jr nz,lz4_copy_match
    ex (sp),hl
lz4_match_length:
    ld c,(hl)
    inc hl
    add a,c
    jr nc,lz4_match_no_carry
    ccf
    inc b
lz4_match_no_carry:
    inc c
    jr z,lz4_match_length
    ld c,a
    ex (sp),hl
lz4_copy_match:
    ldir
    pop hl
    jr lz4_token
lz4_done:
    pop ix
    ret
