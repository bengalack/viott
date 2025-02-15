# VIOTT - VDP I/O Timing Tester
Test **MSX2 (and above)** VDPs for added wait cycles - which are reported on various systems/engines. Written in C with help from assembly.

__Concept 1:__ We run massive amounts of unrolled OUTs in "DI mode". So many that, in case of a +1 cycle on an OUT, will result is a full second difference (+2 cycles => 2 seconds difference). We use the RTC to measure this.

I enable a custom, lightweight ISR, add tons of unrolled I/O-commands, and read the value of the PC-register when the frame is finished.

    I/O command cycle cost = available frame total time / number of I/O instructions executed

__Format/Medium:__ In v1.3 I added support for ROM as well (ASCII16). This made the code a bit more complex, sadly. From now on, each test needs to partly be defined two places - one for RAM setup at runtime and one for ROM segments, prepared up front. ROM version was added to find if there are differences when code reside in ROM vs RAM.

__Assumptions:__
* The cost of kicking off an interrupt: __14 cycles__ ([source](http://www.z80.info/interrup.htm)). Not used for the cycle cost calculations, but used for the simple "available cycles per frame" calculation and comparison. 
* Running the code from internal memory is running at optimal speed with no delays, hence, ALL code that is not the unrolled instructions, are put in RAM, this also includes any test setup code and the ISR both in DOS and in ROM mode.

We measure multiple sets of the tests to see if there are any deviations (as [the interrupt seems to be a bit inaccurate at times](https://www.msx.org/forum/msx-talk/hardware/msx-engine-t9769b-does-it-really-add-2-wait-cycles#comment-470398)), and we then use the average for further calculations. Default is 16 iterations in a set.

The tests are run in both 50Hz and 60Hz (screen will blink during change). Colors will also change when we do testing towards the vdp palette port. After a switch of frequency we do a halt. Tests has shown that not doing this, skews the data measured directly after the frequency change.

__Concept 2:__ We run massive amounts of unrolled OUTs in "DI mode". So many that, in case of a +1 cycle on an OUT, the test result will be one full second difference (+2 cycles => 2 seconds difference). We use the [RTC](https://www.msx.org/wiki/Real_Time_Clock_Programming) to measure this. This test is only done is 60Hz atm. The result from this test is found as last number in the summary like `VDP I/O:+1,+1`. It should confirm the first number, measured in concept 1.

### Output examples ###
__Panasonic A1-ST turboR:__
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/v1_31_a1-st.JPEG" />

__Panasonic FS-A1 MSX2:__
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/v1_31_fs-a1.JPEG" />

### Briefly on the tests ###
...as in [tests_as_macros.inc](tests_as_macros.inc)

1. out98: unrolled `out (0x98), a`
2. in98: set vdp up for reading an address + unrolled `in a, (0x98)`
3. in98x: (wrongly) set vdp up for writing to an address + unrolled `in a, (0x98)`
4. in99: set vdp regs up for reading + unrolled `in a, (0x99)`
5. out9A: massive palette color changes by unrolled `out (0x9A), a`
6. out9B: set vdp in stream mode to constantly update reg #32, then unrolled `out (0x9B), a`
7. outi98FMT: set HL to ROM or RAM according to media and C to 0x98, then unrolled `outi`
8. outi98RAM: set HL to RAM and C to 0x98, then unrolled `outi` forced to run from internal RAM
9. !in06FMT: run unrolled `in a, (0x06)`. 0x06 is a random port seemingly not in use
10. !in06RAM: run unrolled `in a, (0x06)`, forced to run from internal RAM
11. !incaFMT: run unrolled `inc a` (this test is to check the basic performance)
12. !incaRAM: run unrolled `inc a`, forced to run from internal RAM
13. !cpiFMT: run unrolled `cpi` (this test test the use of registers referencing values pointed to in memory)
14. !cpiRAM: run unrolled `cpi`, forced to run from internal RAM

__Naming of the tests:__
* 'FMT' means format (or media), so the test follows the format. DOS: RAM. ROM: ROM.
* 'RAM' means that the test is forced to run in RAM even if the program comes on a cartridge.
* The ! means that the test is not a VDP test, for comparisons (and ROM performance testing)

### Background ###
* It all stems from the waitcycles reported on MSX-Engine T9769A/B/C: https://www.msx.org/wiki/Toshiba_T9769 

### Dependencies & Build ##
* Your MSX needs a working RTC (battery not needed, it just need to tick), if not the application will hang forever
* Tried to make this independent of various libraries. This to make everything as light and transparent as possible.
* Wanted to use as many BIOS-calls as possible, to reduce code and stay as easy to read for others.
* [SDCC](https://sdcc.sourceforge.net/) v4.2 or later, is needed though, to enable C.
* [MSXHex](https://aoineko.org/msxgl/index.php?title=MSXhex), the best ihx-to-binary tool for MSX
* Batch files are made for *Windows*, but should be easy to mod for other platforms. 
* If you use an emulator, edit `run.bat` to fit your paths/tools.

### Target platform / environment ###
* The _ROM_-variant (recommended) is a megarom using the ASCII-16 mapper. Find rom-file in `rom/`
* For the _MSXDOS_ variant you must provide DOS yourself. Find com-file in `dska/`.

### Weaknesses ###
* There are probably real world circumstances that are not covered in the tests (give me a hint, and I'll see if we can add better tests).

### Download executable ###
You can download the latest pre-built `.com` file here: [/dska/viott.com](https://github.com/bengalack/viott/raw/refs/heads/main/dska/viott.com) and `.rom` here: [/rom/viott.rom](https://github.com/bengalack/viott/raw/refs/heads/main/rom/viott.rom)
