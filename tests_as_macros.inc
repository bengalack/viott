; Making test snippets as macros, to ensure they are identical in DOS and ROM format
; author: pal.hansen@gmail.com

.macro macroTEST_EMPTY
    ret
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

