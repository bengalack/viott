	.globl	_main

	.area _HEADER (ABS)
	.area _CODE

msx_rom_header::
;----------------------------------------------------------
;	ROM Header
	.db		#0x41				; ROM ID
	.db		#0x42				; ROM ID
	.dw		#init				; Program start
	.dw		#0x0000				; BASIC's CALL instruction not expanded
	.dw		#0x0000				; BASIC's IO DEVICE not expanded
	.dw		#0x0000	        	; BASIC program
	.dw		#0x0000				; Reserved
	.dw		#0x0000				; Reserved
	.dw		#0x0000				; Reserved

init: 							; will enter in DI initially!
	jp		_main				

