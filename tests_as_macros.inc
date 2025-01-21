; Making test snippets as macros, to ensure they are identical in DOS and ROM format
; author: pal.hansen@gmail.com

.macro macroTEST_0_UNROLL
    inc a
.endm

.macro macroTEST_1_STARTUP
    ld hl, #0x8000              ; // this one is just random
    ld c, #0x98                 ; // VDPIO
.endm

.macro macroTEST_1_UNROLL
    outi
.endm

.macro macroTEST_2_UNROLL
    out (0x98), a               ; // VDPIO. This will break the speed limits.
.endm

.macro macroTEST_3_UNROLL
    in a, (0x98)                ; // VDPIO. This will break the speed limits.
.endm

.macro macroTEST_4_UNROLL
    in a, (0x98)                 ; // VDPIO. This will break the speed limits.
.endm

.macro macroTEST_5_STARTUP
                                ; // NOTE: Somewhat unclear the cycle cost of this
    ld a, #3                    ; // get status for sreg n (https://www.msx.org/wiki/VDP_Status_Registers)
    out (0x99), a               ; // VDPPORT1. status register number
    ld a, #0x8F                 ; // VDP register R#15 (write)
    out (0x99), a               ; // VDPPORT1. out VDP register number
.endm

.macro macroTEST_5_UNROLL
    in a, (0x99)                ; // VDPPORT1. This will break the speed limits.
.endm

.macro macroTEST_6_UNROLL
    out (0x9A), a               ; // VDPPALETTE. This will break the speed limits.
.endm

.macro macroTEST_7_STARTUP      ; // Sets VDPSTREAM-port to constantly overwrite reg 32 (SX: X-coordinate to be transferred (LOW))
                                ; // NOTE: Somewhat unclear the cycle cost of this due to two outs
	ld a, #128+32		        ; // Set "Stream mode", but "non-autoincrement mode"
	out (0x99), a
	ld a, #128+17
	out (0x99), a    	        ; // R#17 := 32
.endm

.macro macroTEST_7_UNROLL
    out (0x9B), a               ; // VDPSTREAM. This will break the speed limits.
.endm

.macro macroTEST_8_UNROLL
    in a, (0x06)                ; // Assuming this one is not in use
.endm