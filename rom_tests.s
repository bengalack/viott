; This is the ROM-spec for the tests. It must be equivalent to the RAM
; setup, which is described in vdptest.c under:
;
;       const TestDescriptor g_aoTest[]
;
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0


    .include "tests_as_macros.inc"

    .area _SEG1 ; out98 --------------------------
_runTestAsmInMem::  ; this address/symbol is reused in every segment in page 2

.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_2_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG2 ; in98 ---------------------------
.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_3_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG3 ; in98x --------------------------
.rept (0x4000-7)/2   ; divide by the bytesize of the unroll
    macroTEST_4_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG4 ; in99 ---------------------------
.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_5_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG5 ; out9A --------------------------
.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_6_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG6 ; out9B --------------------------
.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_7_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG7 ; outi98 -------------------------
.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_8_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG8 ; !in06 --------------------------
.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_9_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEG9 ; !inca --------------------------
.rept (0x4000-7)/1  ; divide by the bytesize of the unroll
    macroTEST_A_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEGA ; !cpi ---------------------------
.rept (0x4000-7)/2  ; divide by the bytesize of the unroll
    macroTEST_B_UNROLL
.endm
    macroTEST_TAIL  ; this one has length 7 bytes (SIZE_TAIL_BLOCK)

    .area _SEGB ; longtest ---------------------- 
    macroTEST_LONG