; This is the ROM-spec for the tests. It must be equivalent to the RAM
; setup, which is described in viott.c under:
;
;       const TestDescriptor g_aoTest[]
;
; author: pal.hansen@gmail.com
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0


    .include "tests_as_macros.inc"
    .globl commonStartForAllTests

; ----------------------------------------------------------------------------
    .area _SEG0 ; sync

_runTestAsmInMem::  ; this address is reused in every segment in page 2
    call    commonStartForAllTests

.rept 75000/5   ; divide by the cost of the unroll
    macroTEST_0_UNROLL
.endm
    ret

; ----------------------------------------------------------------------------
    .area _SEG1 ;   outi98

    call    commonStartForAllTests

    macroTEST_1_STARTUP
.rept 75000/18  ; divide by the cost of the unroll
    macroTEST_1_UNROLL
.endm
    ret
; ----------------------------------------------------------------------------
    .area _SEG2 ; out98

    call    commonStartForAllTests

.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_2_UNROLL
.endm
    ret

; ----------------------------------------------------------------------------
    .area _SEG3 ; in98

    call    commonStartForAllTests

.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_3_UNROLL
.endm
    ret

; ----------------------------------------------------------------------------
    .area _SEG4 ; in98x

    call    commonStartForAllTests
    
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_4_UNROLL
.endm
    ret

; ----------------------------------------------------------------------------
    .area _SEG5 ; in99

    call    commonStartForAllTests

    macroTEST_5_STARTUP
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_5_UNROLL
.endm
    ret

; ----------------------------------------------------------------------------
    .area _SEG6 ; out9A

    call    commonStartForAllTests

.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_6_UNROLL
.endm
    ret

; ----------------------------------------------------------------------------
    .area _SEG7 ; out9B

    call    commonStartForAllTests

    macroTEST_7_STARTUP
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_7_UNROLL
.endm
    ret

; ----------------------------------------------------------------------------
    .area _SEG8 ; (ex) in06

    call    commonStartForAllTests

.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_8_UNROLL
.endm
    ret














; ; ----------------------------------------------------------------------------
;      .area _SEG0
; _runTestAsmInMem::
; .rept 0x4000
;     NOP
; .endm

;     .area _SEG1
; .rept 0x4000
;     NOP
; .endm
;     .area _SEG2
; .rept 0x4000
;     NOP
; .endm
;     .area _SEG3
; .rept 0x4000
;     NOP
; .endm
;     .area _SEG4
; .rept 0x4000
;     NOP
; .endm
;     .area _SEG5
; .rept 0x4000
;     NOP
; .endm
;     .area _SEG6
; .rept 0x4000
;     NOP
; .endm
;     .area _SEG7
; .rept 0x4000
;     NOP
; .endm
;     .area _SEG8
; .rept 0x4000
;     NOP
; .endm
