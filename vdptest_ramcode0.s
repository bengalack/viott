; ============================================================================
; vdptest_ramcode.s - part that MUST be run from internal RAM and not ROM
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0

; ----------------------------------------------------------------------------
; CONSTANTS
    VDPPORT1	.equ 0x99

; ----------------------------------------------------------------------------
; EXTERNAL REFERENCES
    .globl      _g_bStorePCReg
    .globl      _g_pPCReg
    .globl      _g_pFncCurStartupBlock
    .globl      _runTestAsmInMem

_UPPERCODE_BEGIN::

; ------------------
; Common start for tests (found other places)
; Cost after halt: 68 + 5 (73) + the cost of _g_pFncCurStartupBlock (which we will find in TestDescriptor.uStartupCycleCost)
; ------------------
_commonStartForAllTests::
    halt                                ; ensure that the following commands are not interrupted (di/ei is not safe!)

    ld      a, #01
    ld      (_g_bStorePCReg), a         ; true

    ld      hl, (_g_pFncCurStartupBlock); 17
    call    call_hl                     ; 18

    jp      _runTestAsmInMem            ; 11

call_hl::
    jp      (hl)                        ; 5

; ------------------
; Common end
; ------------------
commonTestRetSpot::
    ret

; ----------------------------------------------------------------------------
; Resets VLANK IRQ, and when g_bStorePCReg is true, we store
; the PC reg and ALSO change the stack so that running program jumps to the end
; of the test (=ret). This latter part only to save time during tests.
; 
; Cost: 171 (on first, when g_bStorePCReg==true)
; + CPU kicking this off should be: +13+1 (13 according to this:
; http://www.z80.info/interrup.htm) and as MSX always has +1 cycle per M1, we
; add 1 cycle. Furthermore there is "JP _customISR" at 0x0038 (=11 cycles)
; Totals: 196 cycles
; MODIFIES: (No registers of course!)
_customISR::
    push	af
    push    bc
    push	hl

    xor 	a                       ; get status for sreg 0
    out		(VDPPORT1), a			; status register number
    ld		a, #0x8F				; VDP register R#15
    out		(VDPPORT1), a			; out VDP register number
    nop								; obey speed
    in		a, (VDPPORT1)			; read VDP S#n to reset VBLANK IRQ

    ld      a, (_g_bStorePCReg)     ; global switch on storing or not
    or      a
    jr      z, leave_isr

    ; -- BEGIN STORING PART --
	ld		hl, #0
	add		hl, sp

.rept 3*2							; the main program PC should be found on the stack
	inc		hl                      ; "stack pointer minus those double-byte values I have pushed on the stack"
.endm

	ld		a, (hl)                 ; store current PC-reg
	ld		(_g_pPCReg+0), a        ; low byte
	inc		hl
	ld		a, (hl)
	ld		(_g_pPCReg+1), a        ; high byte

    ld      bc, #commonTestRetSpot  ; force return-to address: where next instruction is RET
    ld      (hl), b                 ; this block is not 100% needed, but speeds up total execution (alt: dec sp x2 at the end)
    dec     hl
    ld      (hl), c

    xor     a
    ld      (_g_bStorePCReg), a     ; global switch set to off as it is now stored
    ; -- END STORING PART --

leave_isr:

    pop 	hl
    pop     bc
    pop		af
    ei
    ret

_UPPERCODE_END::