; Making test snippets as macros, to ensure they are identical in DOS and ROM format
; author: pal.hansen@gmail.com

.macro macroTEST_EMPTY
    ret
.endm

.macro macroTEST_END_SPIN ?spin; length: 6 bytes. Test cannot return, as it will mess up the presumed stack pointer
    ld a, #1
    ld (_g_bTooFast), a ; true
spin:
    jr spin
.endm

.macro macroTEST_0_UNROLL
    inc a
.endm

.macro macroTEST_1_UNROLL
    inc hl
.endm

.macro macroTEST_2_UNROLL
    out (0x98), a               ; VDPIO. This will break the speed limits.
.endm

.macro macroTEST_3_UNROLL
    in a, (0x98)                ; VDPIO. This will break the speed limits.
.endm

.macro macroTEST_4_UNROLL
    in a, (0x98)                 ; VDPIO. This will break the speed limits.
.endm

.macro macroTEST_5_STARTUP
    ld a, #3                    ; get status for sreg n (https://www.msx.org/wiki/VDP_Status_Registers)
    out (0x99), a               ; VDPPORT1. status register number
    ld a, #0x8F                 ; VDP register R#15 (write)
    out (0x99), a               ; VDPPORT1. out VDP register number
    ret
.endm

.macro macroTEST_5_UNROLL
    in a, (0x99)                ; VDPPORT1. This will break the speed limits.
.endm

.macro macroTEST_6_UNROLL
    out (0x9A), a               ; VDPPALETTE. This will break the speed limits.
.endm

.macro macroTEST_7_STARTUP      ; Sets VDPSTREAM-port to constantly overwrite reg 32 (SX: X-coordinate to be transferred (LOW))
	ld a, #128+32		        ; Set "Stream mode", but "non-autoincrement mode"
	out (0x99), a
	ld a, #128+17
	out (0x99), a    	        ; R#17 := 32
    ret
.endm

.macro macroTEST_7_UNROLL
    out (0x9B), a               ; VDPSTREAM. This will break the speed limits.
.endm

.macro macroTEST_8_STARTUP
    ld hl, #0x8000              ; Reads RAM in dos, ROM in ROM
    ld c, #0x98                 ; VDPIO
    ret
.endm

.macro macroTEST_8_UNROLL
    outi
.endm

.macro macroTEST_9_UNROLL
    in a, (0x06)                ; Assuming this one is not in use
.endm

.macro macroTEST_A_UNROLL
    inc a
.endm

.macro macroTEST_B_STARTUP
    ld hl, #0x8000              ; Reads RAM in dos, ROM in ROM. 22
    ret
.endm

.macro macroTEST_B_UNROLL
    cpi
.endm

.macro macroTEST_LONG
; The max possible amount of "OUT(n),a" instructions in one 60Hz frame is:
; 59736/12 = 4978, ie. for every extra cycle on an I/O command in a normalized frame,
; there will be added 4978 cycles. Lets run so many frames that the excess cycles,
; at least, amounts to ONE FULL SECOND. That is:
; 60*(59736/4978) = 60*12 = 720, or 12 seconds
; 
; in cycles, this is 12*60*59736 = 43009920, or 3584160 outs.
; with 8188 outs in one page (16384 bytes), that is 438 times of looping over that page with unrolled outs
;
; assumes:
; de is set as counter (438)
; vdp is set up to do write to a reg (border?)
; in case of using "in" status reg should be set too.



; .rept (0x4000-7)/2  ; = 8188 instructions
;     out (0x9b),a
; .endm
.rept (0x4000-7)/4  ; = 8188 instructions
    out (0x9b),a    ; this is actually waaaaay to speedy
    in a,(0x99)
.endm

    dec de
    ld a,d
    or e
    jp nz,_runTestAsmInMem
    ret
.endm