; Minimal Z88DK classic CRT for the P2000T slot-1 cartridge.
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
defm "P2WP C DEV "

start:
    INCLUDE "crt/classic/crt_init_sp.inc"
    call crt0_init
    INCLUDE "crt/classic/crt_init_atexit.inc"
    INCLUDE "crt/classic/crt_init_heap.inc"
    ei
    call _main

__Exit:
    jp __Exit

l_dcal:
    jp (hl)

INCLUDE "crt/classic/crt_runtime_selection.inc"

defc __crt_model = 1
INCLUDE "crt/classic/crt_section.inc"
