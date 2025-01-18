@set NAME=viott

@REM --code-loc value below is found from the dos_crt.sym file - add the value of _HEADER0 to .org (0x100)
@REM --data-loc is set to 0 to make it follow code, otherwise it lands at 0x8000 it seems
sdasz80 -o -s -p -w objs\dos_crt.rel dos_crt.s
sdasz80 -o -s -p -w objs\vdptestasm.rel vdptestasm.s
sdcc --code-loc 0x010E --data-loc 0 -mz80 --no-std-crt0 --opt-code-speed objs\dos_crt.rel objs\vdptestasm.rel %NAME%.c -o objs\%NAME%.ihx

@REM -s argument is n*16384, where default is n=2 (amend when you get 'error: size of the buffer is too small')
@REM makebin -s 65535 -p -o 0x100 objs\%NAME%.ihx dska\%NAME%.com
makebin -p -o 0x100 objs\%NAME%.ihx dska\%NAME%.com