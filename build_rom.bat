@set ONAME=viott
@set OBJ_PATH=objs\rom\
@set DEFS=-DROM_OUTPUT_FILE=1 

sdasz80 -o -s -p -w %OBJ_PATH%crt.rel crt.s
sdasz80 -o -s -p -w %OBJ_PATH%msx_rom_header.rel msx_rom_header.s
sdasz80 -o -s -p -w %OBJ_PATH%vdptestasm.rel vdptestasm.s
sdasz80 -o -s -p -w %OBJ_PATH%rom_tests.rel rom_tests.s
sdasz80 -o -s -p -w %OBJ_PATH%slots.rel slots.s
sdasz80 -o -s -p -w %OBJ_PATH%vdptest_ramcode.rel vdptest_ramcode_rom.s
sdcc -c -mz80 --opt-code-speed %DEFS% vdptest.c -o %OBJ_PATH%vdptest.rel



sdcc -d -mz80 --no-std-crt0 --opt-code-speed --code-loc 0x4000 --data-loc 0xC100 -Wl-b_UPPER=0x0001C000 -Wl-b_SEG1=0x00028000 -Wl-b_SEG2=0x00038000 -Wl-b_SEG3=0x00048000 -Wl-b_SEG4=0x00058000 -Wl-b_SEG5=0x00068000 -Wl-b_SEG6=0x00078000 -Wl-b_SEG7=0x00088000 -Wl-b_SEG8=0x00098000 -Wl-b_SEG9=0x000A8000 %OBJ_PATH%crt.rel %OBJ_PATH%msx_rom_header.rel %OBJ_PATH%slots.rel %OBJ_PATH%vdptestasm.rel %OBJ_PATH%vdptest.rel %OBJ_PATH%vdptest_ramcode.rel %OBJ_PATH%rom_tests.rel -o %OBJ_PATH%%ONAME%.ihx



@REM Building ROM file is dependent on MSXhex instead of makebin found in SDCC
@REM https://aoineko.org/msxgl/index.php?title=MSXhex
@REM MSXhex %OBJ_PATH%%ONAME%.ihx -l 163840 -s 0x4000 -b 0x4000 -o rom\%ONAME%.rom
MSXhex %OBJ_PATH%%ONAME%.ihx -s 0x4000 -b 0x4000 -o rom\%ONAME%.rom





@REM @REM //TESTING BELOW!!!!!

@REM @set ONAME=viott
@REM @set OBJ_PATH=objs\rom\
@REM @set DEFS=-DROM_OUTPUT_FILE=1 

@REM sdasz80 -o -s -p -w %OBJ_PATH%crt.rel crt.s
@REM sdasz80 -o -s -p -w %OBJ_PATH%msx_rom_header.rel msx_rom_header.s
@REM sdasz80 -o -s -p -w %OBJ_PATH%vdptestasm.rel vdptestasm.s
@REM sdasz80 -o -s -p -w %OBJ_PATH%rom_tests.rel rom_tests.s
@REM sdasz80 -o -s -p -w %OBJ_PATH%slots.rel slots.s
@REM sdasz80 -o -s -p -w %OBJ_PATH%vdptest_ramcode.rel vdptest_ramcode.s
@REM sdcc -c -mz80 --opt-code-speed %DEFS% vdptest.c -o %OBJ_PATH%vdptest.rel


@REM @REM sdcc -d -mz80 --no-std-crt0 --opt-code-speed --code-loc 0x4000 --data-loc 0xD000 -Wl-b_UPPER=0x00007000  -Wl-b_SEG1=0x00018000 %OBJ_PATH%crt.rel %OBJ_PATH%msx_rom_header.rel %OBJ_PATH%slots.rel %OBJ_PATH%vdptestasm.rel %OBJ_PATH%vdptest.rel %OBJ_PATH%rom_tests.rel -o %OBJ_PATH%%ONAME%.ihx

@REM sdcc -d -mz80 --no-std-crt0 --opt-code-speed --code-loc 0x4000 --data-loc 0xD000 -Wl-b_UPPER=0x0002C000 -Wl-b_SEG1=0x00038000 %OBJ_PATH%crt.rel %OBJ_PATH%msx_rom_header.rel %OBJ_PATH%slots.rel %OBJ_PATH%vdptestasm.rel %OBJ_PATH%vdptest.rel %OBJ_PATH%vdptest_ramcode.rel %OBJ_PATH%rom_tests.rel -o %OBJ_PATH%%ONAME%.ihx

@REM @REM Building ROM file is dependent on MSXhex instead of makebin found in SDCC
@REM @REM https://aoineko.org/msxgl/index.php?title=MSXhex
@REM @REM MSXhex %OBJ_PATH%%ONAME%.ihx -s 0x4000 -b 0x4000 -o rom\%ONAME%.rom --segaddr 0 0 --segaddr 1 0x3000 --segaddr 2 0x4000


@REM MSXhex %OBJ_PATH%%ONAME%.ihx -s 0x4000 -b 0x4000 -o rom\%ONAME%.rom
