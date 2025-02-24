// ---------------------------------------------------------------------------
// This program tries to figure out if I/O commands towards the VDP take
// longer time than expected on various systems/engines. It also measures
// if code run in (mega)rom segments are slower than normal. Plain instuction
// cycle cost can also measured and makes sense for turbo and usage of
// peripherals
//
// The tool comes in two variants: 1. DOS 2: ROM (Megarom)
// 
// It is using a custom ISR which does not do much other than storing the
// current address of the program pointer, at address: g_pPCReg
//
// In dos mode, tests are added by a new TestDescriptor entry, with code in
// macro-blocks ('tests_as_macros.inc'), which are used in TEST_n_STARTUP/EMPTY
// and TEST_n_UNROLL for DOS version, and in segments in 'rom_tests.s' for
// ROM-version. The ROM-buildscript must also include info about any added segment.
//
// The two first tests are used to determine the amount of cycles available
// in a frame. These tests must be run from internal RAM.
//
// Assumptions:
//  * There are no line interrupts enabled when we start (DOS)
//  * CPU kicking off the interrupt is 13+1 (+1 due to the M1 +1 cycle)
//    (13 is from this: http://www.z80.info/interrup.htm)
//  * Running programs from internal memory is the best baseline for
//    performance (no delays are added)
//
// Notes:
//  * There is no support for global initialisation of RAM variables in this config
//  * Likely some C programming shortcomings - I don't have much experience with C
//  * Sorry about the various macros for tests and #ifdefs, but adding support for
//    ROM in retrospect complicates things, and some things got ugly
//  * SORRY! for the Hungarian notation, but is helps me when MIXING asm and c
//  * Others:
//      Prefixes:
//      * s  = signed char    (s8)
//      * u  = unsigned char  (u8)
//      * i  = signed short   (s16)
//      * n  = unsigned short (u16)
//      * l  = unsigned long  (u32)
//      * d  = signed long    (s32)
//      * f  = float
//      * p  = pointer
//      * o  = object (struct)
//      * a  = array (single or multi-dim)
//      * b  = bool
//      * sz = zero terminated string (C/SDCC adds the zero automatically)
//      * g_ = global
//
//      Postfixes:
//      * NI = No Interrupt
//
// VOITT © 2025 by Pål Frogner Hansen is licensed under CC BY 4.0
// ---------------------------------------------------------------------------

#include <stdio.h>      // herein be sprintf 
#include <string.h>     // memcpy
#include <stdbool.h>

// Typedefs & defines --------------------------------------------------------
//
#define DEBUG_FORCE_R800_FULLSPEED_IF AVAILABLE 0 // Testing code for provoking various speeds. R800 mode does not work atm!
#define DEBUG_FORCE_TURBO_IF_AVAILABLE 0
#define DEBUG_INSERT_TURBO_MID_TEST 0

#define NUM_ITERATIONS      4       // Can't see that many are needed
#define TEST_SEG_OFFSET     2	    // Test segments starts here. Only used in ROM code
#define CALIBRATION_TESTS   2	    // Num#. We use these for finding the overall available cycles in a frame
#define SIZE_TAIL_BLOCK     7	    // bytes
#define SIZE_LONGTEST_TAIL  7	    // bytes
#define FRAME_COUNT_ADD_UP  0.333f  // a heuristic/assumption to get closer to the exact value

typedef signed char         s8;
typedef unsigned char       u8;
typedef signed short        s16;
typedef unsigned short      u16;
typedef unsigned long       u32;
typedef signed long         s32;
typedef const void          (function)(void);

#define halt()				{__asm halt __endasm;}
#define enableInterrupt()	{__asm ei __endasm;}
#define disableInterrupt()	{__asm di __endasm;}
#define break()				{__asm in a,(0x2e) __endasm;} // for debugging. may be risky to use as it trashes A
#define arraysize(arr)      (sizeof(arr)/sizeof((arr)[0]))

enum cpu_variant {Z80_PLAIN, Z80_TURBO, R800_ROM, R800_DRAM, NUM_CPU_VARIANTS};
enum three_way {NO, YES, NA};
enum freq_variant {NTSC, PAL, FREQ_COUNT};

typedef struct {
    u8*                     szTestName;                 // max 9 characters
	function*               pFncStartupBlock;
	function*               pFncUnrollInstruction;       // or plural.
	u8                      uUnrollInstructionsSize;
	u8                      uUnrollSingleInstructionSize;
    enum three_way          eReadVRAM;                  // if we should set up VRAM for write, read or nothing
    u8                      uStartupCycleCost;          // init of regs or so, at start of frame, before repeats
    u8                      uRealSingleCost;            // the cost of the unroll instruction(s) if run once
    bool                    bForceRAMRun;
    u8                      uSegNum;                    // used only in ROM mode. 0xFF: not in use
} TestDescriptor;

typedef struct {
    u32 lInt;
    u8  uFrac;
} IntWith2Decimals;

// Declarations (see .s-file) ------------------------------------------------
//
u8   getMSXType(void);
u8   getCPU(void);
void changeCPU(u8 uMode);
u8   changeMode(u8 uModeNum);

void enableTurbo(bool bEnable) __preserves_regs(e,h,l,iyl,iyh);
bool isTurboEnabled(void) __preserves_regs(d,e,h,l,iyl,iyh);
bool hasTurboFeature(void) __preserves_regs(d,e,h,l,iyl,iyh);

void print(u8* szMessage);
bool getPALRefreshRate(void);
void setPALRefreshRate(bool bPAL);
void customISR(void);
void setVRAMAddressNI(u8 uBitCodes, u16 nVRAMAddress);
void initPalette(void);             // in case we mess up the palette during testing
void restorePalette(void);

void runTestAsmInMem(void);

void commonStartForAllTests(void);
void longTest(void);

u8   readClock(u8 uBlock0RegID);

// Consts / ROM friendly -----------------------------------------------------
//
const TestDescriptor    g_aoTest[] = {
                                        {
                                            "sync1",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_0_UNROLL,      // void             pFncUnrollInstruction;
                                            1,                  // u8               uUnrollInstructionsSize;
                                            1,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            5,                  // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun; - first run is ALWAYS in RAM
                                            0xFF                // u8               uSegNum;
                                        },
                                        {
                                            "sync2",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_1_UNROLL,      // void             pFncUnrollInstruction;
                                            1,                  // u8               uUnrollInstructionsSize;
                                            1,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            7,                  // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun; - second run is ALWAYS in RAM
                                            0xFF                // u8               uSegNum;
                                        },
                                        {
                                            "out98",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_2_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+0   // u8               uSegNum;
                                        },
                                        {
                                            "in98",             // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_3_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            YES,                // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+1   // u8               uSegNum;
                                        },

                                        // {
                                        //     "in98x",            // u8*              szTestName;
                                        //     TEST_EMPTY,         // function*        pFncStartupBlock;
                                        //     TEST_4_UNROLL,      // void             pFncUnrollInstruction;
                                        //     2,                  // u8               uUnrollInstructionsSize;
                                        //     2,                  // u8               uUnrollSingleInstructionSize;
                                        //     NO,                 // enum three_way   eReadVRAM;
                                        //     11,                 // u8               uStartupCycleCost;
                                        //     12,                 // u8               uRealSingleCost;
                                        //     false,              // bool             bForceRAMRun;
                                        //     TEST_SEG_OFFSET+2   // u8               uSegNum;
                                        // },

                                            {
                                                "!inc(hl)",         // u8*              szTestName;
                                                TEST_4_2_STARTUP,   // function*        pFncStartupBlock;
                                                TEST_4_2_UNROLL,    // void             pFncUnrollInstruction;
                                                1,                  // u8               uUnrollInstructionsSize;
                                                1,                  // u8               uUnrollSingleInstructionSize;
                                                NA,                 // enum three_way   eReadVRAM;
                                                22,                 // u8               uStartupCycleCost;
                                                12,                 // u8               uRealSingleCost;
                                                false,              // bool             bForceRAMRun;
                                                TEST_SEG_OFFSET+2   // u8               uSegNum;
                                            },

                                        // {
                                        //     "in99",             // u8*              szTestName;
                                        //     TEST_5_STARTUP,     // function*        pFncStartupBlock;
                                        //     TEST_5_UNROLL,      // void             pFncUnrollInstruction;
                                        //     2,                  // u8               uUnrollInstructionsSize;
                                        //     2,                  // u8               uUnrollSingleInstructionSize;
                                        //     NA,                 // enum three_way   eReadVRAM;
                                        //     51,                 // u8               uStartupCycleCost;
                                        //     12,                 // u8               uRealSingleCost;
                                        //     false,              // bool             bForceRAMRun;
                                        //     TEST_SEG_OFFSET+3   // u8               uSegNum;
                                        // },

                                            {
                                                "!adca,iy0",        // u8*              szTestName;
                                                TEST_EMPTY,         // function*        pFncStartupBlock;
                                                TEST_5_2_UNROLL,    // void             pFncUnrollInstruction;
                                                3,                  // u8               uUnrollInstructionsSize;
                                                3,                  // u8               uUnrollSingleInstructionSize;
                                                NA,                 // enum three_way   eReadVRAM;
                                                11,                 // u8               uStartupCycleCost;
                                                21,                 // u8               uRealSingleCost;
                                                false,              // bool             bForceRAMRun;
                                                TEST_SEG_OFFSET+3   // u8               uSegNum;
                                            },


                                        // {   // Palette test, must init first and restore palette after
                                        //     "out9A",            // u8*              szTestName;
                                        //     TEST_EMPTY,         // function*        pFncStartupBlock;
                                        //     TEST_6_UNROLL,      // void             pFncUnrollInstruction;
                                        //     2,                  // u8               uUnrollInstructionsSize;
                                        //     2,                  // u8               uUnrollSingleInstructionSize;
                                        //     NA,                 // enum three_way   eReadVRAM;
                                        //     11,                 // u8               uStartupCycleCost;
                                        //     12,                 // u8               uRealSingleCost;
                                        //     false,              // bool             bForceRAMRun;
                                        //     TEST_SEG_OFFSET+4   // u8               uSegNum;
                                        // },

                                        {
                                            "!bit0,iy0",        // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_6_2_UNROLL,    // void             pFncUnrollInstruction;
                                            4,                  // u8               uUnrollInstructionsSize;
                                            4,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            22,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+4   // u8               uSegNum;
                                        },

                                        // {   // Stream port test
                                        //     "out9B",            // u8*              szTestName;
                                        //     TEST_7_STARTUP,     // function*        pFncStartupBlock;
                                        //     TEST_7_UNROLL,      // void             pFncUnrollInstruction;
                                        //     2,                  // u8               uUnrollInstructionsSize;
                                        //     2,                  // u8               uUnrollSingleInstructionSize;
                                        //     NA,                 // enum three_way   eReadVRAM;
                                        //     51,                 // u8               uStartupCycleCost;
                                        //     12,                 // u8               uRealSingleCost;
                                        //     false,              // bool             bForceRAMRun;
                                        //     TEST_SEG_OFFSET+5   // u8               uSegNum;
                                        // },

                                        {
                                            "!cpn",             // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_7_2_UNROLL,    // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            8,                  // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+5   // u8               uSegNum;
                                        },

                                        {
                                            "outi98",           // u8*              szTestName;
                                            TEST_8_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_8_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            30,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+6   // u8               uSegNum;
                                        },
                                        {   // Using same tests as above
                                            "outi98RAM",        // u8*              szTestName;
                                            TEST_8_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_8_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            30,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+6   // u8               uSegNum;
                                        },
                                        {   // Just pick a non-used port (I hope), and check the speed
                                            "!in06",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_9_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+7   // u8               uSegNum;
                                        },
                                        {   // Just pick a non-used port (I hope), and check the speed
                                            "!in06RAM",         // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_9_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun;
                                            0xFF                // u8               uSegNum;
                                        },
                                        {   
                                            "!inca",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_A_UNROLL,      // void             pFncUnrollInstruction;
                                            1,                  // u8               uUnrollInstructionsSize;
                                            1,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            5,                  // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+8   // u8               uSegNum;
                                        },
                                        {   
                                            "!incaRAM",         // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_A_UNROLL,      // void             pFncUnrollInstruction;
                                            1,                  // u8               uUnrollInstructionsSize;
                                            1,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            5,                  // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun;
                                            0xFF                // u8               uSegNum;
                                        },
                                        {   
                                            "!cpi",             // u8*              szTestName;
                                            TEST_B_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_B_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            32,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+9   // u8               uSegNum;
                                        },
                                        {   
                                            "!cpiRAM",          // u8*              szTestName;
                                            TEST_B_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_B_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            32,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun;
                                            0xFF                // u8               uSegNum;
                                        }
                                     };

const u8* const         g_aszCPUModes[]      = {"z80 @ 3.5MHz","z80 @ 5.7MHz (turbo)", "r800 @ 7.2MHz (comp)", "r800 @ 7.2MHz (DRAM)"};


const u8                g_szErrorMSX[]      = "MSX2 and above is required";
const u8                g_szGreeting[]      = "VDP I/O Timing Test v1.40 - %d repeats, %s, CPU: %s\r\n"; 
const u8                g_szWait[]          = "...please wait 30 seconds or so...";
const u8                g_szRemoveWait[]    = "\r                                  \r";
const u8                g_szReportCols[]    = "               avg   min   max  cost  ~d |      avg   min   max  cost  ~d\r\n";
const u8                g_szReportValues[]  = "%9s %5ld.%02d %5ld %5ld %2ld.%02d %+3d | %5ld.%02d %5ld %5ld %2ld.%02d %+3d\r\n";

const u8                g_szSpeedHdrCols[]  = "          ---------- 60 Hz NTSC ---------|----------- 50 Hz PAL ---------\r\n";
const u8                g_szSplitline[]     = "                                         |\r\n";
const u8                g_szSpeedResult[]   = "Framecycles: %27s | %30s\r\n";
const u8                g_szSRPart[]        = "%lu vs %lu, d:%+ld";

const u8                g_szLongTest[]      = " longtest  %s\r\n";
const u8                g_szLongInfo[]      = "VDP I/O added wait: %+d cycle(s)";
const u8                g_szLongRTCError[]  = "(no result as internal clock is not working)";
const u8                g_szSummary[]       = "[EVALUATE] We have an issue if ~d is greater than 0 on any of the lines\r\n";

const u8                g_szNewline[]       = "\r\n";

const u8* const         g_aszFreq[]         = {"60", "50"}; // must be chars

// Normal. Turbo is supposedly 50% faster.
// PAL (“50 FPS”):  71364 cycles (3579545/50.159), turbo (+50%): 107046 cycles, measured: 106776 (49.62%)
// NTSC (“60 FPS”): 59736 cycles (3579545/59.923), turbo (+50%):  89604 cycles, measured:  89387 (49.64%)
// 
const u32 alFRAME_CYCLES_TARGET[NUM_CPU_VARIANTS][FREQ_COUNT] = {{59736, 71364}, {89387, 106776}, {150000, 200000}, {200000, 300000}}; // assumed "ideal"

const u8                FRAME_CYCLES_INT                    = 171;
const u8                FRAME_CYCLES_INT_TURBO_ADD          = 3 * (32) + 7;
const u8                FRAME_CYCLES_INT_KICK_OFF           = 14 + 11; // +11 is the JP at 0x0038
const u8                FRAME_CYCLES_INT_KICK_OFF_TURBO_ADD = 0 + 2;
const u8                FRAME_CYCLES_COMMON_START           = 72; // cycles after halt
const u8                FRAME_CYCLES_COMMON_START_TURBO_ADD = 8;

const u8                FRAME_CYCLES_TAIL_Z80               = 42;
const u8                FRAME_CYCLES_TAIL_Z80_TURBO_ADD     = 3; // maybe more, how do I know??? 


// RAM variables -------------------------------------------------------------
//
enum cpu_variant        g_eCPUMode;
void* __at(0x0039)      g_pInterrupt;       // We assume that 0x0038 already holds 0xC3 (JP) in dos mode at startup
void*                   g_pInterruptOrg;
u8                      g_auBuffer[120];    // temp/general buffer here to avoid stack explosion

volatile u8*            g_pPCReg;           // pointer to PC-reg when the interrupt was triggered
volatile bool           g_bStorePCReg;
volatile u8             g_uExtraRounds;     // test is "too" fast, one full segment is processed multiple times
function*               g_pFncCurStartupBlock;

                        // RESULTS BELOW. As R800 can have instructions of 1 cycle only, we can get iterations with > u16 in PAL
float                   g_afFrmTotalCycles      [FREQ_COUNT];
float                   g_afFrmTotalCyclesNoTail[FREQ_COUNT];
u32                     g_alFrameInstrResult    [FREQ_COUNT][arraysize(g_aoTest)][NUM_ITERATIONS];
float                   g_afFrameInstrResultAvg [FREQ_COUNT][arraysize(g_aoTest)];
u32                     g_alFrameInstrResultMin [FREQ_COUNT][arraysize(g_aoTest)];
u32                     g_alFrameInstrResultMax [FREQ_COUNT][arraysize(g_aoTest)];
u8                      g_auFrameInstrResultXtra[FREQ_COUNT][arraysize(g_aoTest)][NUM_ITERATIONS];
float                   g_afFrameInstrResultXtra[FREQ_COUNT][arraysize(g_aoTest)];
u8                      g_auFrameInstrResultXtr2[FREQ_COUNT][arraysize(g_aoTest)];
float                   g_afFinalTestCost       [FREQ_COUNT][arraysize(g_aoTest)];
s16                     g_iVDPDiff;

                        // Long test timings via RTC (start:0, end:1)
bool                    g_bRTCWorking;
u32                     g_lStartTimeStamp;
u32                     g_lEndTimeStamp;
u8                      g_uSecondsL0;
u8                      g_uSecondsH0;
u8                      g_uMinsL0;
u8                      g_uMinsH0;

u8                      g_uSecondsL1;
u8                      g_uSecondsH1;
u8                      g_uMinsL1;
u8                      g_uMinsH1;

// --------------------------------------------------------------------------
// Specials in case of ROM outfile
//
#ifdef ROM_OUTPUT_FILE

void                    UPPERCODE_BEGIN(void); // used for getting address only!
void                    UPPERCODE_END(void); // used for getting address only!


void                    establishSlotIDsNI_fromC(void);
void                    memAPI_enaSltPg0_NI_fromC(u8 uSlotID);
void                    memAPI_enaSltPg2_NI_fromC(u8 uSlotID);

u8 __at(0x0038)         g_uInt38;           // In ROM mode there is NO JP at this address initially

u8 __at(0xF3AE)         g_uBIOS_LINL40;     // LINL40, MSX BIOS for width/columns


u8                      g_uSlotidPage0BIOS;
u8                      g_uSlotidPage0RAM;
u8                      g_uSlotidPage2RAM;
u8                      g_uSlotidPage2ROM;
u8                      g_uCurSlotidPage0;

#define UPPER_SEG_ID    1
#define SEG_P2_SW       0x7000	// Segment switch on page 8000h-BFFFh (ASCII 16k Mapper) https://www.msx.org/wiki/MegaROM_Mappers#ASC16_.28ASCII.29
#define ENABLE_SEGMENT_PAGE2(data) (*((u8* volatile)(SEG_P2_SW)) = ((u8)(data)));

const u8                g_szMedium[] = "ROM (MEGAROM)";
#else
const u8                g_szMedium[] = "DOS";
#endif

// Code ----------------------------------------------------------------------
// Test code: All bodies must be pure asm via __naked.
// See the "tests_as_macros.inc"-file
//
void TEST_EMPTY(void) __naked {
__asm .include "tests_as_macros.inc" ; // hacky! also used by the tests below
    macroTEST_EMPTY
__endasm;  
}
void TEST_TAIL(void) __naked {
__asm macroTEST_TAIL __endasm;
}
void TEST_0_UNROLL(void) __naked {
__asm macroTEST_0_UNROLL __endasm;
}
void TEST_1_UNROLL(void) __naked {
__asm macroTEST_1_UNROLL __endasm;
}
void TEST_2_UNROLL(void) __naked {
__asm macroTEST_2_UNROLL __endasm;
}
void TEST_3_UNROLL(void) __naked {
__asm macroTEST_3_UNROLL __endasm;
}
void TEST_4_UNROLL(void) __naked {
__asm macroTEST_4_UNROLL __endasm;
}
void TEST_4_2_STARTUP(void) __naked {
__asm macroTEST_4_2_STARTUP __endasm;
}
void TEST_4_2_UNROLL(void) __naked {
__asm macroTEST_4_2_UNROLL __endasm;
}
void TEST_5_STARTUP(void) __naked {
__asm macroTEST_5_STARTUP __endasm;
}
void TEST_5_UNROLL(void) __naked {
__asm macroTEST_5_UNROLL __endasm;
}
void TEST_5_2_UNROLL(void) __naked {
__asm macroTEST_5_2_UNROLL __endasm;
}
void TEST_6_UNROLL(void) __naked {
__asm macroTEST_6_UNROLL __endasm;
}
void TEST_6_2_UNROLL(void) __naked {
__asm macroTEST_6_2_UNROLL __endasm;
}
void TEST_7_STARTUP(void) __naked {
__asm macroTEST_7_STARTUP __endasm;
}
void TEST_7_UNROLL(void) __naked {
__asm macroTEST_7_UNROLL __endasm;
}
void TEST_7_2_UNROLL(void) __naked {
__asm macroTEST_7_2_UNROLL __endasm;
}
void TEST_8_STARTUP(void) __naked {
__asm macroTEST_8_STARTUP __endasm;
}
void TEST_8_UNROLL(void) __naked {
__asm macroTEST_8_UNROLL __endasm;
}
void TEST_9_UNROLL(void) __naked {
__asm macroTEST_9_UNROLL __endasm;
}
void TEST_A_UNROLL(void) __naked {
__asm macroTEST_A_UNROLL __endasm;
}
void TEST_B_STARTUP(void) __naked {
__asm macroTEST_B_STARTUP __endasm;
}
void TEST_B_UNROLL(void) __naked {
__asm macroTEST_B_UNROLL __endasm;
}
void TEST_LONGTEST_UNROLL(void) __naked {
__asm macroTEST_LONG_UNROLL __endasm;
}
void TEST_LONGTEST_TAIL(void) __naked {
__asm macroTEST_LONG_TAIL __endasm;
}

// ---------------------------------------------------------------------------
//
u32 getTimeStamp(void)
{
    u32 lSL = (u32)readClock(0);
    u32 lSH = (u32)readClock(1);
    u32 lML = (u32)readClock(2);
    u32 lMH = (u32)readClock(3);

    return (lMH<<24)|(lML<<16)|(lSH<<8)|lSL;
}

// ---------------------------------------------------------------------------
// Special rounding. Caters for the 3rd decimal already presented to user
// rounded up (using +0.005). Just to avoid making it look like a bug.
//
s8 signedRoundX(float f)
{
    return f<0?(s8)(f-0.5):(s8)(f+0.505);
}

// ---------------------------------------------------------------------------
//
float unsignedRound(float f)
{
    return (float)((u32)(f+0.5));
}

// ---------------------------------------------------------------------------
//
u32 abs32(s32 s)
{
    return s<0?-s:s;
}

// ---------------------------------------------------------------------------
//
u8 abs(s8 s)
{
    return s<0?-s:s;
}

// ---------------------------------------------------------------------------
//
float fmax(float f1, float f2)
{
    return f1>f2?f1:f2;
}

        
// ---------------------------------------------------------------------------
void setCustomISR(void)
{
    disableInterrupt();

#ifdef ROM_OUTPUT_FILE
    memAPI_enaSltPg0_NI_fromC(g_uSlotidPage0RAM);
#endif

    g_bStorePCReg   = false;        // control if storing & stack mods should take place. MUST be reset
    g_pInterruptOrg = g_pInterrupt;
    g_pInterrupt    = &customISR;
    enableInterrupt();
}

// ---------------------------------------------------------------------------
void restoreOriginalISR(void)
{
    disableInterrupt();
    g_pInterrupt = g_pInterruptOrg;

#ifdef ROM_OUTPUT_FILE
    memAPI_enaSltPg0_NI_fromC(g_uSlotidPage0BIOS);
#endif

    enableInterrupt();
}

// ---------------------------------------------------------------------------
// If line is greater than 80 chars, cut at 80 (to avoid 80 char strings with
// "\r\n" at the end (after pos 80), inserting an unwanted line). Does only work on RAM strings ofc
void printX(u8* sz)
{
    u8 l = strlen(sz);
    if((l >= 80) && (*(sz + 79) >= ' ') )
        *(sz + 80) = 0;

    print(sz);
}

// ---------------------------------------------------------------------------
void enableR800FullSpeedIfAvailable(bool bEnable)
{
    if(getMSXType() == 3) // MSX turbo R
        changeCPU(bEnable ? 2 : 0); // 0=Z80 (ROM) mode, 1=R800 ROM  mode, 2=R800 DRAM mode
}

// ---------------------------------------------------------------------------
void enableTurboIfAvailable(bool bEnable)
{
    if(hasTurboFeature())
        enableTurbo(bEnable);
}

// ---------------------------------------------------------------------------
// Just set write address to upper 64kB area. We will not see this garbage
// on screen while in DOS prompt/screen
void prepareVDP(enum three_way eRead)
{
    disableInterrupt();

    if(eRead == NO) // == Write
        setVRAMAddressNI(1 | 0x40, 0x0000);
    else if(eRead == YES) // ignore if NA
        setVRAMAddressNI(1 | 0x00, 0x0000);

    enableInterrupt();
}

// ---------------------------------------------------------------------------
// For DOS mode: Copy test in at runTestAsmInMem, X amount of unrolleds
// Some tests are forced to run in RAM (i.e. dos mode). Like the first run.
// As this seem to be the fastest, most accurate and reliable way to measure
// the speed.
//
void setupTestInMemoryDOS(u8 uTest)
{
    u16 nMax = (u16)((u32)(0x4000 - SIZE_TAIL_BLOCK) / g_aoTest[uTest].uUnrollInstructionsSize);
    u8* p = (u8*) &runTestAsmInMem;

    for(u16 n = 0; n < nMax; n++)
    {
        memcpy(p, *g_aoTest[uTest].pFncUnrollInstruction, g_aoTest[uTest].uUnrollInstructionsSize);
        p += g_aoTest[uTest].uUnrollInstructionsSize;
    }

    memcpy(p, &TEST_TAIL, SIZE_TAIL_BLOCK);
}

// ---------------------------------------------------------------------------
// For ROM mode: Set correct segment (tests already present in seg) in page 2
// (runTestAsmInMem is within the segment). If the test is the "baseline"-test
// (the first test), we run this using RAM in page 2, as RAM is most precise.
// (external (flash-)ROMs may introduce latency)
//
#ifdef ROM_OUTPUT_FILE
void setupTestInMemoryROM(u8 uTest)
{
    if(g_aoTest[uTest].bForceRAMRun)
    {
        disableInterrupt();
        memAPI_enaSltPg2_NI_fromC(g_uSlotidPage2RAM);
        enableInterrupt();
        setupTestInMemoryDOS(uTest);
    }
    else
    {
        disableInterrupt();
        memAPI_enaSltPg2_NI_fromC(g_uSlotidPage2ROM);
        enableInterrupt();
        ENABLE_SEGMENT_PAGE2(g_aoTest[uTest].uSegNum);
    }
}
#endif

// ---------------------------------------------------------------------------
//
void setupTestInMemory(u8 uTest)
{
    g_pFncCurStartupBlock = g_aoTest[uTest].pFncStartupBlock;

#ifdef ROM_OUTPUT_FILE
    setupTestInMemoryROM(uTest);
#else
    setupTestInMemoryDOS(uTest);
#endif
}

// ---------------------------------------------------------------------------
void runIteration(enum freq_variant eFreq, u8 uTest, u8 uIterationNum)
{
    prepareVDP(g_aoTest[uTest].eReadVRAM);

    commonStartForAllTests();

    u8 uUnrollSingleInstrSize = g_aoTest[uTest].uUnrollSingleInstructionSize;
    u8 uUnrollInstrSize = g_aoTest[uTest].uUnrollInstructionsSize;

    u16 nLength = (u16)g_pPCReg - (u16)&runTestAsmInMem;
    u32 lInstructions = nLength / uUnrollSingleInstrSize;

    g_auFrameInstrResultXtra[eFreq][uTest][uIterationNum] = g_uExtraRounds;

    if(g_uExtraRounds != 0)
    {
        // u32 lTestBlocksInSegment = ((u32)(0x4000 - SIZE_TAIL_BLOCK) / uUnrollInstrSize);
        u32 lTestBlocksInSegment = (u32)0x4000 / uUnrollInstrSize; // the above should be correct, but this one seems to empirically hit better (no decimals)
        lTestBlocksInSegment = (lTestBlocksInSegment * uUnrollInstrSize) / uUnrollSingleInstrSize; // two divs on purpose
        u32 lBlockCost = lTestBlocksInSegment * g_uExtraRounds;

        lInstructions += lBlockCost;
    }

    g_alFrameInstrResult[eFreq][uTest][uIterationNum] = lInstructions;
}

// ---------------------------------------------------------------------------
void calcStatistics(void)
{
    for(u8 f = 0; f < FREQ_COUNT; f++)
    {
        for(u8  t = 0; t < arraysize(g_aoTest); t++)
        {
            u32 lTotal = 0;
            u32 lMin = (u32)-1; // Wraps around to maximum u32 value
            u32 lMax = 0;

            u8 uExtra = 0;

            for(u8 i=0; i<NUM_ITERATIONS; i++)
            {
                u32 n = g_alFrameInstrResult[f][t][i];

                lTotal += n;

                if(n < lMin)
                    lMin = n;

                if(n > lMax)
                    lMax = n;

                if(g_auFrameInstrResultXtra[f][t][i] > uExtra)
                    uExtra = g_auFrameInstrResultXtra[f][t][i];
            }

            if(t<CALIBRATION_TESTS) // max out on the calibration tests.
                g_afFrameInstrResultAvg[f][t] = lMax;
            else
                g_afFrameInstrResultAvg[f][t] = (float)lTotal/NUM_ITERATIONS;

            g_alFrameInstrResultMin[f][t] = lMin;
            g_alFrameInstrResultMax[f][t] = lMax;

            g_auFrameInstrResultXtr2[f][t] = uExtra;
        }
    }


    // Store the first test run as master timing for each frequency
    for(u8 f = 0; f < FREQ_COUNT; f++)
    {
        g_afFrmTotalCyclesNoTail[f] = ((g_afFrameInstrResultAvg[f][0] + FRAME_COUNT_ADD_UP) * g_aoTest[0].uRealSingleCost +
                                       (g_afFrameInstrResultAvg[f][1] + FRAME_COUNT_ADD_UP) * g_aoTest[1].uRealSingleCost) / 2;

        u8 uFrmCycles = FRAME_CYCLES_TAIL_Z80;
        if(g_eCPUMode == Z80_TURBO )
            uFrmCycles += FRAME_CYCLES_TAIL_Z80_TURBO_ADD;

        g_afFrmTotalCycles[f] = g_afFrmTotalCyclesNoTail[f] + (g_auFrameInstrResultXtr2[f][0] + g_auFrameInstrResultXtr2[f][1]) * uFrmCycles / 2;
    }

    // populate the testcost float array
    for(u8 f = 0; f < FREQ_COUNT; f++)
        for(u8 t = 0; t < arraysize(g_aoTest); t++)
            g_afFinalTestCost[f][t] = (g_afFrmTotalCyclesNoTail[f] + g_aoTest[0].uStartupCycleCost - g_aoTest[t].uStartupCycleCost) / g_afFrameInstrResultAvg[f][t];

    u32 lAfter =  ((u32)g_uMinsH1 * 10 + g_uMinsL1) * 60 + ((u32)g_uSecondsH1 * 10 + g_uSecondsL1);
    u32 lBefore = ((u32)g_uMinsH0 * 10 + g_uMinsL0) * 60 + ((u32)g_uSecondsH0 * 10 + g_uSecondsL0);

    u16 nDiff = (s16)(lAfter - lBefore);

    if(g_eCPUMode == Z80_TURBO)
        nDiff = (u16)(unsignedRound((float)nDiff * 1.5f));

    g_iVDPDiff = (s16)(nDiff - 12); // 12 is norm - expected for 0 delay
}


// ---------------------------------------------------------------------------
//
void runLongTest(void)
{
    g_lEndTimeStamp = getTimeStamp();

    g_bRTCWorking = g_lEndTimeStamp != g_lStartTimeStamp;

    if(!g_bRTCWorking)
        return;

#ifdef ROM_OUTPUT_FILE

    disableInterrupt();
    memAPI_enaSltPg2_NI_fromC(g_uSlotidPage2ROM);
    enableInterrupt();
    ENABLE_SEGMENT_PAGE2(TEST_SEG_OFFSET+10);

#else
    u8 uUnrollInstructionsSize = 2;
    u16 nMax = (u16)((u32)(0x4000 - SIZE_LONGTEST_TAIL) / uUnrollInstructionsSize);

    u8* p = (u8*) &runTestAsmInMem;

    for(u16 n = 0; n < nMax; n++)
    {
        memcpy(p, &TEST_LONGTEST_UNROLL, uUnrollInstructionsSize);
        p += uUnrollInstructionsSize;
    }

    memcpy(p, &TEST_LONGTEST_TAIL, SIZE_LONGTEST_TAIL);
#endif

    setPALRefreshRate(false);
    halt();                 // halt here is needed on AX-370, otherwise we get skewed results
    longTest();
}

// ---------------------------------------------------------------------------
void runAllIterations(void)
{

    initPalette();      // just in case we test/trash the palette
 
    bool bPALOrg = getPALRefreshRate();

    setCustomISR();

    for(enum freq_variant f = 0; f < FREQ_COUNT; f++)
    {
        setPALRefreshRate((bool)f);
        halt();                 // halt here is needed on AX-370, otherwise we get skewed results

        for(u8 t = 0; t < arraysize(g_aoTest); t++)
        {

#if DEBUG_INSERT_TURBO_MID_TEST==1
    if(t == 11)
        enableTurboIfAvailable(true);
    else if(t == 12)
        enableTurboIfAvailable(false);
#endif

            setupTestInMemory(t);

            for(u8 i = 0; i < NUM_ITERATIONS; i++)
                runIteration(f, t, i);
        }
    }

    if(g_eCPUMode <= Z80_TURBO)
        runLongTest();

    setPALRefreshRate(bPALOrg);

    restoreOriginalISR();       // sets ROM in page 0 too
    restorePalette();           // uses BIOS. just in case the palette was messed up
}

// ---------------------------------------------------------------------------
// Because SDCC does not come out of the box with support for %f (or %.2f in
// our case), we manually split it up in two %d. float adder (0.005) is as a
// "round(val, 2)" when we truncate using ints
void floatToIntWith2Decimals(float f, IntWith2Decimals* pObj)
{
    f += 0.005;

    float fFrac = f - (u32)f;
    pObj->lInt  = (u32)f;
    pObj->uFrac = (u8)(fFrac * 100);
}

// ---------------------------------------------------------------------------
//
void printReport(void)
{
    printX(g_szRemoveWait);

    // First the frame cycle speed
    printX(g_szSpeedHdrCols);

    u16 nTotalOverhead = (u16)FRAME_CYCLES_INT + FRAME_CYCLES_INT_KICK_OFF + FRAME_CYCLES_COMMON_START + g_aoTest[0].uStartupCycleCost;

    if(g_eCPUMode == Z80_TURBO )
        nTotalOverhead += FRAME_CYCLES_INT_TURBO_ADD + FRAME_CYCLES_INT_KICK_OFF_TURBO_ADD + FRAME_CYCLES_COMMON_START_TURBO_ADD;

    u32 lFrmTotalCyclesNTSC;
    u32 lFrmTotalCyclesPAL;
    s32 dDiffPAL;
    s32 dDiffNTSC;

    lFrmTotalCyclesNTSC = (u32)(g_afFrmTotalCycles[NTSC] + 0.5 + nTotalOverhead);
    dDiffNTSC = (s32)(lFrmTotalCyclesNTSC - alFRAME_CYCLES_TARGET[g_eCPUMode][NTSC]);

    lFrmTotalCyclesPAL = (u32)(g_afFrmTotalCycles[PAL] + 0.5 + nTotalOverhead);
    dDiffPAL = (s32)(lFrmTotalCyclesPAL - alFRAME_CYCLES_TARGET[g_eCPUMode][PAL]);

    u8 szBuf1[50];
    u8 szBuf2[50];

    sprintf(szBuf1,
            g_szSRPart,
            lFrmTotalCyclesNTSC,
            alFRAME_CYCLES_TARGET[g_eCPUMode][NTSC],
            dDiffNTSC);

    sprintf(szBuf2,
            g_szSRPart,
            lFrmTotalCyclesPAL,
            alFRAME_CYCLES_TARGET[g_eCPUMode][PAL],
            dDiffPAL);


    sprintf(g_auBuffer,
            g_szSpeedResult,
            szBuf1,
            szBuf2
           );

    printX(g_auBuffer);
    print(g_szSplitline);

    // Then the tests
    print(g_szReportCols);
    // for(u8 t = CALIBRATION_TESTS; t < arraysize(g_aoTest); t++)
    for(u8 t = 0; t < arraysize(g_aoTest); t++)
    {
        IntWith2Decimals oAvgNTSC, oAvgPAL, oTestCostNTSC, oTestCostPAL;

        floatToIntWith2Decimals(g_afFrameInstrResultAvg[NTSC][t], &oAvgNTSC);
        floatToIntWith2Decimals(g_afFrameInstrResultAvg[PAL][t], &oAvgPAL);
        floatToIntWith2Decimals(g_afFinalTestCost[NTSC][t], &oTestCostNTSC);
        floatToIntWith2Decimals(g_afFinalTestCost[PAL][t], &oTestCostPAL);

        s8 sDiffNTSC = signedRoundX(g_afFinalTestCost[NTSC][t] - g_aoTest[t].uRealSingleCost);
        s8 sDiffPAL = signedRoundX(g_afFinalTestCost[PAL][t] - g_aoTest[t].uRealSingleCost);

        sprintf(g_auBuffer,
                g_szReportValues,
                g_aoTest[t].szTestName,
                oAvgNTSC.lInt,
                oAvgNTSC.uFrac,
                g_alFrameInstrResultMin[NTSC][t],
                g_alFrameInstrResultMax[NTSC][t],
                oTestCostNTSC.lInt,
                oTestCostNTSC.uFrac,
                sDiffNTSC,

                oAvgPAL.lInt,
                oAvgPAL.uFrac,
                g_alFrameInstrResultMin[PAL][t],
                g_alFrameInstrResultMax[PAL][t],
                oTestCostPAL.lInt,
                oTestCostPAL.uFrac,
                sDiffPAL
               );

        printX(g_auBuffer);
    }

    u8* szLast;
    u8 szBuf[50];
    if(g_bRTCWorking)
    {
        sprintf(szBuf, g_szLongInfo, g_iVDPDiff);
        szLast = (u8*)szBuf;
    }
    else
        szLast = (u8*)g_szLongRTCError;

    sprintf(g_auBuffer, g_szLongTest, szLast);
    printX(g_auBuffer);

    // print(g_szNewline);
    printX(g_szSummary);
}

// ---------------------------------------------------------------------------
//
void initRomIfAnyNI(void)
{
#ifdef ROM_OUTPUT_FILE

    g_uBIOS_LINL40 = 80;    // LINL40, MSX BIOS for width/columns. Must be set before we change mode
    changeMode(0);
    disableInterrupt();     // DI was probably changed to EI in previous routine

    establishSlotIDsNI_fromC();

    // Put Upper-segment (ISR++) in page 2 and copy contents to 0xC000
    memAPI_enaSltPg2_NI_fromC(g_uSlotidPage2ROM);
    ENABLE_SEGMENT_PAGE2(UPPER_SEG_ID);
    memcpy((void*)0xC000, (void*)0x8000, (u8*)&UPPERCODE_END - (u8*)&UPPERCODE_BEGIN);
    memAPI_enaSltPg2_NI_fromC(g_uSlotidPage2RAM); // need RAM here to copy in data

    memAPI_enaSltPg0_NI_fromC(g_uSlotidPage0RAM);
    g_uInt38 = 0xC3; // code for JUMP
    memAPI_enaSltPg0_NI_fromC(g_uSlotidPage0BIOS);

#endif
}

// ---------------------------------------------------------------------------
enum cpu_variant detectActiveCPU(void)
{
    enum cpu_variant eCPU = Z80_PLAIN;
    u8 uType = getMSXType();

    if(uType == 2) // MSX2+
    {
        if(hasTurboFeature())
            if(isTurboEnabled())
                eCPU = Z80_TURBO;
    }
    else if(uType == 3) // MSX turbo R
    {
        u8 uCPU = getCPU(); // 0=Z80 (ROM) mode, 1=R800 ROM  mode, 2=R800 DRAM mode

        if(uCPU == 1)  // 
            eCPU = R800_ROM;
        else if(uCPU == 2)
            eCPU = R800_DRAM;
    }

    return eCPU;
}

// ---------------------------------------------------------------------------
#pragma disable_warning 126
u8 main(void)
{
    initRomIfAnyNI();

    if(getMSXType() == 0)
    {
        print(g_szErrorMSX);
        return 1;
    }

    // safeguard against R800 until we support it properly
    s8 sOrgCPU = -1;
    if(getMSXType() == 3) // MSX turbo R
    {
        u8 uCPU = getCPU();
        if(uCPU != 0)
        {
            sOrgCPU = (s8)uCPU;
            changeCPU(0); // 0=Z80 (ROM) mode, 1=R800 ROM  mode, 2=R800 DRAM mode
        }
    }

    g_lStartTimeStamp = getTimeStamp();

#if DEBUG_FORCE_R800_FULLSPEED_IF_AVAILABLE==1
enableR800FullSpeedIfAvailable(true);
#endif

#if DEBUG_FORCE_TURBO_IF_AVAILABLE==1
enableTurboIfAvailable(true);
#endif

    g_eCPUMode = detectActiveCPU();

    sprintf(g_auBuffer, g_szGreeting, NUM_ITERATIONS, g_szMedium, g_aszCPUModes[ g_eCPUMode ]);
    printX(g_auBuffer);

    print(g_szWait);

    // changeMode(5);   // changing mode does not seem to matter at all, so we can just ignore for now
    runAllIterations();
    calcStatistics();
    // changeMode(0);

#if DEBUG_FORCE_TURBO_IF_AVAILABLE==1
    enableTurboIfAvailable(false);
#endif

    printReport();
    // print("testline1\r\n");
    // print("testline2");

#ifdef ROM_OUTPUT_FILE
spin_forever: goto spin_forever;
#endif

    if(sOrgCPU != -1)
        changeCPU(sOrgCPU);

    return 0;
}
