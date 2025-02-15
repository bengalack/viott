; This is the ROM-spec for the tests. It must be equivalent to the RAM
; setup, which is described in vdptest.c under:
;
;       const TestDescriptor g_aoTest[]
;
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0


    .include "tests_as_macros.inc"

    .area _SEG1 ; out98 --------------------------
_runTestAsmInMem::  ; this address/symbol is reused in every segment in page 2
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_2_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG2 ; in98 ---------------------------
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_3_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG3 ; in98x --------------------------
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_4_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG4 ; in99 ---------------------------
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_5_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG5 ; out9A --------------------------
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_6_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG6 ; out9B --------------------------
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_7_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG7 ; outi98 -------------------------
.rept 75000/18  ; divide by the cost of the unroll
    macroTEST_8_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG8 ; !in06 --------------------------
.rept 75000/12  ; divide by the cost of the unroll
    macroTEST_9_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEG9 ; !inca --------------------------
.rept 75000/5  ; divide by the cost of the unroll
    macroTEST_A_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEGA ; !cpi ---------------------------
.rept 75000/18  ; divide by the cost of the unroll
    macroTEST_B_UNROLL
.endm
    macroTEST_END_SPIN

    .area _SEGB ; longtest ---------------------- 
    macroTEST_LONG