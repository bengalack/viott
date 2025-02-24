; ---------------------------------------------------------------------------
; Search in slots and find all slot IDs for ROM and RAM.
; These routines were initially made for ROM development with NO BIOS usage.
; It takes one shortcut: RAM slot for use in page two is copied from page 3.
; It is also expected that this code runs from page 1
; To get this to work, you need to provide the variables mentioned below
; externally.
; 
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0
; ---------------------------------------------------------------------------

	.allow_undocumented
	.module slots

	.area _CODE

	.globl _g_uSlotidPage0BIOS
	.globl _g_uSlotidPage0RAM
	.globl _g_uSlotidPage2RAM
	.globl _g_uSlotidPage2ROM
	.globl _g_uCurSlotidPage0

BIOS_EXPTBL  .equ 0xfcc1
BIOS_SLTTBL  .equ 0xfcc5

;----------------------------------------------------------
; At startup. MSX2 - 64k https://www.msx.org/wiki/Develop_a_program_in_cartridge_ROM#Create_a_ROM_without_mapper
; Memory is epected to look like this:
; page 0: mainrom (FCC1)
; page 1: rom (cart) 
; page 2: ram 
; page 3: ram
;
; We will build ids on the format: E000SSPP for these, for fast use later
; unsigned char               g_uSlotidPage0BIOS;
; unsigned char               g_uSlotidPage0RAM;
; unsigned char               g_uSlotidPage2ROM;
; unsigned char               g_uSlotidPage2RAM;
; IN: 		
; OUT:
; MODIFIES: AF, BC, HL
;
; Page 0 will be restored to main-ROM at end of run
;
; extern void establishSlotIDsNI_fromC(); // same from C as in asm.
;
_establishSlotIDsNI_fromC::

	in		a, (0xA8)	    		; Current primary slots
	ld		b, a					; store in format P3P2P1P0

	; ------------------ PAGE 0 - BIOS
	ld  	a, (BIOS_EXPTBL+0)
	ld		(_g_uSlotidPage0BIOS), a

	; ; ------------------ PAGE 2 - RAM (CURRENT)
	; ld		a, b					; current P3P2P1P0
	; and		#0b00110000				; P2 only
	; rlca
	; rlca
	; rlca
	; rlca							; now moved into PP-spot (and is a base number)
	; ld		c, a					; c=a: 000000PP

	; ld		hl, #BIOS_EXPTBL
	; add     l 
	; ld		l, a
	; ld  	a, (hl) 				; these have only bit 7 set if any, ie. if bit 7 not set: value is 0
	; or 		a
	; jr  	z, page2ramslot_has_no_subslot

	; ld		a, c

	; ld		hl, #BIOS_SLTTBL
	; add		l
	; ld		l, a					; is now pointing the #( BIOS_SLTTBL + slot )

	; ld		a, (hl) 				; format S3S2S1S0
	; and		#0b00110000
	; rrca
	; rrca							; a now contains 0000SS00
	; or		#0b10000000				; expanded slot flag

	; ------------------ PAGE 2 - RAM (COPY FROM P3)
	ld		a, b					; current P3P2P1P0
	and		#0b11000000				; P3 only
	rlca
	rlca
	ld		c, a					; c=a: 000000PP

	ld		hl, #BIOS_EXPTBL
	add     l 
	ld		l, a
	ld  	a, (hl) 				; these have only bit 7 set if any, ie. if bit 7 not set: value is 0
	or 		a
	jr  	z, page2ramslot_has_no_subslot

	ld		a, c

	ld		hl, #BIOS_SLTTBL
	add		l
	ld		l, a					; is now pointing the #( BIOS_SLTTBL + slot )

	ld		a, (hl)	    			; format P3P2P1P0
	and		#0b11000000
	rrca
	rrca
	rrca
	rrca							; a now contains 0000SS00
	or		#0b10000000				; expanded slot flag

page2ramslot_has_no_subslot: 		; a is 0 at this point, IF jumped here
	or		c
	ld		(_g_uSlotidPage2RAM), a

	; ------------------ PAGE 2 - ROM (COPY FROM CURRENT PAGE1)
	ld		a, b					; current P3P2P1P0
	and		#0b00001100				; P2 only
	rrca 
	rrca
	ld		c, a					; c=a: 000000PP

	ld		hl, #BIOS_EXPTBL
	add     l 
	ld		l, a
	ld  	a, (hl)				    ; these have only bit 7 set if any, ie. if bit 7 not set: value is 0
	or 		a
	jr  	z, page2romslot_has_no_subslot

	ld		a, c

	ld		hl, #BIOS_SLTTBL
	add		l						; just an opposite "l = l + a"
	ld		l, a					; is now pointing the #( BIOS_SLTTBL + slot )

	ld		a, (hl) 				; format P3P2P1P0			- we want to copy the same subslot as page one here as well.
	and		#0b00001100				; a now contains 0000SS00
	or		#0b10000000				; expanded slot flag

page2romslot_has_no_subslot: 		; a is 0 at this point, IF jumped here
	or		c
	ld		(_g_uSlotidPage2ROM), a

	; ------------------ PAGE 0 - RAM (RAM IS DETECTED FROM A RAM SEARCH)
	call 	searchRamSlotIDPage0
	ld		(_g_uSlotidPage0RAM), a

	ret

;----------------------------------------------------------
; Goes through all slots and subslots in page 0 and tests
; if memory is writeable.
; It does not handle a situation where nothing is found
; there should always be possible to find a slot in an
; MSX2 that has ram in page0
; It will be restored to main-ROM in page 0 (at end of run)
;
; IN:		
; OUT: 		A - SlotID in std slot format E000SSPP
; MODIFIES: AF, BC, DE
;
searchRamSlotIDPage0::

	ld 		b, #255						; holds primary slot... counter

outer_loop::
	inc 	b

	ld		hl, #BIOS_EXPTBL
	ld		a, b
	add 	l
	ld		l, a						; hl now holds the address of the BIOS_EXPTBL for the right primary slot

	ld		d, (hl)					    ; d now holds the expanded-bit
	ld 		c, #255						; holds secondary slot... counter

inner_loop::
	inc 	c							; c is secondary slot

	ld		a, c 						; Start baking up slotid, E000SSPP
	rlca
	rlca								; 0000SS00
	or		b							; 0000SSPP
	or		d 							; E000SSPP

	ld		e, a						; store in e (cannot push af easily, as return is in F below)
	call 	enableSlotInPage0_NI
	call 	isRamSlotIDPage0
	ld		a, e

	jp		z, isFound

	bit		7, d						; test if it is set. a "bitwise cp"
	jr		z, end_outer_loop

	ld 		a, c
	cp      #3

	jr  	nz, inner_loop

end_outer_loop::
	ld 		a, b
	cp		#3
	jr 		nz, outer_loop

isFound::

	ld		a, (BIOS_EXPTBL)
	call 	enableSlotInPage0_NI	    ; restore BIOS

	ld		a, e						; return in a. E SHOULD have been set above

	ret

;----------------------------------------------------------
; Helper-routine, tests if we can write at a mem-loc in page 0
; IN:		
; OUT: 		z-flag. Z is set: RAM. Z is not set: ROM
; MODIFIES: AF
isRamSlotIDPage0::
	push	bc
	ld 		a, (0x0037) 				; Pretty much a random address
	ld 		c, a
	inc 	a
	ld		b, a
	ld 		(0x0037), a
	ld 		a, (0x0037)
	cp		b

	ld		a, c
	ld 		(0x0037), a				    ; restore, in case the ram really was nvram... we don't want to mess up

	pop 	bc

	ret


;----------------------------------------------------------
; Select this slot and subslot in page 0
; This code must reside in slot 1
; IN: SlotID in std slot format E000SSPP
;
; extern void memAPI_enaSltPg0_NI_fromC( unsigned char uSlotID );
_memAPI_enaSltPg0_NI_fromC::
	call	enableSlotInPage0_NI
	ret

;----------------------------------------------------------
; Select this slot and subslot in page 2
; This code must reside in slot 1
; extern void memAPI_enaSltPg2_NI_fromC( unsigned char uSlotID );
_memAPI_enaSltPg2_NI_fromC::
	call	enableSlotInPage2_NI
	ret

;----------------------------------------------------------
; Select this slot and subslot in page 0
; Store this value in g_uCurSlotidPage0 (ram in page 3) as well
; This code must reside in slot 1
; IN: 		A - SlotID in std slot format E000SSPP
; OUT:
; MODIFIES: AF
; Cost: 294/145
; Size: 48 bytes
enableSlotInPage0_NI::

	push	bc
	push	de

	ld		(_g_uCurSlotidPage0), a

	ld		c, a

	; ----------------------  Set primary first
	and		#0b00000011     ; keep "PP"-value only
	ld		b, a
    in      a, (0xA8)       ; read slot value. Format P3P2P1P0 ; http://map.grauw.nl/resources/msx_io_ports.php#ppi
	and		#0b11111100     ; reset old "P0"-value
	or		b				; set new "P0" as PP
	out 	(0xA8), a	    ; Set in effect
	ld      d, a 			; possible store for later

	; ----------------------  Now set secondary, if any
	ld		a, c
	bit		7, a
	jr		z, bail0		; if not expanded, we are done!

							; if expanded, we must set secondary slot. To do this:
							; 	read current primary slot for all pages,  - it is actually stored in d
							; 	set same the PP slot in page 3
							; 	set secondary SS for P0, format: S3S2S1S0 at 0xFFFF: http://map.grauw.nl/resources/msxsystemvars.php
							; 	restore XX for all slots

	ld 		a, c
	and		#0b00000011     ; keep "PP"-value only

	rrca
	rrca					; move to P3 pos
	ld      b, a			; store temporary in b
	ld  	a, d			; get current config
    and     #0b00111111     ; keep P0-P2
	or      b
	out 	(0xA8), a	    ; Set in effect

	ld		a, (0xFFFF)	
	cpl						; a holds current SS for the whole slot: S3S2S1S0
	and		#0b11111100		; mask away current S0
	ld		e, a			; store in e

	ld		a, c			; get org parameter
    and     #0b00001100     ; single out SS
	rrca
	rrca					; move to S0 pos
	or		e
	ld		(0xFFFF), a	    ; set S0 (S3S2S1S0) in effect

	ld		a, d			; restore wanted primary slots
	out 	(0xA8), a	    ; Set in effect

bail0::
	pop		de
	pop		bc
	ret

;----------------------------------------------------------
; Select this slot and subslot in page 2
; This code must reside in slot 1
; To avoid any problems with stack, stack is NOT used
; IN: 		A - SlotID in std slot format E000SSPP
; OUT:
; MODIFIES: AF, BC, DE
; Cost: 247/81 cycles
; Size: 48 bytes
enableSlotInPage2_NI::

	; ld		(_g_uCurSlotidPage2), a

	ld		c, a

	; ----------------------  Set primary first
	and		#0b00000011     ; keep "PP"-value only
	rlca
	rlca
	rlca
	rlca					; Move to P2-pos
	ld		b, a
    in      a, (0xA8)       ; read slot value. Format P3P2P1P0 ; http://map.grauw.nl/resources/msx_io_ports.php#ppi
	and		#0b11001111     ; reset old "P2"-value
	or		b				; set new "P2" as PP
	out 	(0xA8), a	    ; Set in effect
	ld      d, a 			; possible store for later

	; ----------------------  Now set secondary, if any
	ld		a, c
	bit		7, a
	ret		z				; if not expanded, we are done!

							; if expanded, we must set secondary slot. To do this:
							; 	read current primary slot for all pages,  - it is actually stored in d
							; 	set same, the PP, slot in page 3
							; 	set secondary SS for P0, format: S3S2S1S0 at 0xFFFF: http://map.grauw.nl/resources/msxsystemvars.php
							; 	restore XX for all slots

	ld 		a, c
	and		#0b00000011     ; keep "PP"-value only

	rrca
	rrca					; move to P3 pos
	ld      b, a			; store temporary in b
	ld  	a, d			; get current config
    and     #0b00111111     ; keep P0-P2
	or      b
	out 	(0xA8), a	    ; Set in effect

	ld		a, (#0xFFFF)	
	cpl						; a holds current SS for the whle slot: S3S2S1S0
	and		#0b11001111		; mask away current S2
	ld		e, a			; store in e

	ld		a, c			; get org parameter
    and     #0b00001100     ; single out SS
	rlca
	rlca					; move to S2 pos
	or		e
	ld		(#0xFFFF), a	; set S2 (S3S2S1S0) in effect

	ld		a, d			; restore wanted primary slots
	out 	(0xA8), a	    ; Set in effect

	ret