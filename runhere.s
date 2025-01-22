; ============================================================================
; HEAP / RAM (only valid in MSXDOS mode, runTestAsmInMem is defined in segs in ROM)
; 
    .area _HEAP
_runTestAsmInMem:: ; test code to be copied in here, in the heap (after ram variables)
