; This is the ROM-spec for the tests. It must be equivalent to the RAM
; setup, which is described in vdptest.c under:
;
;       const TestDescriptor g_aoTest[]
;
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0


    .include "tests_as_macros.inc"

    .area _SEG1 ; outdi98
_runTestAsmInMem::  ; this address is reused in every segment in page 2
.rept 75000/(2*18) ; divide by the cost of the unroll
    macroTEST_1_UNROLL
.endm
    ret

    .area _SEG2 ; out98
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_2_UNROLL
.endm
    ret

    .area _SEG3 ; in98
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_3_UNROLL
.endm
    ret

    .area _SEG4 ; in98x
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_4_UNROLL
.endm
    ret

    .area _SEG5 ; in99
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_5_UNROLL
.endm
    ret

    .area _SEG6 ; out9A
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_6_UNROLL
.endm
    ret

    .area _SEG7 ; out9B
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_7_UNROLL
.endm
    ret

    .area _SEG8 ;   outi98
.rept 75000/18  ; divide by the cost of the unroll
    macroTEST_8_UNROLL
.endm
    ret

    .area _SEG9 ; !in06FMT
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_9_UNROLL
.endm
    ret

    .area _SEGA ; !in06RAM
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_9_UNROLL
.endm
    ret    