@set ONAME=viott
@set OBJ_PATH=objs\rom\
@set DEFS=-DROM_OUTPUT_FILE=1 
@set SRC=src\

sdasz80 -o -s -p -w -Isrc %OBJ_PATH%crt.rel %SRC%crt.s
sdasz80 -o -s -p -w -Isrc %OBJ_PATH%msx_rom_header.rel %SRC%msx_rom_header.s
sdasz80 -o -s -p -g -w -Isrc %OBJ_PATH%vdptestasm.rel %SRC%vdptestasm.s
sdasz80 -o -s -p -g -w -Isrc %OBJ_PATH%rom_tests.rel %SRC%rom_tests.s
sdasz80 -o -s -p -w -Isrc %OBJ_PATH%slots.rel %SRC%slots.s
sdasz80 -o -s -p -w -Isrc %OBJ_PATH%vdptest_ramcode.rel %SRC%vdptest_ramcode_rom.s
sdcc -c -mz80 -Wa-Isrc -Isrc --opt-code-speed %DEFS% %SRC%vdptest.c -o %OBJ_PATH%vdptest.rel

sdcc -d -mz80 --no-std-crt0 --opt-code-speed --code-loc 0x4000 --data-loc 0xC100 -Wl-b_UPPER=0x0001C000 -Wl-b_SEG1=0x00028000 -Wl-b_SEG2=0x00038000 -Wl-b_SEG3=0x00048000 -Wl-b_SEG4=0x00058000 -Wl-b_SEG5=0x00068000 -Wl-b_SEG6=0x00078000 -Wl-b_SEG7=0x00088000 -Wl-b_SEG8=0x00098000 -Wl-b_SEG9=0x000A8000 -Wl-b_SEGA=0x000B8000 -Wl-b_SEGB=0x000C8000 %OBJ_PATH%crt.rel %OBJ_PATH%msx_rom_header.rel %OBJ_PATH%slots.rel %OBJ_PATH%vdptestasm.rel %OBJ_PATH%vdptest.rel %OBJ_PATH%vdptest_ramcode.rel %OBJ_PATH%rom_tests.rel -o %OBJ_PATH%%ONAME%.ihx

@REM Building ROM file is dependent on MSXhex instead of makebin found in SDCC
@REM https://aoineko.org/msxgl/index.php?title=MSXhex
MSXhex %OBJ_PATH%%ONAME%.ihx -l 212992 -s 0x4000 -b 0x4000 -o rom\%ONAME%.rom