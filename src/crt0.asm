; @file crt0.asm
; @brief Minimal Z88DK classic CRT and P2000T cartridge memory map.
;
; SPDX-License-Identifier: GPL-3.0-only
; This file is part of the P2000T Teletekst cartridge and is licensed under
; version 3 of the GNU General Public License. See the repository LICENSE.
;
; The monitor maps this image at 0x1000 and jumps to 0x1010.  The first
; sixteen bytes are therefore the cartridge header, not executable code.

DEFC ROM_Start = $1000
DEFC RAM_Start = $7000
DEFC Stack_Top = $9ff0

MODULE p2000t_crt0

defc crt0 = 1
INCLUDE "zcc_opt.def"

EXTERN _main
PUBLIC __Exit
PUBLIC l_dcal

IF DEFINED_CRT_ORG_BSS
    defc __crt_org_bss = CRT_ORG_BSS
ELSE
    defc __crt_org_bss = RAM_Start
ENDIF

defc CRT_ORG_CODE = ROM_Start
defc CRT_ENABLE_STDIO = 0
defc TAR__register_sp = Stack_Top
defc TAR__clib_exit_stack_size = 0
defc TAR__crt_on_exit = $10001
defc __CPU_CLOCK = 2500000

INCLUDE "crt/classic/crt_rules.inc"

org CRT_ORG_CODE

; sign_cartridge.py fills the checksum length and checksum words.
defb $5e
defw 0
defw 0
defm "P2K-TELETXT"

; @brief Initializes the C runtime and transfers control to main().
; @return Does not return; main() ultimately enters platform_halt().
start:
    di
    INCLUDE "crt/classic/crt_init_sp.inc"
    call crt0_init
    INCLUDE "crt/classic/crt_init_atexit.inc"
    INCLUDE "crt/classic/crt_init_heap.inc"
    ; The monitor writes Shift/lock status at (0x6014)+1, and an
    ; attribute byte 0x800 bytes above it. Keep both writes outside
    ; the visible 40 columns of the 80-byte video row stride.
    ; On the T, the attribute address can alias the same video RAM.
    ld hl,$5028
    ld ($6014),hl
    ei
    call _main

; @brief Traps an unexpected return from the freestanding application.
; @return Never returns.
__Exit:
    jp __Exit

; @brief Implements Z88DK's indirect-call trampoline.
; @param[in] HL Address of the routine to call.
; @return Whatever registers and flags the target routine returns.
l_dcal:
    jp (hl)

INCLUDE "crt/classic/crt_runtime_selection.inc"

defc __crt_model = 1
INCLUDE "crt/classic/crt_section.inc"
