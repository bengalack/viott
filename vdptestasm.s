; ============================================================================
; vdptestasm.s - assembler companion part for vdptest.c
; Note: any symbol to be reached via C in SDCC is prefixed with an underscore
; Parameters are passed according to __sdcccall(1) found here:
; https://sdcc.sourceforge.net/doc/sdccman.pdf
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0

    .module vdptestasm
    .area _CODE

; ----------------------------------------------------------------------------
; CONSTANTS
    BIOS_CHPUT      .equ 0x00A2
    CALSLT          .equ 0x001C
    RDSLT           .equ 0x000C
    EXPTBL          .equ 0xFCC1

    INIPLT          .equ 0x0141
    RSTPLT          .equ 0x0145
    ; BDOS           .equ 0x0005             ; "Basic Disk Operating System"
    ; BDOS_STROUT    .equ 9                  ;.string output

    CHGMOD          .equ 0x005F             ; BIOS routine used to initialize the screen

    CHGCPU          .equ 0x0180             ; tame that turbo please
    GETCPU          .equ 0x0183

    VDPPORT1	    .equ 0x99

    VDP_REG9        .equ 0xFFE8             ; mirror
    FRQ_BITMASK     .equ #0b00000010        ; to be used with VDP_REG9

    NMI             .equ 0x0066             ; subrom stuff
    EXTROM          .equ 0x015f             ; subrom stuff
    H_NMI           .equ 0xfdd6             ; subrom stuff

    RTC_PORT_REG    .equ 0xB4               ; Reg 0-15
    RTC_PORT_DATA   .equ 0xB5               ; read/write, note this chip is 4 bits... (jeeeez)

    VDPIO           .equ 0x98               ; VRAM Data (Read/Write)
    VDPPORT1        .equ 0x99
    VDPPALETTE      .equ 0x9A
    VDPSTREAM       .equ 0x9B

;-------------------------
; Uses the RTC clock: https://www.msx.org/wiki/Real_Time_Clock_Programming
;
; You can do "in a, ( RTC_PORT_DATA )" repeatedly after this to poll value
;
; COST: 73 cycles (+ the call(18))
; IN: 		A - register in block 0
; OUT: 		-
; Modifies: AF, AF'
;-------------------------
; u8 setupClockForDigitRead( u8 uBlock0RegID );
_setupClockForDigitRead::

	ex		af, af'

	ld 		a, #13					; set reg 13 to set block
	out		( RTC_PORT_REG ), a
	ld		a, #8					; block 0 and (re-)"enable timer" seconds? think this one needs to be set to make clock tick
	out		( RTC_PORT_DATA ), a

	ex		af, af'
	out		( RTC_PORT_REG ), a

	ret


; Cost: 60+73 = 133 cycles
; trashes AF, AF'
.macro recordTime regid addr
    ld      a,#regid
    call    _setupClockForDigitRead
	in		a,(RTC_PORT_DATA)
	and		#0x0F
    ld      (addr),a
.endm

; ----------------------------------------------------------------------------
; This one runs fully in DI.
; ASSUMES:
;           that the test code is present at _runTestAsmInMem (normally 0x8000)
; MODIFIES: 
;
; void longTest(void);
_longTest::

    di

    ; set up for changing background color every time we send a byte to 0x9B port
    ld      a,#7|0x80               ; "R#7 border colour", but plan for NON-autoincrement
    out     (VDPPORT1),a
    ld      a,#17|0x80              ; reg write in "R#17 | indirect specification of R#n"
    out     (VDPPORT1),a

    ; set up for reading, in case we test reads
    ld      a,#1                    ; get status for S#1
    out		(VDPPORT1), a			; status register number
    ld		a,#0x8F				    ; VDP register R#15
    out		(VDPPORT1), a			; out VDP register number

    ld      de,#438                 ; total iterations of ROM-block (to be used far down)
    ld      c,#0x0F                 ; just a quick mask, as these are 4 bit only

    ; SYNCRONIZING with the RTC - we kick off RIGHT after the digit flipped
    ld      a,#0                    ; lower digit of the seconds (values 0-9)
    call    _setupClockForDigitRead

	in		a,(RTC_PORT_DATA)
	and		c

    add     a,#2                    ; inc by two seconds, to avoid a tick while doing these preps
    cp      #0x0A
    jr      c,weiter
    sub     #0x0A

weiter:
    ld      b, a

spin_0:                             ; spin until the right value occurs
	in		a, (RTC_PORT_DATA)
	and		c
    cp      b
    jp      nz,spin_0               ; this will loop forever when the clock is not working
    ; TIMING STARTS HERE (cycle counting)

    ld      (_g_uSecondsL0),a
    
    recordTime 1 _g_uSecondsH0      ; 133 cycles
    recordTime 2 _g_uMinsL0         ; 133 cycles
    recordTime 3 _g_uMinsH0         ; 133 cycles

    call    _runTestAsmInMem

    ; grab timestamp
encore_une_fois:
    recordTime 0 _g_uSecondsL1
    ld      b,a
    recordTime 1 _g_uSecondsH1
    recordTime 2 _g_uMinsL1
    recordTime 3 _g_uMinsH1

    ld      a,#0                    ; check if the second counter moved while we grabbed the timestamp. Re-fetch if so
    call    _setupClockForDigitRead
	in		a,(RTC_PORT_DATA)
	and		#0x0F
    cp      b
    jp      nz, encore_une_fois

    ; fix colors
    ld      a,#0xF4
    out     (VDPSTREAM),a

    ei
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
	out     (VDPPORT1), a       
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
; Uses Matsushita device
; from here: https://map.grauw.nl/resources/msx_io_ports.php#expanded_io
;
; MODIFIES:     AF, BC
; RETURN:       A (bool)
;
; bool hasTurboFeature(void) __preserves_regs(d,e,h,l,iyl,iyh);
_hasTurboFeature::

    ld      b, #0           ; return value, default 0 (false)
    in      a,(0x40)
    cpl
    ld      c,a
    ld      a,#8
    out     (0x40),a        ; out the manufacturer code 8 (Panasonic) to I/O port 40h
    in      a,(0x40)        ; read the value you have just written
    cpl                     ; complement all bits of the value
    cp      #8              ; if it does not match the value you originally wrote,
    jr      nz,bye_bye      ; it does not have the Panasonic expanded I/O ports
    in      a,(0x41)
    bit     2,a             ; is turbo mode available?
    jr      nz,bye_bye
    ld      b, #1           ; yes, it is enabled

bye_bye:

    ld      a,c
    out     (0x40),a
    ld      a, b
    ret

; ----------------------------------------------------------------------------
; Uses Matsushita device
; from here: https://map.grauw.nl/resources/msx_io_ports.php#expanded_io
;
; MODIFIES:     AF, BC
; RETURN:       A (bool)
;
; bool isTurboEnabled(void) __preserves_regs(d,e,h,l,iyl,iyh);
_isTurboEnabled::

    ld      c,#0x40
    in      a,(c)
    cpl
    ld      b,a
    ld      a,#8
    out     (c),a        ; out the manufacturer code 8 (Panasonic) to I/O port 40h

    in      a,(0x41)
    rra                     ; bit 0: is turbo mode on? 0==on
    ld      a,#0
    jr      c,bye_bye2
    inc     a
bye_bye2:
    out     (c),b
    ret

; ----------------------------------------------------------------------------
; Uses Matsushita device
; from here: https://map.grauw.nl/resources/msx_io_ports.php#expanded_io
;
; bit 0 is turbo or not. 0=turbo, 1=normal
;
; MODIFIES: AF, BC, D
;
; void enableTurbo(bool bEnable) __preserves_regs(e,h,l,iyl,iyh);
_enableTurbo::

    xor     #1              ; flip the bit
    ld      b,a

    ld      c,#0x40
    in      a,(c)
    cpl
    ld      d,a
    ld      a,#8
    out     (c),a           ; out the manufacturer code 8 (Panasonic) to I/O port 40h

    in      a,(0x41)
    and     #0b11111110
    or      b
    out     (0x41),a        ; enable turbo(?)

    out     (c),d           ; retstore org device
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