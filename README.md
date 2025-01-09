# VIOTT - VDP I/O Timing Tester
Test **MSX2 (and above)** VDPs for added wait cycles - which are reported on various systems/engines. Written in C with help from assembly.

**Idea:** Figure out if I/O commands towards the VDP take longer time than expected (current test tests `out`, but `in` variants are easy to add).

**The concept on how to achieve this**, is a simple concept: As we know the amount of cycles per frame on a standard MSX2, we measure how many I/O commands we were able to execute during that time.

I enable a custom, lightweight ISR, add tons of unrolled I/O-commands, and read the value of the PC-register when the frame is finished.

    I/O command cycle cost = ( frame total time - ISR time) /  number of instructions executed

**Assumptions:**
* The available cycles per frame is constant across MSX2 (and up) models when running in z80-mode. I [have no guarantee](https://www.msx.org/forum/msx-talk/general-discussion/msx-models-deviating-from-standard-358mhz) this being constant, but it is part of this model. Small deviations here will result in small "rounding-errors" in the final report.
* The cost of kicking off an interrupt: **14 cycles**.

We measure multiple sets of these, to see if there are any deviations (as [the interrupt seems to be a bit inaccurate at times](https://www.msx.org/forum/msx-talk/hardware/msx-engine-t9769b-does-it-really-add-2-wait-cycles#comment-470398)), and we then use the average for further calculations.

    const float g_afFrmTotalCycles[] = {59736.0, 71364.0};  // NTSC, PAL

    NUM_ITERATIONS = 128;                                   // default (which seem way over the top -  needed at all?)

The tests are run in both 50Hz and 60Hz (screen will blink during change).

### Output examples ###

<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/a1-st.jpeg" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/ax-370.jpeg" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/hb-f1xd.jpeg" />
<img src="https://raw.githubusercontent.com/bengalack/viott/refs/heads/main/img/svi-738.jpeg" />

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
