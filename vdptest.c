// ---------------------------------------------------------------------------
// This program tries to figure out if I/O commands towards the VDP take
// longer time than expected on various systems/engines. It also measures
// if code run in (mega)rom segments are slower than normal.
//
// The tool comes in two variants: 1. DOS 2: ROM (Megarom)
// 
// It is using a custom ISR which does not do much other than storing the
// current address of the program pointer, at address: g_pPCReg
//
// In do mode, tests are added by a new TestDescriptor entry, with code in
// macro-blocks ('tests_as_macros.inc'), which are used in TEST_n_STARTUP/EMPTY
// and TEST_n_UNROLL for DOS version, and in segments in 'rom_tests.s' for
// ROM-version. The ROM-buildscript must also include info about any added segment.
//
// The two first tests are used to determine the amount of cycles available
// in a frame. If we only used one, it would have the accuracy in the range [0,5]
// as the test-instruction takes 5 cycles. By also using a test with accuracy in
// the range [0,7] we can get the accuracy down to range [0,3] which proves
// to conclude better on the statistics on some machines. These tests must NOT
// remotely affect ROM. 
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
#define NUM_ITERATIONS      4      // Can`t see that many are needed, 16 seems ok
#define TEST_SEG_OFFSET     2	    // Test segments starts here. Only used in ROM code
#define CALIBRATION_TESTS   2	    // We use these for finding the overall available cycles in a frame
#define FRM_DIFF_THRESHOLD  7	    // If diff is bigger, state NOT OK (guessing on this value!)
#define SIZE_TAIL_BLOCK     7	    // bytes
#define SIZE_LONGTEST_TAIL  7	    // bytes

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

enum cpu_variant {Z80_PLAIN, Z80_TURBO, R800_ROM, R800_DRAM};
enum three_way {NO, YES, NA};
enum freq_variant {NTSC, PAL, FREQ_COUNT};
enum calc_bitmask {CC_NONE        = 0,
                   CC_VDP_DIFF    = 0b1,
                   CC_ROM_DIFF_A  = 0b10,
                   CC_ROM_DIFF_B  = 0b100,              // ROM I/O
                   CC_ROM2_DIFF_A = 0b1000,
                   CC_ROM2_DIFF_B = 0b10000,            // ROM fundamental
                   CC_ROM3_DIFF_A = 0b100000,
                   CC_ROM3_DIFF_B = 0b1000000};         // ROM references (like "ld a,(hl)")

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
    enum calc_bitmask       eCalcCandidate;
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
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
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
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
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
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
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
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
                                            TEST_SEG_OFFSET+1   // u8               uSegNum;
                                        },
                                        {
                                            "in98x",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_4_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
                                            TEST_SEG_OFFSET+2   // u8               uSegNum;
                                        },
                                        {
                                            "in99",             // u8*              szTestName;
                                            TEST_5_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_5_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            51,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
                                            TEST_SEG_OFFSET+3   // u8               uSegNum;
                                        },
                                        {   // Palette test, must init first and restore palette after
                                            "out9A",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_6_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
                                            TEST_SEG_OFFSET+4   // u8               uSegNum;
                                        },
                                        {   // Stream port test
                                            "out9B",            // u8*              szTestName;
                                            TEST_7_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_7_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            51,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
                                            TEST_SEG_OFFSET+5   // u8               uSegNum;
                                        },
                                        {
                                            "outi98FMT",        // u8*              szTestName;
                                            TEST_8_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_8_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            30,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_NONE,            // enum calc_bitmask eCalcCandidate;
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
                                            CC_VDP_DIFF,        // enum calc_bitmask eCalcCandidate;
                                            TEST_SEG_OFFSET+6   // u8               uSegNum;
                                        },
                                        {   // Just pick a non-used port (I hope), and check the speed
                                            "!in06FMT",         // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_9_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_ROM_DIFF_B,      // enum calc_bitmask eCalcCandidate;
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
                                            CC_ROM_DIFF_A,      // enum calc_bitmask eCalcCandidate;
                                            0xFF                // u8               uSegNum;
                                        },
                                        {   
                                            "!incaFMT",         // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            TEST_A_UNROLL,      // void             pFncUnrollInstruction;
                                            1,                  // u8               uUnrollInstructionsSize;
                                            1,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            11,                 // u8               uStartupCycleCost;
                                            5,                  // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_ROM2_DIFF_B,     // enum calc_bitmask eCalcCandidate;
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
                                            CC_ROM2_DIFF_A,     // enum calc_bitmask eCalcCandidate;
                                            0xFF                // u8               uSegNum;
                                        },
                                        {   
                                            "!cpiFMT",          // u8*              szTestName;
                                            TEST_B_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_B_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            22,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            CC_ROM3_DIFF_B,     // enum calc_bitmask eCalcCandidate;
                                            TEST_SEG_OFFSET+9   // u8               uSegNum;
                                        },
                                        {   
                                            "!cpiRAM",          // u8*              szTestName;
                                            TEST_B_STARTUP,     // function*        pFncStartupBlock;
                                            TEST_B_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            22,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun;
                                            CC_ROM3_DIFF_A,     // enum calc_bitmask eCalcCandidate;
                                            0xFF                // u8               uSegNum;
                                        }
                                     };

const u8* const         g_aszCPUModes[]      = {"z80 @ 3.5MHz","z80 @ 5.7MHz (turbo)", "r800 @ 7.2MHz (comp)", "r800 @ 7.2MHz (DRAM)"};


const u8                g_szErrorMSX[]      = "MSX2 and above is required";
const u8                g_szGreeting[]      = "VDP I/O Timing Test v1.4 - %d repeats, %s, CPU: %s\r\n"; 
const u8                g_szWait[]          = "...please wait 30 seconds or so...";
const u8                g_szRemoveWait[]    = "\r                                  \r\n";
const u8                g_szReportCols[]    = "               avg   min   max  cost  ~d |      avg   min   max  cost  ~d\r\n";
const u8                g_szReportValues[]  = "%9s %5ld.%02d %5ld %5ld %2ld.%02d %+3d | %5ld.%02d %5ld %5ld %2ld.%02d %+3d\r\n";

const u8                g_szSpeedHdrCols[]  = "          Hz computer  norm  delta (~d)  | Hz computer  norm  delta (~d)\r\n";
const u8                g_szSpeedResult[]   = "Frmcycles %s %8ld %5ld %+11ld  | %s %8ld %5ld %+11ld\r\n";

const u8                g_szSummary[]       = "[SUMMARY]  Cycles:%s  VDP_I/O:%+d,%+d%s\r\n";
const u8                g_szSummaryROM[]    = "  ROM:%+d  ROM_I/O:%+d  ROM_REF:%+d";
const u8                g_szSummaryROM2[]   = "  (ROM not tested)";

const u8                g_szNewline[]       = "\r\n";

const u8* const         g_aszFreq[]         = {"60", "50"}; // must be chars

const u32               anFRAME_CYCLES_TARGET[]     = {59736, 71364}; // assumed "ideal"
const u8                FRAME_CYCLES_INT            = 171;
const u8                FRAME_CYCLES_INT_KICK_OFF   = 14 + 11; // +11 is the JP at 0x0038
const u8                FRAME_CYCLES_COMMON_START   = 72;   // cycles after halt

const u8                FRAME_CYCLES_TAIL_Z80   = 45;   // cycles after halt


// RAM variables -------------------------------------------------------------
//
enum cpu_variant        g_eCPUMode;
void* __at(0x0039)      g_pInterrupt;       // We assume that 0x0038 already holds 0xC3 (JP) in dos mode at startup
void*                   g_pInterruptOrg;
u8                      g_auBuffer[120];    // temp/general buffer here to avoid stack explosion

volatile u8*            g_pPCReg;           // pointer to PC-reg when the interrupt was triggered
volatile bool           g_bStorePCReg;
volatile bool           g_uRomExtraRounds;  // test is "too" fast, one full segment is processed multiple times
function*               g_pFncCurStartupBlock;

                        // RESULTS BELOW. As R800 can have instructions of 1 cycle only, we can get iterations with > u16 in PAL
float                   g_afFrmTotalCycles     [FREQ_COUNT];
u32                     g_alFrameInstrResult   [FREQ_COUNT][arraysize(g_aoTest)][NUM_ITERATIONS];
float                   g_afFrameInstrResultAvg[FREQ_COUNT][arraysize(g_aoTest)];
u32                     g_alFrameInstrResultMin[FREQ_COUNT][arraysize(g_aoTest)];
u32                     g_alFrameInstrResultMax[FREQ_COUNT][arraysize(g_aoTest)];
float                   g_afFinalTestCost      [FREQ_COUNT][arraysize(g_aoTest)];
s8                      g_sVDPDiff;
s8                      g_sVDPDiff2;
s8                      g_sROMDiff;
s8                      g_sROM2Diff;
s8                      g_sROM3Diff;

                        // Long test timings via RTC (start:0, end:1)
u8                      g_uSecondsL0;
u8                      g_uSecondsH0;
u8                      g_uMinsL0;
u8                      g_uMinsH0;

u8                      g_uSecondsL1;
u8                      g_uSecondsH1;
u8                      g_uMinsL1;
u8                      g_uMinsH1;

// u16                     g_nMax; // variable made global for easier debugging
// u8 debug1, debug2;

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
__asm .include "tests_as_macros.inc" ; // also used by the tests below
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
void TEST_5_STARTUP(void) __naked {
__asm macroTEST_5_STARTUP __endasm;
}
void TEST_5_UNROLL(void) __naked {
__asm macroTEST_5_UNROLL __endasm;
}
void TEST_6_UNROLL(void) __naked {
__asm macroTEST_6_UNROLL __endasm;
}
void TEST_7_STARTUP(void) __naked {
__asm macroTEST_7_STARTUP __endasm;
}
void TEST_7_UNROLL(void) __naked {
__asm macroTEST_7_UNROLL __endasm;
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
// Special rounding. Caters for the 3rd decimal already presented to user
// rounded up (using +0.005). Just to avoid making it look like a bug.
//
s8 signedRoundX(float f)
{
    return f<0?(s8)(f-0.5):(s8)(f+0.505);
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

    // break();

    commonStartForAllTests();

    u16 nLength = (u16)g_pPCReg - (u16)&runTestAsmInMem;
    u32 lInstructions = nLength/g_aoTest[uTest].uUnrollSingleInstructionSize;

    if(g_uRomExtraRounds!=0)
    {
        u16 nTestBlocksInSegment = (u16)((u32)(0x4000 - SIZE_TAIL_BLOCK) / g_aoTest[uTest].uUnrollInstructionsSize);
        u8 uDivider = g_aoTest[uTest].uUnrollInstructionsSize / g_aoTest[uTest].uUnrollSingleInstructionSize;
        lInstructions += (u32)nTestBlocksInSegment * g_uRomExtraRounds / uDivider;

        lInstructions += FRAME_CYCLES_TAIL_Z80 / g_aoTest[uTest].uRealSingleCost; // make dependent on CPU!
    }

    g_alFrameInstrResult[eFreq][uTest][uIterationNum] = lInstructions;
}

// ---------------------------------------------------------------------------
//
void runLongTest(void)
{
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

        // break();

        for(u8 t = 0; t < arraysize(g_aoTest); t++)
        {
                                                                // // test!
                                                                // if(t == 11)
                                                                //     enableTurboIfAvailable(true);
                                                                // else if(t == 12)
                                                                //     enableTurboIfAvailable(false);

            setupTestInMemory(t);

            for(u8 i = 0; i < NUM_ITERATIONS; i++)
                runIteration(f, t, i);
        }
    }

    break();

    runLongTest();

    setPALRefreshRate(bPALOrg);

    restoreOriginalISR();       // sets ROM in page 0 too
    restorePalette();           // uses BIOS. just in case the palette was messed up
}

// ---------------------------------------------------------------------------
s8 getTestCostDiff(enum calc_candidate eBitMask)
{
    // find the fundamental ROM diff
    u8 uNumTests = 0;
    float fTestDiffsTotal = 0;

    for(u8 f = 0; f < FREQ_COUNT; f++)
    {
        for(u8 t = CALIBRATION_TESTS; t < arraysize(g_aoTest); t++)
        {
            if((g_aoTest[t].eCalcCandidate & eBitMask) != 0)
            {
                uNumTests++;
                fTestDiffsTotal += (g_afFinalTestCost[f][t] - g_aoTest[t].uRealSingleCost);
            }
        }
    }

    if(uNumTests == 0) // we MUST have at least one of these
        break();

    return signedRoundX(fTestDiffsTotal / uNumTests);
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

            for(u8 i=0; i<NUM_ITERATIONS; i++)
            {
                u32 n = g_alFrameInstrResult[f][t][i];

                lTotal += n;

                if(n < lMin)
                    lMin = n;

                if(n > lMax)
                    lMax = n;
            }

            g_afFrameInstrResultAvg[f][t] = (float)lTotal/NUM_ITERATIONS;
            g_alFrameInstrResultMin[f][t] = lMin;
            g_alFrameInstrResultMax[f][t] = lMax;
        }
    }

    // Store the first test run as master timing for each frequency
    for(u8 f = 0; f < FREQ_COUNT; f++)
        g_afFrmTotalCycles[f] = (g_afFrameInstrResultAvg[f][0] * g_aoTest[0].uRealSingleCost + g_afFrameInstrResultAvg[f][1] * g_aoTest[1].uRealSingleCost)/2;

    // populate the testcost float array
    for(u8 f = 0; f < FREQ_COUNT; f++)
        for(u8 t = CALIBRATION_TESTS; t < arraysize(g_aoTest); t++)
            g_afFinalTestCost[f][t] = (g_afFrmTotalCycles[f] - (g_aoTest[t].uStartupCycleCost - g_aoTest[0].uStartupCycleCost)) / g_afFrameInstrResultAvg[f][t];

    s8 sROMDiffA = getTestCostDiff(CC_ROM_DIFF_A);
    s8 sROMDiffB = getTestCostDiff(CC_ROM_DIFF_B);
    g_sROMDiff = sROMDiffB - sROMDiffA; // ROM I/O

    s8 sROM2DiffA = getTestCostDiff(CC_ROM2_DIFF_A);
    s8 sROM2DiffB = getTestCostDiff(CC_ROM2_DIFF_B);
    g_sROM2Diff = sROM2DiffB - sROM2DiffA; // ROM fundament

    s8 sROM3DiffA = getTestCostDiff(CC_ROM3_DIFF_A);
    s8 sROM3DiffB = getTestCostDiff(CC_ROM3_DIFF_B);
    g_sROM3Diff = sROM3DiffB - sROM3DiffA; // ROM REFs

    g_sVDPDiff = getTestCostDiff(CC_VDP_DIFF);

    u32 lAfter =  ((u32)g_uMinsH1 * 10 + g_uMinsL1) * 60 + ((u32)g_uSecondsH1 * 10 + g_uSecondsL1);
    u32 lBefore = ((u32)g_uMinsH0 * 10 + g_uMinsL0) * 60 + ((u32)g_uSecondsH0 * 10 + g_uSecondsL0);

    g_sVDPDiff2 = (s8)((lAfter - lBefore) - 12); // 12 is norm - expected for 0 delay
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
    print(g_szRemoveWait);

    // First the frame cycle speed
    print(g_szSpeedHdrCols);

    u16 nTotalOverhead = (u16)FRAME_CYCLES_INT + FRAME_CYCLES_INT_KICK_OFF + FRAME_CYCLES_COMMON_START + g_aoTest[0].uStartupCycleCost;

    u32 lFrmTotalCyclesNTSC;
    u32 lFrmTotalCyclesPAL;
    s32 dDiffPAL;
    s32 dDiffNTSC;

    lFrmTotalCyclesNTSC = (u32)(g_afFrmTotalCycles[NTSC] + 0.5 + nTotalOverhead);
    dDiffNTSC = (s32)(lFrmTotalCyclesNTSC - anFRAME_CYCLES_TARGET[NTSC]);

    lFrmTotalCyclesPAL = (u32)(g_afFrmTotalCycles[PAL] + 0.5 + nTotalOverhead);
    dDiffPAL = (s32)(lFrmTotalCyclesPAL - anFRAME_CYCLES_TARGET[PAL]);

    sprintf(g_auBuffer,
            g_szSpeedResult,
            g_aszFreq[NTSC],
            lFrmTotalCyclesNTSC,
            anFRAME_CYCLES_TARGET[NTSC],
            dDiffNTSC,
            g_aszFreq[PAL],
            lFrmTotalCyclesPAL,
            anFRAME_CYCLES_TARGET[PAL],
            dDiffPAL
           );

    print(g_auBuffer);
    print(g_szNewline);

    // Then the tests
    print(g_szReportCols);
    for(u8 t = CALIBRATION_TESTS; t < arraysize(g_aoTest); t++)
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

        print(g_auBuffer);
    }

    print(g_szNewline);

    u8* szOkOrNot;

    if((abs32(dDiffNTSC) > FRM_DIFF_THRESHOLD) || (abs32(dDiffPAL) > FRM_DIFF_THRESHOLD))
        szOkOrNot = (u8*)"NOT OK";
    else
        szOkOrNot = (u8*)"OK";

    u8* szRom;

#ifdef ROM_OUTPUT_FILE
    u8 szBuf[50];
    sprintf(szBuf, g_szSummaryROM, g_sROM2Diff, g_sROMDiff, g_sROM3Diff);
    szRom = (u8*)szBuf;
#else
    szRom = g_szSummaryROM2;
#endif

    sprintf(g_auBuffer, g_szSummary, szOkOrNot, g_sVDPDiff, g_sVDPDiff2, szRom);
    print(g_auBuffer);
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
    

                    // Testing code for provoking various speeds
                    enableR800FullSpeedIfAvailable(true);

                    // // Testing code for provoking various speeds
                    // enableTurboIfAvailable(true);

    g_eCPUMode = detectActiveCPU();

    sprintf(g_auBuffer, g_szGreeting, NUM_ITERATIONS, g_szMedium, g_aszCPUModes[ g_eCPUMode ]);
    print(g_auBuffer);

    print(g_szWait);

    // changeMode(5);   // changing mode does not seem to matter at all, so we can just ignore for now
    runAllIterations();
    calcStatistics();
    // changeMode(0);
    printReport();
    // print("testline1\r\n");
    // print("testline2");

#ifdef ROM_OUTPUT_FILE
spin_forever: goto spin_forever;
#endif

    break();

    return 0;
}
