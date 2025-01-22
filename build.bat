@set ONAME=viott
@set OBJ_PATH=objs\
@set DEFS=-DROM_OUTPUT_FILE=1 

sdasz80 -o -s -p -w %OBJ_PATH%crt.rel crt.s
sdasz80 -o -s -p -w %OBJ_PATH%msx_dos_header.rel msx_dos_header.s
sdasz80 -o -s -p -w %OBJ_PATH%vdptestasm.rel vdptestasm.s
sdasz80 -o -s -p -w %OBJ_PATH%runhere.rel runhere.s
sdcc -c -mz80 --opt-code-speed vdptest.c -o %OBJ_PATH%vdptest.rel

sdcc --code-loc 0x0100 --data-loc 0 -mz80 --no-std-crt0 --opt-code-speed %OBJ_PATH%crt.rel %OBJ_PATH%msx_dos_header.rel %OBJ_PATH%vdptestasm.rel %OBJ_PATH%vdptest.rel %OBJ_PATH%runhere.rel -o %OBJ_PATH%%ONAME%.ihx

@REM -s argument is default is 2*16384 (amend when you get 'error: size of the buffer is too small')
@REM makebin -s 65535 -p -o 0x100 %OBJ_PATH%%ONAME%.ihx dska\%ONAME%.com
makebin -p -o 0x100 %OBJ_PATH%%ONAME%.ihx dska\%ONAME%.com

MSXhex %OBJ_PATH%%ONAME%.ihx -s 0x0100 -b 0x4000 -o dska\%ONAME%.com
