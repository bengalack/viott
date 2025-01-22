; ============================================================================
; HEAP / RAM (only valid in MSXDOS mode, runTestAsmInMem is defined in segs in ROM)
; VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0
;
    .area _HEAP
_runTestAsmInMem:: ; test code to be copied in here, in the heap (after ram variables)
