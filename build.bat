@set ONAME=viott
@set OBJ_PATH=objs\
@set DEFS=-DROM_OUTPUT_FILE=1 
@set SRC=src\

sdasz80 -o -s -p -w -Isrc %OBJ_PATH%crt.rel %SRC%crt.s
sdasz80 -o -s -p -w -Isrc %OBJ_PATH%msx_dos_header.rel %SRC%msx_dos_header.s
sdasz80 -o -s -p -w -g -Isrc %OBJ_PATH%vdptestasm.rel %SRC%vdptestasm.s
sdasz80 -o -s -p -w -Isrc %OBJ_PATH%runhere.rel %SRC%runhere.s
sdasz80 -o -s -p -w -Isrc %OBJ_PATH%vdptest_ramcode.rel %SRC%vdptest_ramcode_dos.s
sdcc -c -mz80 -Wa-Isrc -Isrc --opt-code-speed %SRC%vdptest.c -o %OBJ_PATH%vdptest.rel

sdcc --code-loc 0x0100 --data-loc 0 -mz80 --no-std-crt0 --opt-code-speed %OBJ_PATH%crt.rel %OBJ_PATH%msx_dos_header.rel %OBJ_PATH%vdptestasm.rel %OBJ_PATH%vdptest_ramcode.rel %OBJ_PATH%vdptest.rel %OBJ_PATH%runhere.rel -o %OBJ_PATH%%ONAME%.ihx

MSXhex %OBJ_PATH%%ONAME%.ihx -s 0x0100 -b 0x4000 -o dska\%ONAME%.com
