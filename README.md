# VIOTT - VDP I/O Timing Tester
Test **MSX2 (and above)** VDPs for added wait cycles - which are reported on various systems/engines. Written in C with help from assembly.

**Idea:** Figure out if I/O commands towards the VDP take longer time than expected.

**The concept on how to achieve this**, is a simple concept: We measure the amount of cycles we can spend during a frame, by using normal, non-I/O instructions. Then we run frames with I/O instructions only, and by measuring how many instructions we were able to execute during that time, we find the cost of each I/O instruction.

I enable a custom, lightweight ISR, add tons of unrolled I/O-commands, and read the value of the PC-register when the frame is finished.

    I/O command cycle cost = available frame total time / number of I/O instructions executed

**Assumptions:**
* The cost of kicking off an interrupt: **14 cycles**. Not used for the cycle cost calculations, but used for the simple "available cycles per frame" calculation and comparison.

We measure multiple sets of the tests to see if there are any deviations (as [the interrupt seems to be a bit inaccurate at times](https://www.msx.org/forum/msx-talk/hardware/msx-engine-t9769b-does-it-really-add-2-wait-cycles#comment-470398)), and we then use the average for further calculations.

The tests are run in both 50Hz and 60Hz (screen will blink during change). Colors will also change when we do testing towards the vdp palette port.

One test is added at the end, to test the I/O command towards non-VDP ports for validation.

### Output examples ###

<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/AX-370_2.JPEG" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/NMS-8245.JPEG" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/HB-F1XD_2.JPEG" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/HB-F1XDJ.JPEG" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/A1-ST_2.JPEG" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/SVI-738_2.JPEG" />

### Background ###
* It all stems from the waitcycles reported on MSX-Engine T9769A/B/C: https://www.msx.org/wiki/Toshiba_T9769 

### Dependencies & Build ##
* Tried to make this independent of various libraries. This to make everything as light and transparent as possible.
* Wanted to use as many BIOS-calls as possible, to reduce code and stay as easy to read for others.
* [SDCC](https://sdcc.sourceforge.net/) v4.2 or later, is needed though, to enable C.
* Batch files are made for *Windows*, but should be easy to mod for other platforms. 
* (Optional) uncomment a line and `python` generates `.sym`-files which *openmsx* understands.
* If you use an emulator, edit `run.bat` to fit your paths/tools.

### Target platform / environment ###
* You need *MSXDOS*. Put into `dska/`. - We've seen that *MSXDOS* has been involved when an additional delay have been reported. Therefore this tool has been made in *MSXDOS*. It is furthermore convenient that *MSXDOS* simplifies slot handling. It improves compatibility, it starts with 64 kB RAM, in IM 1 and thus has 0x0038 easily available from the outset, reducing complexity.

### Weaknesses ###
* Unsure if there are circumstances that are not covered in the test that may affect the timings.

### Download executable ###
You can download the latest pre-built `.com` file here: [/dska/viott.com](https://github.com/bengalack/viott/raw/refs/heads/main/dska/viott.com)
