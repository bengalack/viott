; ============================================================================
; vdptestasm.s - assembler companion part for voitt.c
; note: any symbol to be reached via C in SDCC is prefixed with an underscore
; any parameters are passed according to __sdcccall(1) found here:
; https://sdcc.sourceforge.net/doc/sdccman.pdf
; author: pal.hansen@gmail.com

    .allow_undocumented
    .area _CODE

; ----------------------------------------------------------------------------
; CONSTANTS
    BIOS_CHPUT  .equ 0x00A2
    CALSLT      .equ 0x001C
    RDSLT       .equ 0x000C
    EXPTBL      .equ 0xFCC1

    INIPLT      .equ 0x0141
    RSTPLT      .equ 0x0145
    ; BDOS        .equ 0x0005             ; "Basic Disk Operating System"
    ; BDOS_STROUT .equ 9                  ;.string output

    CHGMOD      .equ 0x005F             ; BIOS routine used to initialize the screen

    CHGCPU      .equ 0x0180             ; tame that turbo please
    GETCPU      .equ 0x0183

    VDPIO		.equ 0x98				; VRAM Data (Read/Write)
    VDPPORT1	.equ 0x99

    VDP_REG9    .equ 0xFFE8             ; mirror
    FRQ_BITMASK .equ #0b00000010        ; to be used with VDP_REG9

    NMI         .equ 0x0066             ; subrom stuff
    EXTROM      .equ 0x015f             ; subrom stuff
    H_NMI       .equ 0xfdd6             ; subrom stuff

; ----------------------------------------------------------------------------
; EXTERNAL REFERENCES
    .globl      _g_bStorePCReg
    .globl      _g_pPCReg

; ----------------------------------------------------------------------------
; Below are a few chunks of tests. A small kickoff chunk with a halt before
; massive unrolled out/variants

; ------------------
; Common start
; Cost after halt: 33 (includes ret cost)
; ------------------
commonStartForAllTests:
    halt                                ; ensure that the following commands are not interrupted (di/ei is not safe!)

    ld      a, #01
    ld      (_g_bStorePCReg), a         ; true

    ret

; ------------------
; Common end
; ------------------
commonTestRetSpot::
    ret

_TEST_START_BLOCK_BEGIN::                ; this will be copied into RAM(HEAP) where the test will be run
    call    commonStartForAllTests
    ; ... rest of the test code comes here ...
_TEST_START_BLOCK_END::

; ----------------------------------------------------------------------------
; MODIFIES: AF
;
; bool getPALRefreshRate();
_getPALRefreshRate::
    ld      a, (VDP_REG9)
    and     #FRQ_BITMASK
    srl     a
    ret

; ----------------------------------------------------------------------------
; IN:       A:  1 if PAL, 0 if NTSC
; MODIFIES: AF, B
; void setPALRefreshRate(bool bPAL);
_setPALRefreshRate::

    sla     a
    ld      b, a   

    ld      a, (VDP_REG9)
    and     #~FRQ_BITMASK
    or      b
    ld      (VDP_REG9), a

    di

	out 	(VDPPORT1), a
	ld  	a, #9|0x80
	out 	(VDPPORT1), a

    ei
    ret

; ----------------------------------------------------------------------------
; Enable VDP port #98 for start writing at address (A&3)DE 
; IN:       A:  Bits: 0W0000UU, W = Write, U means Upper VRAM address(bit 17-18)
;           DE: VRAM address, 16 lowest bits
; MODIFIES: AF, B, DE
; setVRAMAddressNI(u8 uBitCodes, u16 nVRAMAddress);
_setVRAMAddressNI::

    ld      b, a
    and     #3                      ; first bits

	rlc     d
	rla
	rlc     d
	rla
	srl     d
	srl     d

	out 	(VDPPORT1), a           ; set bits 14-16
	ld  	a, #14|0x80             ; indicate value being a register by setting bit 7
	out 	(VDPPORT1), a           ; in reg 14

	ld      a, e                    ; set bits 0-7
	out     (#VDPPORT1), a

    ld      a, b                    ; prepare write flag in b
    and     #0b01000000
    ld      b, a   

	ld      a, d                    ; set bits 8-13
	or      b                       ; + write access via bit 6?
	out     (#VDPPORT1), a       
    ret

; ----------------------------------------------------------------------------
; Resets VLANK IRQ, and when g_bStorePCReg is true, we store
; the PC reg and ALSO change the stack so that running program jumps to the end
; of the test (=ret). This latter part only to save time during tests.
; 
; Cost: 171 (on first, when g_bStorePCReg==true)
; + CPU kicking this off should be: +13+1 (13 according to this:
; http://www.z80.info/interrup.htm) and then I assume there is at least one M1
; wait cycle. Furthermore there is "JP _customISR" at 0x0038 (=11 cycles)
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
 
; ----------------------------------------------------------------------------
; Print to console. Both '\r\n' is needed for a carriage return and newline.
; Heavy(!), as it does interslot calls per character (but print performance is
; of no concern in this program)
; IN:       HL - pointer to zero-terminated string
; MODIFIES: ? (BIOS...)
; void print(u8* szMessage)
_print::

    ; ; BDOS Variant (needs $ as ending character)
    ; ex      de, hl                  ; p to msg in de
    ; ld      c, #BDOS_STROUT         ; function code
    ; jp      BDOS

    ; BIOS variant (heavy)
    push    ix
loop:
	ld      a, (hl)
	and     a
	jr      z, leave_me
    ld      ix, #BIOS_CHPUT
    call    callSlot

	inc     hl
	jr      loop

leave_me:
    pop     ix
    ret

; ----------------------------------------------------------------------------
; MSX version number http://map.grauw.nl/resources/msxsystemvars.php
;
; 0 = MSX 1
; 1 = MSX 2
; 2 = MSX 2+
; 3 = MSX turbo R
;
; MODIFIES: ? (BIOS...)
; u8 getMSXType()
_getMSXType::
    push    ix                  ; just in case, as SDCC is peculiar about this register
    ld      a, (EXPTBL)         ; BIOS slot
    ld      hl, #0x002D         ; Location to read
    di
    call    RDSLT               ; interslot call. RDSLT needs slot in A, returns value in A. address in HL
    pop     ix
    ret

; --------------------
; Tiny internal helper
; IN:       IX: address of BIOS routine
callSlot:
    ld     iy, (EXPTBL-1)       ;BIOS slot in iyh
    jp      CALSLT              ;interslot call

; ----------------------------------------------------------------------------
; https://map.grauw.nl/resources/msxbios.php#msxtrbios
; IN:  A = 0 0 0 0 0 0 x x
;                      0 0 = Z80 (ROM) mode
;                      0 1 = R800 ROM  mode
;                      1 0 = R800 DRAM mode
;
; MODIFIES: ? (BIOS...)
; void change CPU();
_changeCPU::

    push    ix
    ld      ix, #CHGCPU
    call    callSlot
    pop     ix
    ret

; ----------------------------------------------------------------------------
; https://map.grauw.nl/resources/msxbios.php#msxtrbios
; OUT: A = 0 0 0 0 0 0 x x
;                      0 0 = Z80 (ROM) mode
;                      0 1 = R800 ROM  mode
;                      1 0 = R800 DRAM mode
;
; MODIFIES: ? (BIOS...)
; u8 getCPU();
_getCPU::

    push    ix
    ld      ix, #GETCPU
    call    callSlot
    pop     ix
    ret

; ----------------------------------------------------------------------------
; Set screen.
; IN:       A - mode, as in screen (https://www.msx.org/wiki/SCREEN)
; MODIFIES: ? (BIOS...)
; u8 changeMode(u8 uModeNum)
_changeMode::

    push    ix
    ld      ix, #CHGMOD
    call    callSlot
    pop     ix
    ret

; ----------------------------------------------------------------------------
; CALSUB - from: https://map.grauw.nl/sources/callbios.php
;
; In: IX = address of routine in MSX2 SUBROM
;     AF, HL, DE, BC = parameters for the routine
;
; Out: AF, HL, DE, BC = depending on the routine
;
; Changes: IX, IY, AF', BC', DE', HL'
;
; Call MSX2 subrom from MSXDOS. Should work with all versions of MSXDOS.
;
; Notice: NMI hook will be changed. This should pose no problem as NMI is
; not supported on the MSX at all.
;
CALSUB:
    exx
    ex      af, af'       ; store all registers
    ld      hl, #EXTROM
    push    hl
    ld      hl, #0xC300
    push    hl           ; push NOP ; JP EXTROM
    push    ix
    ld      hl, #0x21DD
    push    hl           ; push LD IX,<entry>
    ld      hl, #0x3333
    push    hl           ; push INC SP; INC SP
    ld      hl, #0
    add     hl, sp        ; HL = offset of routine
    ld      a, #0xC3
    ld      (H_NMI), a
    ld      (H_NMI + 1), hl ; JP <routine> in NMI hook
    ex      af, af'
    exx                 ; restore all registers
    ld      ix, #NMI
    ld      iy, (EXPTBL - 1)
    call    CALSLT       ; call NMI-hook via NMI entry in ROMBIOS
                        ; NMI-hook will call SUBROM
    exx
    ex      af, af'       ; store all returned registers
    ld      hl, #10
    add     hl, sp
    ld      sp, hl        ; remove routine from stack
    ex      af, af'
    exx                 ; restore all returned registers
    ret


; ----------------------------------------------------------------------------
; Init/Stores palette in VRAM
; IN:       -
; MODIFIES: ? (BIOS...)
; void initPalette()
_initPalette::

    push    ix
    ld      ix, #INIPLT
    call    CALSUB
    pop     ix
    ret

; ----------------------------------------------------------------------------
; Restores palette from VRAM
; IN:       -
; MODIFIES: ? (BIOS...)
; void restorePalette()
_restorePalette::

    push    ix
    ld      ix, #RSTPLT
    call    CALSUB
    pop     ix
    ret

; ============================================================================
; HEAP / RAM
; 
    .area _HEAP

_runTestAsmInHeap:: ; test code to be copied in here, in the heap (after ram variables)