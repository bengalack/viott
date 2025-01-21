@set NAME=viott
@set OBJ_PATH=objs\

@REM --code-loc value below is found from the dos_crt.sym file - add the value of _HEADER0 to .org (0x100)
@REM --data-loc is set to 0 to make it follow code, otherwise it lands at 0x8000 it seems
sdasz80 -o -s -p -w %OBJ_PATH%crt.rel crt.s
sdasz80 -o -s -p -w %OBJ_PATH%msx_dos_header.rel msx_dos_header.s
sdasz80 -o -s -p -w %OBJ_PATH%vdptestasm.rel vdptestasm.s
sdcc --code-loc 0x010E --data-loc 0 -mz80 --no-std-crt0 --opt-code-speed %OBJ_PATH%crt.rel %OBJ_PATH%msx_dos_header.rel %OBJ_PATH%vdptestasm.rel %NAME%.c -o %OBJ_PATH%%NAME%.ihx

@REM -s argument is default is 2*16384 (amend when you get 'error: size of the buffer is too small')
@REM makebin -s 65535 -p -o 0x100 %OBJ_PATH%%NAME%.ihx dska\%NAME%.com
makebin -p -o 0x100 %OBJ_PATH%%NAME%.ihx dska\%NAME%.com