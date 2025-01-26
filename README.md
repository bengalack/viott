# VIOTT - VDP I/O Timing Tester
Test **MSX2 (and above)** VDPs for added wait cycles - which are reported on various systems/engines. Written in C with help from assembly.

**Idea:** Figure out if I/O commands towards the VDP take longer time than expected.

**The concept on how to achieve this**, is a simple concept: We measure the amount of cycles we can spend during a frame, by using normal, "safe" non-I/O instructions. Then we run frames with I/O instructions, and by measuring how many instructions we were able to execute during that time, we find the cost of each I/O instruction.

I enable a custom, lightweight ISR, add tons of unrolled I/O-commands, and read the value of the PC-register when the frame is finished.

    I/O command cycle cost = available frame total time / number of I/O instructions executed

**Format/Medium:** In v1.3 I added support for ROM as well (ASCII16). This made the code a bit more complex, sorry about that. From now on, each test needs to partly be defined two places - one for RAM setup at runtime and one for ROM segments, prepared up front. ROM version was added to find if there are differences when code reside in ROM vs RAM.

**Assumptions:**
* The cost of kicking off an interrupt: **14 cycles**. Not used for the cycle cost calculations, but used for the simple "available cycles per frame" calculation and comparison.
* Running the code from internal memory is running at optimal speed with no delays, hence, ALL code that is not the unrolled instructions, are put in RAM, this also includes any test setup code and the ISR both in DOS and in ROM mode.

We measure multiple sets of the tests to see if there are any deviations (as [the interrupt seems to be a bit inaccurate at times](https://www.msx.org/forum/msx-talk/hardware/msx-engine-t9769b-does-it-really-add-2-wait-cycles#comment-470398)), and we then use the average for further calculations. Default is 16 iterations in a set.

The tests are run in both 50Hz and 60Hz (screen will blink during change). Colors will also change when we do testing towards the vdp palette port. After a switch of frequency we do a halt. Tests has shown that not doing this, skews the data measured directly after the frequency change.

### Output examples ###

<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/AX-370_2.JPEG" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/NMS-8245.JPEG" />

### Briefly on the tests ###


1. outdi98
2. out98
3. in98
4. in98x
5. in99
6. out9A
7. out9B
8. outi98FMT
9. outi98RAM
10. !in06FMT
11. !in06RAM




### Background ###
* It all stems from the waitcycles reported on MSX-Engine T9769A/B/C: https://www.msx.org/wiki/Toshiba_T9769 

### Dependencies & Build ##
* Tried to make this independent of various libraries. This to make everything as light and transparent as possible.
* Wanted to use as many BIOS-calls as possible, to reduce code and stay as easy to read for others.
* [SDCC](https://sdcc.sourceforge.net/) v4.2 or later, is needed though, to enable C.
* [MSXHex](https://aoineko.org/msxgl/index.php?title=MSXhex), the best ihx-to-binary tool for MSX
* Batch files are made for *Windows*, but should be easy to mod for other platforms. 
* If you use an emulator, edit `run.bat` to fit your paths/tools.

### Target platform / environment ###
* For the _MSXDOS_ variant you must provide DOS yourself. Find com-file in `dska/`.
* The _ROM_-variant is a megarom using the ASCII-16 mapper. Find rom-file in `rom/`

### Weaknesses ###
* There are probably real world circumstances that are not covered in the tests (give me a hint, and I'll see if we can add better tests).

### Download executable ###
You can download the latest pre-built `.com` file here: [/dska/viott.com](https://github.com/bengalack/viott/raw/refs/heads/main/dska/viott.com) and `.rom` here: [/rom/viott.rom](https://github.com/bengalack/viott/raw/refs/heads/main/rom/viott.rom)
