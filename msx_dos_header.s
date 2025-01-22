; MSXDOS starts at 0x100
; (derived from https://github.com/Konamiman/MSX/blob/master/SRC/SDCC/crt0-msxdos/crt0msx_msxdos_advanced.asm)
; author: pal.hansen@gmail.com
;

	.globl	_main

	.area _HEADER (ABS)
	.area _CODE
	; .org 0x0100

msx_dos_entry_point::

    call    _main

    ; Standard program termination. 
    ld      c, #0x62                ; DOS 2 function for program termination. Termination code for DOS 2 was returned in a from _main
    ld      b, a
    call    5			            ; On DOS 2 this terminates; on DOS 1 this returns...
    ld      c, #0x0
    jp      5			            ; ... and then this one terminates (DOS 1 function for program termination).
