; Making test snippets as macros, to ensure they are identical in DOS and ROM format
; author: pal.hansen@gmail.com

.macro macroTEST_EMPTY
    ret
.endm

; May seem corny to have ei/di in a test timed using interrupt, but this one saves
; us from special handling of PC-reg calculations if interrupt occured inside this tail
; interrupt occuring inside this tail will introduce an inaccuracy between 0 and 45
; cycles, and will only happen on fast (turbo and R800) modes. This is at maximum:
; 0,055% deviation (using quick 5 cycle test operation, smaller deviation on other tests)
.macro macroTEST_TAIL   ; length:   7 bytes. 45 cycles. (FRAME_CYCLES_TAIL_Z80)
    ; di                  ; 1 byte,   5 cycles
    ; inc 0(iy)           ; 3 bytes, 25 cycles (add the round-counter)
    ; ei                  ; 1 byte,   5 cycles (ei happens after the next command)
    ; jp (ix)             ; 2 bytes, 10 cycles

    di                  ; 1 byte
    exx                 ; 1 byte
    inc (hl)            ; 1 byte, +2 cycles on turbo!
    exx                 ; 1 byte
    ei                  ; 1 byte
    jp (ix)             ; 2 bytes, 10 cycles
.endm



.macro macroTEST_0_UNROLL
    cpl
.endm

.macro macroTEST_1_UNROLL
    inc hl                      ; sync test. MUST NOT differ in speed on turbo (like adc (hl))
.endm

.macro macroTEST_2_UNROLL
    out (0x98), a               ; VDPIO. This will break the speed limits.
.endm

.macro macroTEST_3_UNROLL
    in a, (0x98)                ; VDPIO. This will break the speed limits.
.endm

.macro macroTEST_4_UNROLL
    in a, (0x98)                ; VDPIO. This will break the speed limits.
.endm

.macro macroTEST_4_2_STARTUP
    ld hl,#0xFB00               ; YSAVE #FB00 1 Light pen Y coordinate read from the device (internal use only).
    ret
.endm

.macro macroTEST_4_2_UNROLL
    ; adc (hl)                    ; gives one extra on turbo
    inc (hl)
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

.macro macroTEST_5_2_UNROLL
    adc a, 0(iy)
.endm

.macro macroTEST_6_UNROLL
    out (0x9A), a               ; VDPPALETTE. This will break the speed limits.
.endm

.macro macroTEST_6_2_UNROLL
    bit 0, 0(iy)
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

.macro macroTEST_7_2_UNROLL
    cp #5
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
    ld hl, #0x8000              ; Reads RAM in dos, ROM in ROM.
    ld d, h
    ld e, l
    ret
.endm

.macro macroTEST_B_UNROLL
    cpi
.endm

; =============================================================================

.macro macroTEST_LONG_UNROLL
    out (0x9b),a
;     out (0x9b),a    ; this is actually waaaaay to speedy
;     in a,(0x99)
.endm

; length of below block is 7 (SIZE_LONGTEST_TAIL)
; these cycles should be part of the calculation of available cycles in a frame
; but NOT part of calculation of a test duration / instruction cost
.macro macroTEST_LONG_TAIL
    dec de
    ld a,d
    or e
    jp nz,_runTestAsmInMem  ; cycles: 28
    ret                     ; cycles: 39
.endm

; TODO: Adapt for turbo speeds also
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
.macro macroTEST_LONG

.rept (0x4000-7)/2  ; = 8188 instructions
    macroTEST_LONG_UNROLL
.endm

    macroTEST_LONG_TAIL
.endm