// ---------------------------------------------------------------------------
// This program tries to figure out if I/O commands towards the VDP take
// longer time than expected various systems/engines.
// 
// It is using a custom ISR which does not do much other than storing the
// current address of the program pointer, at address: g_pPCReg
//
// Add a test by adding a TestDescriptor entry, with code in blocks for
// TEST_n_STARTUP/EMPTY and TEST_n_UNROLL
//
// Assumptions:
//  * We start in DOS, hence 0x0038 already contains 0xC3 (jp)
//  * There are no line interrupts enabled
//
// Notes:
//  * There is no support for global initialisation of RAM variables in this config
//  * Likely some C programming shortcomings - I don't have much experience with C
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
// author: pal.hansen@gmail.com
// ---------------------------------------------------------------------------

#include <stdio.h>      // herein be sprintf 
#include <string.h>     // memcpy
#include <stdbool.h>

// Typedefs & defines --------------------------------------------------------
//
// #define NUM_ITERATIONS      128 // (max 255) 128 is similar to VATT iterations
#define NUM_ITERATIONS      16

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
	u8                      uUnrollInstructionSize;
    enum three_way          eReadVRAM;                  // if we should set up VRAM for write, read or nothing
    u8                      uStartupCycleCost;          // init of regs or so, at start of frame, before repeats
    u8                      uRealSingleCost;            // the cost of the unroll instruction(s) if run once
} TestDescriptor;

typedef struct {
    u16 nInt;
    u8  uFrac;
} IntWith2Decimals;

enum three_way { NO, YES, NA };
enum freq_variant { NTSC, PAL, FREQ_COUNT };

// Declarations (see .s-file) ------------------------------------------------
//
u8   getMSXType();
u8   getCPU();
void changeCPU(u8 uMode);
u8   changeMode(u8 uModeNum);

void print(u8* szMessage);
bool getPALRefreshRate();
void setPALRefreshRate(bool bPAL);
void customISR();
void setVRAMAddressNI(u8 uBitCodes, u16 nVRAMAddress);
void initPalette();         // in case we mess up the palette during testing
void restorePalette();

void runTestAsmInHeap();

void TEST_START_BLOCK_BEGIN();  // used for getting address only!
void TEST_START_BLOCK_END();    // used for getting address only!

// Consts / ROM friendly -----------------------------------------------------
//
const TestDescriptor    g_aoTest[] = {
                                        {
                                            "sync",             // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_0_UNROLL,      // void             pFncUnrollInstruction;
                                            1,                  // u8               uUnrollInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            5                   // u8               uRealSingleCost;
                                        },
                                        {
                                            "outi98",           // u8*              szTestName;
                                            TEST_1_STARTUP,     // function*        pFncStartupBlock;
                                            5,                  // u8               uStartupBlockSize;
                                            TEST_1_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            19,                 // u8               uStartupCycleCost;
                                            18                  // u8               uRealSingleCost;
                                        },
                                        {
                                            "out98",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_2_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12                  // u8               uRealSingleCost;
                                        },
                                        {
                                            "in98",             // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_3_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            YES,                // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12                  // u8               uRealSingleCost;
                                        },
                                        {
                                            "in98x",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_4_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            NO,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12                  // u8               uRealSingleCost;
                                        },
                                        {   // IN Regwrite test, WEAKNESS: two OUTs are used in the STARTUP, making the cost a tad inaccurate!
                                            "in99",             // u8*              szTestName;
                                            TEST_5_STARTUP,     // function*        pFncStartupBlock;
                                            8,                  // u8               uStartupBlockSize;
                                            TEST_5_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            40+2,               // u8               uStartupCycleCost;
                                            12                  // u8               uRealSingleCost;
                                        },
                                        {   // Palette test, must init first and restore palette after
                                            "out9A",            // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_6_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12                  // u8               uRealSingleCost;
                                        },
                                        {   // Stream port test. WEAKNESS: two OUTs are used in the STARTUP, making the cost a tad inaccurate!
                                            "out9B",            // u8*              szTestName;
                                            TEST_7_STARTUP,     // function*        pFncStartupBlock;
                                            8,                  // u8               uStartupBlockSize;
                                            TEST_7_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            40+2,               // u8               uStartupCycleCost;
                                            12                  // u8               uRealSingleCost;
                                        },
                                        {   // Just pick a non-used port (I hope), and check the speed
                                            "(ex) in06",        // u8*              szTestName;
                                            TEST_EMPTY,         // function*        pFncStartupBlock;
                                            0,                  // u8               uStartupBlockSize;
                                            TEST_8_UNROLL,      // void             pFncUnrollInstruction;
                                            2,                  // u8               uUnrollInstructionSize;
                                            NA,                 // enum three_way   eReadVRAM;
                                            0,                  // u8               uStartupCycleCost;
                                            12                  // u8               uRealSingleCost;
                                        }
                                     };

const u8                g_szErrorMSX[]      = "MSX2 and above is required";
const u8                g_szGreeting[]      = "VDP I/O Timing Test [z80] v1.2 - Test: %d repeats\r\n";
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

// Code ----------------------------------------------------------------------
// All bodies must be pure asm via __naked and without ret/return at the end.
//
void TEST_EMPTY() __naked
{
__asm
__endasm;
}

void TEST_0_UNROLL() __naked
{
__asm
    inc a
__endasm;
}

void TEST_1_STARTUP() __naked
{
__asm
    ld hl, #0x0000              ; // this one is just random
    ld c, #0x98                 ; // VDPIO
__endasm;
}
void TEST_1_UNROLL() __naked
{
__asm
    outi
__endasm;
}

void TEST_2_UNROLL() __naked
{
__asm
    out (0x98), a               ; // VDPIO. This will break the speed limits.
__endasm;
}

void TEST_3_UNROLL() __naked
{
__asm
    in a, (0x98)                ; // VDPIO. This will break the speed limits.
__endasm;
}

void TEST_4_UNROLL() __naked
{
__asm
    in a, (0x98)                 ; // VDPIO. This will break the speed limits.
__endasm;
}

void TEST_5_STARTUP() __naked
{
__asm
                                ; // NOTE: Somewhat unclear the cycle cost of this
    ld a, #3                    ; // get status for sreg n (https://www.msx.org/wiki/VDP_Status_Registers)
    out (0x99), a               ; // VDPPORT1. status register number
    ld a, #0x8F                 ; // VDP register R#15 (write)
    out (0x99), a               ; // VDPPORT1. out VDP register number
__endasm;
}

void TEST_5_UNROLL() __naked
{
__asm
    in a, (0x99)                ; // VDPPORT1. This will break the speed limits.
__endasm;
}

void TEST_6_UNROLL() __naked
{
__asm
    out (0x9A), a               ; // VDPPALETTE. This will break the speed limits.
__endasm;
}

void TEST_7_STARTUP() __naked   // Sets VDPSTREAM-port to constantly overwrite reg 32 (SX: X-coordinate to be transferred (LOW))
{
__asm
                                ; // NOTE: Somewhat unclear the cycle cost of this due to two outs
	ld    	a, #128 + 32		; // Set "Stream mode", but "non-autoincrement mode"
	out   	(0x99), a
	ld    	a, #128 + #17
	out   	(0x99), a    	    ; // R#17 := 32
__endasm;
}

void TEST_7_UNROLL() __naked
{
__asm
    out (0x9B), a               ; // VDPSTREAM. This will break the speed limits.
__endasm;
}

void TEST_8_UNROLL() __naked
{
__asm
    in a, (0x06)                ; // Assuming this one is not in use
__endasm;
}

// ---------------------------------------------------------------------------
void setCustomISR()
{
    disableInterrupt();
    g_bStorePCReg   = false;        // control if storing & stack mods should take place. MUST be reset
    g_pInterruptOrg = g_pInterrupt;
    g_pInterrupt    = &customISR;
    enableInterrupt();
}

// ---------------------------------------------------------------------------
void restoreOriginalISR()
{
    disableInterrupt();
    g_pInterrupt = g_pInterruptOrg;
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
void initTestInRamSetups()
{
    g_nTestStartBlockSize =  (u8*)&TEST_START_BLOCK_END - (u8*)&TEST_START_BLOCK_BEGIN;
    memcpy( &runTestAsmInHeap, &TEST_START_BLOCK_BEGIN, g_nTestStartBlockSize );
}

// ---------------------------------------------------------------------------
// Put test at runTestAsmInHeap, just after the common start block with unrolled
// instructions. First the startblock, and then x amount of unrolleds filling
// a PAL frame (+ a buffer: we set 75000 cycles as max cycles in a frame)
void setupTestInRam(u8 uTest)
{
    u8* p = (u8*) &runTestAsmInHeap;
    p += g_nTestStartBlockSize;
    
    memcpy(p, *g_aoTest[uTest].pFncStartupBlock, g_aoTest[uTest].uStartupBlockSize);

    p += g_aoTest[uTest].uStartupBlockSize;

    g_pCurTestBaseline = p;

    // Below: The fastest instr (5 cycles, 1 byte) would max give 0x3A98 bytes
    // Medium instr (8 cycles, 2 bytes) gives 0x493E bytes - we should be fine
    u16 nMax = 75000/g_aoTest[uTest].uRealSingleCost;
    for(u16 n = 0; n < nMax; n++)
    {
        memcpy(p, *g_aoTest[uTest].pFncUnrollInstruction, g_aoTest[uTest].uUnrollInstructionSize);
        p += g_aoTest[uTest].uUnrollInstructionSize;
    }

    *p = 0xC9; // add a "ret" at the end as security
}

// ---------------------------------------------------------------------------
void runIteration(enum freq_variant eFreq, u8 uTest, u8 uIterationNum)
{
    prepareVDP( g_aoTest[ uTest ].eReadVRAM );

    runTestAsmInHeap();

    u16 nLength = (u16)g_pPCReg - (u16)g_pCurTestBaseline; 
    u16 nInstructions = nLength/g_aoTest[ uTest ].uUnrollInstructionSize;

    g_anFrameInstrResult[eFreq][uTest][uIterationNum] = nInstructions;
}

// ---------------------------------------------------------------------------
void runAllIterations()
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

        for(u8 t = 0; t < arraysize(g_aoTest); t++)
        {
            setupTestInRam(t);

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
void calcStatistics()
{
    for(enum freq_variant f = 0; f < FREQ_COUNT; f++)
    {
        setPALRefreshRate((bool)f);

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
//
void printReport()
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

        s8 sDiff60Hz = (u8)(fTestCost60Hz + 0.5 ) - g_aoTest[t].uRealSingleCost;
        s8 sDiff50Hz = (u8)(fTestCost50Hz + 0.5 ) - g_aoTest[t].uRealSingleCost;

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
u8 main()
{
    if(getMSXType() == 0)
    {
        print(g_szErrorMSX);
        return 1;
    }
    
    sprintf(g_auBuffer, g_szGreeting, NUM_ITERATIONS);
    print(g_auBuffer);

    print(g_szWait);

    initPalette();      // just in case we test/trash the palette
 
    initTestInRamSetups();
    // changeMode(5);   // changing mode does not seem to matter at all, so we can just ignore for now
    runAllIterations();
    restorePalette();   // just in case the palette was messed up
    calcStatistics();
    // changeMode(0);
    printReport();

    return 0;
}