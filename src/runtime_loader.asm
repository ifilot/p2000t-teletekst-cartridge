; @file runtime_loader.asm
; @brief ROM-resident ZX0 loader for the RAM-linked viewer core.
;
; SPDX-License-Identifier: GPL-3.0-only
; Included by crt0.asm after normal BSS/DATA initialization, with interrupts
; disabled. make_cartridge_rom.py validates both linker regions and stack space.

EXTERN asm_dzx0_standard
EXTERN __ROMABLE_END_tail, __DATA_head, __DATA_END_tail

; @brief Unpacks the self-terminating ZX0 block immediately following ROM DATA.
; @return No value; AF, BC, DE and HL are clobbered, and IX is unchanged.
unpack_runtime:
    ld hl,__ROMABLE_END_tail + (__DATA_END_tail - __DATA_head)
    ld de,Runtime_Start
    call asm_dzx0_standard
    ret
