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
    ; BDOS        .equ 0x0005             ; "Basic Disk Operating System"
    ; BDOS_STROUT .equ 9                  ;.string output

    CHGMOD      .equ 0x005F             ; BIOS routine used to initialize the screen
    ; LINL40      .equ 0xF3AE             ; Screen width

    CHGCPU      .equ 0x0180             ; tame that turbo please
    GETCPU      .equ 0x0183

    VDPIO		.equ 0x98				; VRAM Data (Read/Write)
    VDPPORT1	.equ 0x99

    VDP_REG9    .equ 0xFFE8             ; mirror
    FRQ_BITMASK .equ #0b00000010        ; to be used with VDP_REG9

; ----------------------------------------------------------------------------
; EXTERNAL REFERENCES
    .globl      _g_bStorePCReg
    .globl      _g_bToggle
    .globl      _g_pPCReg

; ----------------------------------------------------------------------------
; Below are a few chunks of tests. A small kickoff chunk with a halt before
; massive unrolled out/variants

; ------------------
; Common start
; ------------------
commonStartForAllTests:

    halt                                ; ensure that the following commands are not interrupted (di/ei is not safe!)

    xor     a
    ld      (_g_bToggle), a             ; false
    inc     a
    ld      (_g_bStorePCReg), a         ; true

    ret

; ------------------
; Common end
; ------------------
commonTestRetSpot::
    ret

; ------------------
; OUTI (ADD_ENTRY)
; ------------------
_testRunOUTI::
    call    commonStartForAllTests
    ld      hl, #0x0000                 ; this one is just random
    ld      c, #VDPIO  
    halt

_testRunOUTIBaseline::
.rept (75000/18)                        ; repeats are, at max, a huge frame (PAL) split into the minimum cost of an instruction
    outi
.endm

    ret

; ------------------
; OUT (ADD_ENTRY)
; ------------------
_testRunOUT::
    call    commonStartForAllTests
    ld      a, #0x00                    ; this one is just random
    halt

_testRunOUTBaseline::
.rept (75000/12)                        ; repeats are, at max, a huge frame (PAL) split into the minimum cost of an instruction
    out     (VDPIO), a                     ; this will break the speed limits, but our focus is something else
.endm

    ret

; ------------------
; IN (ADD_ENTRY)
; ------------------
_testRunIN::
    call    commonStartForAllTests
    halt

_testRunINBaseline::
.rept (75000/12)                        ; repeats are, at max, a huge frame (PAL) split into the minimum cost of an instruction
    in      a, (VDPIO)                  ; this will break the speed limits, but our focus is something else
.endm

    ret

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
;
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
;
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
; Resets VLANK IRQ, and when g_bStorePCReg and g_bToggle are true, we store
; the PC reg and ALSO change the stack so that running program jumps to the end
; of the test (=ret). This latter part only to save time during tests.
; 
; Cost: 215 (on first, when g_bStorePCReg==true and g_bToggle==0)
; + CPU kicking this off should be: +13+1 (13 according to this:
; http://www.z80.info/interrup.htm) and then I assume there is at least one M1
; wait cycle. Furthermore there is "JP _customISR" at 0x0028 (=18 cycles)
; Totals: 247 cycles
;
; MODIFIES: (No registers of course!)
;
_customISR::
    push	af
    push    bc
    push	hl

    xor 	a                       ; get status for sreg 0
    out		(VDPPORT1), a			; status register number
    ld		a, #0x8F				; VDP register R#15 (and set 7th bit to signal reg)
    out		(VDPPORT1), a			; out VDP register number
    nop								; obey speed
    in		a, (VDPPORT1)			; read VDP S#n to reset VBLANK IRQ

    ld      a, (_g_bStorePCReg)     ; global switch on storing or not
    or      a
    jr      z, leave_isr

    ld      a, (_g_bToggle)         ; should be 0 on first run, to avoid storing and force-jumping
    xor     #1
    ld      (_g_bToggle), a
    jr      nz, leave_isr           ; if this is the first/forced interrupt, we bail

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
; 
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
;
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
;
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
;
; u8 getCPU();
_getCPU::

    push    ix
    ld      ix, #GETCPU
    call    callSlot
    pop     ix
    ret

; ----------------------------------------------------------------------------
; Set screen.
;
; IN:       A - mode, as in screen (https://www.msx.org/wiki/SCREEN)
; 
; MODIFIES: ? (BIOS...)
;
; u8 changeMode(u8 uModeNum)
_changeMode::

    push    ix
    ld      ix, #CHGMOD
    call    callSlot
    pop     ix
    ret

