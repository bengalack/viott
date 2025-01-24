// ---------------------------------------------------------------------------
// This program tries to figure out if I/O commands towards the VDP take
// longer time than expected on various systems/engines.
//
// Ouput comes in two variants: 1. DOS 2: ROM (Megarom)
// 
// It is using a custom ISR which does not do much other than storing the
// current address of the program pointer, at address: g_pPCReg
//
// In do mode, tests are added by a new TestDescriptor entry, with code in
// macro-blocks ('tests_as_macros.inc'), which are used in TEST_n_STARTUP/EMPTY
// and TEST_n_UNROLL for DOS version, and in segments in 'rom_tests.s' for
// ROM-version. Buildscript must also include info about any added segment
//
// Assumptions:
//  * There are no line interrupts enabled when we start (DOS)
//
// Notes:
//  * There is no support for global initialisation of RAM variables in this config
//  * Likely some C programming shortcomings - I don't have much experience with C
//  * Sorry about the various macros for tests and #ifdefs, but adding support for
//    ROM in retrospect complicates things
//  * SORRY! for the Hungarian notation, but is helps me when mixing asm and c
//  * Others:
//      Prefixes:
//      * s  = signed char    (s8)
//      * u  = unsigned char  (u8)
//      * i  = signed short   (s16)
//      * n  = unsigned short (u16)
//      * l  = unsigned long  (u32)
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
// #define NUM_ITERATIONS      128 // (max 255) 128 is similar to VATT iterations
#define NUM_ITERATIONS      16
#define TEST_SEG_OFFSET     2	    // Test segments starts here. Only used in ROM code

typedef signed char         s8;
typedef unsigned char       u8;
typedef signed short        s16;
typedef unsigned short      u16;
typedef unsigned long       u32;
typedef const void          (function)( void );

#define halt()				{ __asm halt; __endasm; }
#define enableInterrupt()	{ __asm ei;   __endasm; }
#define disableInterrupt()	{ __asm di;   __endasm; }
#define break()				{ __asm in a,(0x2e);__endasm; } // for debugging. may be risky to use as it trashes A
#define arraysize(arr)      (sizeof(arr)/sizeof((arr)[0]))

typedef struct {
    u8*                     szTestName;                 // max 9 characters
	function*               pFncStartupBlock;
	u8                      uStartupBlockSize;
	function*               pFncUnrollInstruction;       // or plural.
	u8                      uUnrollInstructionsSize;
	u8                      uUnrollSingleInstructionSize;
    enum three_way          eReadVRAM;                  // if we should set up VRAM for write, read or nothing
    u8                      uStartupCycleCost;          // init of regs or so, at start of frame, before repeats
    u8                      uRealSingleCost;            // the cost of the unroll instruction(s) if run once
    bool                    bForceRAMRun;
    u8                      uSegNum;                    // used only in ROM mode
} TestDescriptor;

typedef struct {
    u16 nInt;
    u8  uFrac;
} IntWith2Decimals;

enum three_way { NO, YES, NA };
enum freq_variant { NTSC, PAL, FREQ_COUNT };

// Declarations (see .s-file) ------------------------------------------------
//
u8   getMSXType(void);
u8   getCPU(void);
void changeCPU(u8 uMode);
u8   changeMode(u8 uModeNum);

void print(u8* szMessage);
bool getPALRefreshRate(void);
void setPALRefreshRate(bool bPAL);
void customISR(void);
void setVRAMAddressNI(u8 uBitCodes, u16 nVRAMAddress);
void initPalette(void);             // in case we mess up the palette during testing
void restorePalette(void);

void runTestAsmInMem(void);
void TEST_START_BLOCK_BEGIN(void);  // used for getting address only!
void TEST_START_BLOCK_END(void);    // used for getting address only!

// Consts / ROM friendly -----------------------------------------------------
//
const TestDescriptor    g_aoTest[] = {
                                        {
                                            "sync",             // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_0_UNROLL,      // void             pFncUnrollInstruction;
                                            1,                  // u8               uUnrollInstructionsSize;
                                            1,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            5,                  // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun; - first run is ALWAYS in RAM
                                            0xFF                // u8               uSegNum;
                                        },
                                        {
                                            "outdi98",          // u8*              szTestName;
                                            TEST_1_STARTUP,     // function*        pFncStartupBlock;
                                            5,                  // u8               uStartupBlockSize;
                                            TEST_1_UNROLL,      // void             pFncUnrollInstruction;
                                            4,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            19,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+0                   // u8               uSegNum;
                                        },
                                        {
                                            "out98",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_2_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+1   // u8               uSegNum;
                                        },
                                        {
                                            "in98",             // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_3_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            YES,                // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+2   // u8               uSegNum;
                                        },
                                        {
                                            "in98x",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_4_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+3   // u8               uSegNum;
                                        },
                                        {   // IN Regwrite test, WEAKNESS: two OUTs are used in the STARTUP, making the cost a tad inaccurate!
                                            "in99",             // u8*              szTestName;
                                            TEST_5_STARTUP,     // function*        pFncStartupBlock;
                                            8,                  // u8               uStartupBlockSize;
                                            TEST_5_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            40+2,               // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+4   // u8               uSegNum;
                                        },
                                        {   // Palette test, must init first and restore palette after
                                            "out9A",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_6_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+5   // u8               uSegNum;
                                        },
                                        {   // Stream port test. WEAKNESS: two OUTs are used in the STARTUP, making the cost a tad inaccurate!
                                            "out9B",            // u8*              szTestName;
                                            TEST_7_STARTUP,     // function*        pFncStartupBlock;
                                            8,                  // u8               uStartupBlockSize;
                                            TEST_7_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            40+2,               // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+6   // u8               uSegNum;
                                        },
                                        {
                                            "outi98FMT",        // u8*              szTestName;
                                            TEST_8_STARTUP,     // function*        pFncStartupBlock;
                                            5,                  // u8               uStartupBlockSize;
                                            TEST_8_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            19,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+7   // u8               uSegNum;
                                        },
                                        {
                                            "outi98RAM",        // u8*              szTestName;
                                            TEST_8_STARTUP,     // function*        pFncStartupBlock;
                                            5,                  // u8               uStartupBlockSize;
                                            TEST_8_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            19,                 // u8               uStartupCycleCost;
                                            18,                 // u8               uRealSingleCost;
                                            true,               // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+8   // u8               uSegNum;
                                        },
                                        {   // Just pick a non-used port (I hope), and check the speed
                                            "(ex) in06",        // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_9_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionsSize;
                                            2,                  // u8               uUnrollSingleInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12,                 // u8               uRealSingleCost;
                                            false,              // bool             bForceRAMRun;
                                            TEST_SEG_OFFSET+9   // u8               uSegNum;
                                        }
                                     };

const u8                g_szErrorMSX[]      = "MSX2 and above is required";
const u8                g_szGreeting[]      = "VDP I/O Timing Test [z80] v1.3 - Test: %d repeats [%s]\r\n";
const u8                g_szWait[]          = "...please wait a minute...";
const u8                g_szRemoveWait[]    = "\r                                \r\n";
const u8                g_szReportCols[]    = "          Hz  avg      min   max   cost  ~d | Hz  avg      min   max   cost  ~d\r\n";
const u8                g_szReportValues[]  = "% 9s %2s % 5hu.%02d % 5hu % 5hu % 2d.%02d %+ 2d | %2s % 5hu.%02d % 5hu % 5hu % 2d.%02d %+ 2d\r\n";

const u8                g_szSpeedHdrCols[]  = "          Hz computer  norm  delta (~d)     | Hz computer  norm  delta (~d)\r\n";
const u8                g_szSpeedResult[]   = "Frmcycles %s % 8ld % 5ld %+ 10d     | %s % 8ld % 5ld %+ 10d\r\n";

const u8                g_szNewline[]       =  "\r\n";

const u8* const         g_aszFreq[]    = {"60", "50"}; // must be chars

const u32               anFRAME_CYCLES_TARGET[]     = {59736, 71364}; // assumed "ideal"
const u16               FRAME_CYCLES_INT            = 171;
const u16               FRAME_CYCLES_INT_KICK_OFF   = 14 + 11; // +11 is the JP at 0x0038
const u16               FRAME_CYCLES_COMMON_START   = 33;

// RAM variables -------------------------------------------------------------
//
void* __at(0x0039)      g_pInterrupt;       // We assume that 0x0038 already holds 0xC3 (JP) in dos mode at startup
void*                   g_pInterruptOrg;
u8                      g_auBuffer[ 256 ];  // temp/general buffer here to avoid stack explosion

volatile u8*            g_pPCReg;           // pointer to PC-reg when the interrupt was triggered
volatile bool           g_bStorePCReg;

float                   g_afFrmTotalCycles     [FREQ_COUNT];
u16                     g_anFrameInstrResult   [FREQ_COUNT][arraysize(g_aoTest)][NUM_ITERATIONS];
float                   g_afFrameInstrResultAvg[FREQ_COUNT][arraysize(g_aoTest)];
u16                     g_anFrameInstrResultMin[FREQ_COUNT][arraysize(g_aoTest)];
u16                     g_anFrameInstrResultMax[FREQ_COUNT][arraysize(g_aoTest)];

size_t                  g_nTestStartBlockSize;
u16*                    g_pCurTestBaseline; // Start of unrolleds

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
// Test code: All bodies must be pure asm via __naked and without ret/return
// at the end. See the "tests_as_macros.inc"-file
//
void TEST_EMPTY(void) __naked {
__asm .include "tests_as_macros.inc" __endasm;  // also used by the tests below
}
void TEST_0_UNROLL(void) __naked {
__asm macroTEST_0_UNROLL __endasm;
}
void TEST_1_STARTUP(void) __naked {
__asm macroTEST_1_STARTUP __endasm;
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
// Just set write address to upper 64kB area. We will not see this garbage
// on screen while in DOS prompt/screen
void prepareVDP(enum three_way eRead)
{
    disableInterrupt(); // generates "info 218: z80instructionSize() failed to parse line node, assuming 999 bytes" - dunno why, SDCC funkiness

    if(eRead == NO) // == Write
        setVRAMAddressNI(1 | 0x40, 0x0000);
    else if(eRead == YES) // ignore if NA
        setVRAMAddressNI(1 | 0x00, 0x0000);

    enableInterrupt();
}

// ---------------------------------------------------------------------------
// The first part/block is identical for all tests
void initTestInMemorySetups(void)
{
    g_nTestStartBlockSize =  (u8*)&TEST_START_BLOCK_END - (u8*)&TEST_START_BLOCK_BEGIN;

    memcpy( &runTestAsmInMem, &TEST_START_BLOCK_BEGIN, g_nTestStartBlockSize );
}

// ---------------------------------------------------------------------------
// For DOS mode: Copy test in at runTestAsmInMem, just after the common start
// block with unrolled instructions. First the startblock, and then x amount
// of unrolleds filling a PAL frame (+ a buffer: we set 75000 cycles as max
// cycles in a frame)
// Some tests are forced to run in RAM (i.e. dos mode). Like the first run.
// As this seem to be the fastest, most accurate and reliable way to measure
// the speed.
// Example: MSX Pico slows down if you have megaroms (on plain commands too,
// not only outs), so we need to use RAM. (MSX pico code in page1 is also
// slown down, so putting tests in RAM does not fix things 100%).
//
void setupTestInMemoryDOS(u8 uTest, u8* p)
{
    memcpy(p, *g_aoTest[uTest].pFncStartupBlock, g_aoTest[uTest].uStartupBlockSize);

    p += g_aoTest[uTest].uStartupBlockSize;
    g_pCurTestBaseline = p;

    // Below: The fastest instr (5 cycles, 1 byte) would max give 0x3A98 bytes
    // Medium instr (8 cycles, 2 bytes) gives 0x493E bytes - we should be fine when using RAM
    u8 n = g_aoTest[uTest].uUnrollInstructionsSize / g_aoTest[uTest].uUnrollSingleInstructionSize;
    u16 nMax = (75000/g_aoTest[uTest].uRealSingleCost)/n;

    if((nMax + g_aoTest[uTest].uStartupBlockSize + g_nTestStartBlockSize)>0x4000) // check when developing!
        break();

    for(u16 n = 0; n < nMax; n++)
    {
        memcpy(p, *g_aoTest[uTest].pFncUnrollInstruction, g_aoTest[uTest].uUnrollInstructionsSize);
        p += g_aoTest[uTest].uUnrollInstructionsSize;
    }

    *p = 0xC9; // add a "ret" at the end as security
}

// ---------------------------------------------------------------------------
// For ROM mode: Set correct segment (tests already laid out) in page 2
// (runTestAsmInMem is within the segment). If the test is the "baseline"-test
// (the first test), we run this using RAM in page 2, as 
//
#ifdef ROM_OUTPUT_FILE
void setupTestInMemoryROM(u8 uTest, u8* p)
{
    if(g_aoTest[uTest].bForceRAMRun)
    {
        disableInterrupt();
        memAPI_enaSltPg2_NI_fromC(g_uSlotidPage2RAM);
        enableInterrupt();
        setupTestInMemoryDOS(uTest, p);
    }
    else
    {
        p += g_aoTest[uTest].uStartupBlockSize;
        g_pCurTestBaseline = p;

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
    u8* p = (u8*) &runTestAsmInMem;
    p += g_nTestStartBlockSize;

#ifdef ROM_OUTPUT_FILE
    setupTestInMemoryROM(uTest, p);
#else
    setupTestInMemoryDOS(uTest, p);
#endif
}

// ---------------------------------------------------------------------------
void runIteration(enum freq_variant eFreq, u8 uTest, u8 uIterationNum)
{
    prepareVDP( g_aoTest[ uTest ].eReadVRAM );

    runTestAsmInMem();

    u16 nLength = (u16)g_pPCReg - (u16)g_pCurTestBaseline; 
    u16 nInstructions = nLength/g_aoTest[ uTest ].uUnrollSingleInstructionSize;

    g_anFrameInstrResult[eFreq][uTest][uIterationNum] = nInstructions;
}

// ---------------------------------------------------------------------------
void runAllIterations(void)
{
    u8 uType = getMSXType();
    u8 uCPUOrg = 0;

    if(uType == 3)              // 3 = MSX turbo R
    {
        uCPUOrg = getCPU();
        changeCPU(0);           // 0 = Z80 (ROM) mode
    }

    bool bPALOrg = getPALRefreshRate();

    setCustomISR();

    for(enum freq_variant f = 0; f < FREQ_COUNT; f++)
    {
        setPALRefreshRate((bool)f);
        halt();                 // halt here is needed on AX-370, otherwise we get skewed results

        for(u8 t = 0; t < arraysize(g_aoTest); t++)
        {
            setupTestInMemory(t);

            for(u8 i = 0; i < NUM_ITERATIONS; i++)
                runIteration(f, t, i);
        }
    }


    restoreOriginalISR();
    setPALRefreshRate(bPALOrg);

    if(uType == 3)              // restore orginal CPU
        changeCPU(uCPUOrg);
}

// ---------------------------------------------------------------------------
void calcStatistics(void)
{
    for(enum freq_variant f = 0; f < FREQ_COUNT; f++)
    {
        for(u8  t = 0; t < arraysize(g_aoTest); t++)
        {
            u32 lTotal = 0;
            u16 nMin = (u16)-1; // Wraps around to maximum u16 value
            u16 nMax = 0;

            for(u8 i=0; i<NUM_ITERATIONS; i++)
            {
                u16 n = g_anFrameInstrResult[f][t][i];

                lTotal += n;

                if(n < nMin)
                    nMin = n;

                if(n > nMax)
                    nMax = n;
            }

            g_afFrameInstrResultAvg[f][t] = (float)lTotal/NUM_ITERATIONS;
            g_anFrameInstrResultMin[f][t] = nMin;
            g_anFrameInstrResultMax[f][t] = nMax;
        }
    }

    // Store the first test run as master timing for each frequency
    for(enum freq_variant f = 0; f < FREQ_COUNT; f++)
        g_afFrmTotalCycles[f] = g_afFrameInstrResultAvg[f][0] * g_aoTest[0].uRealSingleCost;
}

// ---------------------------------------------------------------------------
// Because SDCC does not come out of the box with support for %f (or %.2f in
// our case), we manually split it up in two %d. float adder (0.005) is as a
// "round(val, 2)" when we truncate using ints
void floatToIntWith2Decimals(float f, IntWith2Decimals* pObj)
{
    f += 0.005;

    float fFrac = f - (u16)f;
    pObj->nInt  = (u16)f;
    pObj->uFrac = (u8)(fFrac * 100);
}

// ---------------------------------------------------------------------------
// Special rounding.
// Caters for the 3rd decimal already presented to user rounded up (using +0.005),
// Just to avoid making it look like a bug.
//
s8 signedRoundX(float f)
{
    return f<0?(s8)(f-0.5):(s8)(f+0.505);
}

// ---------------------------------------------------------------------------
//
void printReport(void)
{
    print(g_szRemoveWait);

    // First the frame cycle speed
    print(g_szSpeedHdrCols);

    u16 nTotalOverhead = FRAME_CYCLES_INT + FRAME_CYCLES_INT_KICK_OFF + FRAME_CYCLES_COMMON_START + g_aoTest[0].uStartupCycleCost; // latter should 0

    u32 lFRmTotalCycles60Hz = (u32)(g_afFrmTotalCycles[NTSC] + 0.5 + nTotalOverhead);
    u32 lFRmTotalCycles50Hz = (u32)(g_afFrmTotalCycles[PAL] + 0.5 + nTotalOverhead);

    sprintf(g_auBuffer,
            g_szSpeedResult,
            g_aszFreq[NTSC],
            lFRmTotalCycles60Hz,
            anFRAME_CYCLES_TARGET[NTSC],
            (s16)(lFRmTotalCycles60Hz - anFRAME_CYCLES_TARGET[NTSC]),
            g_aszFreq[PAL],
            lFRmTotalCycles50Hz,
            anFRAME_CYCLES_TARGET[PAL],
            (s16)(lFRmTotalCycles50Hz - anFRAME_CYCLES_TARGET[PAL])
           );

    print(g_auBuffer);
    print(g_szNewline);

    // Then the tests
    print(g_szReportCols);
    for(u8 t = 1; t < arraysize(g_aoTest); t++)
    {
        float fTestCost60Hz = (g_afFrmTotalCycles[NTSC] - g_aoTest[t].uStartupCycleCost) / g_afFrameInstrResultAvg[NTSC][t];
        float fTestCost50Hz = (g_afFrmTotalCycles[PAL] - g_aoTest[t].uStartupCycleCost) / g_afFrameInstrResultAvg[PAL][t];

        IntWith2Decimals oAvg60Hz, oAvg50Hz, oTestCost60Hz, oTestCost50Hz;

        floatToIntWith2Decimals(g_afFrameInstrResultAvg[NTSC][t], &oAvg60Hz);
        floatToIntWith2Decimals(g_afFrameInstrResultAvg[PAL][t], &oAvg50Hz);
        floatToIntWith2Decimals(fTestCost60Hz, &oTestCost60Hz);
        floatToIntWith2Decimals(fTestCost50Hz, &oTestCost50Hz);

        s8 sDiff60Hz = signedRoundX(fTestCost60Hz - g_aoTest[t].uRealSingleCost);
        s8 sDiff50Hz = signedRoundX(fTestCost50Hz - g_aoTest[t].uRealSingleCost);

        sprintf(g_auBuffer,
                g_szReportValues,
                g_aoTest[t].szTestName,
                g_aszFreq[NTSC],
                oAvg60Hz.nInt,
                oAvg60Hz.uFrac,
                g_anFrameInstrResultMin[NTSC][t],
                g_anFrameInstrResultMax[NTSC][t],
                oTestCost60Hz.nInt,
                oTestCost60Hz.uFrac,
                sDiff60Hz,

                g_aszFreq[PAL],
                oAvg50Hz.nInt,
                oAvg50Hz.uFrac,
                g_anFrameInstrResultMin[PAL][t],
                g_anFrameInstrResultMax[PAL][t],
                oTestCost50Hz.nInt,
                oTestCost50Hz.uFrac,
                sDiff50Hz
                );

        print(g_auBuffer);
    }
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
    ENABLE_SEGMENT_PAGE2( UPPER_SEG_ID );
    memcpy( (void*)0xC000, (void*)0x8000, (u8*)&UPPERCODE_END - (u8*)&UPPERCODE_BEGIN );
    memAPI_enaSltPg2_NI_fromC(g_uSlotidPage2RAM); // need RAM here to copy in data

    memAPI_enaSltPg0_NI_fromC(g_uSlotidPage0RAM);
    g_uInt38 = 0xC3; // code for JUMP
    memAPI_enaSltPg0_NI_fromC(g_uSlotidPage0BIOS);

#endif
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
    
    sprintf(g_auBuffer, g_szGreeting, NUM_ITERATIONS, g_szMedium);
    print(g_auBuffer);

    print(g_szWait);

    initPalette();      // just in case we test/trash the palette
 
    initTestInMemorySetups();
    // changeMode(5);   // changing mode does not seem to matter at all, so we can just ignore for now
    runAllIterations();
    restorePalette();   // just in case the palette was messed up
    calcStatistics();
    // changeMode(0);
    printReport();

#ifdef ROM_OUTPUT_FILE
spin_forever: goto spin_forever;
#endif

    return 0;
}
