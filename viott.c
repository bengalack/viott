// ---------------------------------------------------------------------------
// This program tries to figure out if I/O commands towards the VDP take
// longer time than expected various systems/engines.
// 
// It is using a custom ISR which does not do much other than storing the
// current address of the program pointer, at address: g_pPCReg
//
// Adding a test: See all places ADD_ENTRY is commented
//
// Assumptions:
//  * We start in DOS, hence 0x0038 already contains 0xC3 (jp)
//  * There are no line interrupts enabled
//  * PAL (“50 FPS”): 71364 cycles (3579545/50.159)
//  * NTSC (“60 FPS”): 59736 cycles (3579545/59.923)
//  * Default screen width is at least 29 chars (https://www.msx.org/wiki/MODE)
//
// Notes:
//  * There is no support for global initialisation of RAM variables in this config
//  * Likely some C programming shortcomings - I don't have much experience with C
//  * SORRY! for the Hungarian notation, but is helps me when mixing asm and c
//  * Others:
//      Prefixes:
//      * u  = unsigned char  (u8)
//      * n  = unsigned short (u16)
//      * l  = unsigned long  (u32)
//      * f  = float
//      * p  = pointer
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
#include <stdbool.h>

// Typedefs & defines --------------------------------------------------------
//
#define NUM_ITERATIONS      128 // similar to VATT iterations
// #define NUM_ITERATIONS      16

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned long       u32;

typedef const void          callable( void );

#define halt()				{ __asm halt; __endasm; }
#define enableInterrupt()	{ __asm ei;   __endasm; }
#define disableInterrupt()	{ __asm di;   __endasm; }
#define break()				{ __asm in a,(0x2e);__endasm; } // for debugging. may be risky to use as it trashes A

enum test_variant { TEST_OUTI, TEST_OUT, TEST_IN, TEST_INX, TEST_COUNT }; // ADD_ENTRY
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

// tests
void testRunOUTI(); // ADD_ENTRY
void testRunOUT(); // ADD_ENTRY
void testRunIN(); // ADD_ENTRY

extern void*            testRunOUTIBaseline; // ADD_ENTRY
extern void*            testRunOUTBaseline; // ADD_ENTRY
extern void*            testRunINBaseline; // ADD_ENTRY

// Consts / ROM friendly -----------------------------------------------------
//
const u8                g_szErrorMSX[]      =  "MSX2 and above is required";
const u8                g_szGreeting[]      =  "VDP I/O Timing Test z80/v1.1\r\n";
const u8                g_szWait[]          =  "...please wait 30 secs or so";
const u8                g_szRemoveWait[]    =  "\r                            \r";
const u8                g_szReportHdr[]     =  "Report: %d repeats\n\r";
const u8                g_szReportSubFreq[] =  "Freq:%s\r\n";
const u8                g_szReportSubTest[] =  "Test:%s\r\n";
const u8                g_szReportCols[]    =  "     avg     min  max  cost\r\n";
const u8                g_szReportValues[]  =  "% 4s %04hu.%02d %04hu %04hu %d.%02d\r\n";
const u8                g_szNewline[]       =  "\r\n";

const u8* const         g_aszFreqNames[]    = {"NTSC-60Hz", "PAL-50Hz"};
const callable* const   g_apTestFunction[]  = {&testRunOUTI, &testRunOUT, &testRunIN, &testRunIN}; // ADD_ENTRY
const void* const       g_apTestBaseline[]  = {&testRunOUTIBaseline, &testRunOUTBaseline, &testRunINBaseline, &testRunINBaseline}; // ADD_ENTRY
const bool              g_abTestRead[]      = {false, false, true, false}; // ADD_ENTRY
const u8* const         g_aszTestNames[]    = {"outi", "out", "in", "inx"}; // ADD_ENTRY
const float             g_afFrmTotalCycles[]= {59736.0, 71364.0};

const u16               FRAME_CYCLES_INT            = 215;
const u16               FRAME_CYCLES_INT_KICK_OFF   = 14 + 18; 

// RAM variables -------------------------------------------------------------
//
void* __at(0x0039)      g_pInterrupt;       // We assume that 0x0038 already holds 0xC3 (JP) in dos mode at startup
void*                   g_pInterruptOrg;
u8                      g_auBuffer[ 256 ];  // temp/general buffer here to avoid stack explosion

volatile u8*            g_pPCReg;           // pointer to PC-reg when the interrupt was triggered
volatile bool           g_bToggle;          // 
volatile bool           g_bStorePCReg;      // 

u16                     g_anFrameInstrResult[ FREQ_COUNT ][ TEST_COUNT ][ NUM_ITERATIONS ];
float                   g_afFrameInstrResultAvg[ FREQ_COUNT ][ TEST_COUNT ];
u16                     g_anFrameInstrResultMin[ FREQ_COUNT ][ TEST_COUNT ];
u16                     g_anFrameInstrResultMax[ FREQ_COUNT ][ TEST_COUNT ];

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
void prepareVDP( bool bRead )
{
    disableInterrupt(); // generates "info 218: z80instructionSize() failed to parse line node, assuming 999 bytes" - dunno why, SDCC funkiness

    if( bRead )
        setVRAMAddressNI(1|0x40, 0x0000);
    else
        setVRAMAddressNI(1|0x00, 0x0000);

    enableInterrupt();
}

// ---------------------------------------------------------------------------
void runIteration(enum freq_variant eFreq, enum test_variant eTest, u8 uIterationNum )
{
    prepareVDP( g_abTestRead[ eTest ] );

    g_apTestFunction[ eTest ]();

    u16 nLength = (u16)g_pPCReg - (u16)(g_apTestBaseline[eTest]); 
    u16 nOUTIs = nLength/2;

    g_anFrameInstrResult[eFreq][eTest][uIterationNum] = nOUTIs;
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

    for(enum freq_variant f=0; f<FREQ_COUNT; f++)
    {
        setPALRefreshRate((bool)f);

        for(enum test_variant t=0; t<TEST_COUNT; t++)
            for(u8 i=0; i<NUM_ITERATIONS; i++)
                runIteration(f, t, i);

    }

    restoreOriginalISR();
    setPALRefreshRate(bPALOrg);

    if(uType == 3)              // restore orginal CPU
        changeCPU(uCPUOrg);
}

// ---------------------------------------------------------------------------
void calcStatistics()
{
    for(enum freq_variant f=0; f<FREQ_COUNT; f++)
    {
        setPALRefreshRate((bool)f);

        for(enum test_variant t=0; t<TEST_COUNT; t++)
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
}

// ---------------------------------------------------------------------------
//
void printReport()
{
    print(g_szRemoveWait);

    sprintf(g_auBuffer, g_szReportHdr, NUM_ITERATIONS);
    print(g_auBuffer);


    for(enum freq_variant f=0; f<FREQ_COUNT; f++)
    {
        float fTotalCyclesPrFrm = g_afFrmTotalCycles[f] - FRAME_CYCLES_INT - FRAME_CYCLES_INT_KICK_OFF;

        print(g_szNewline);
        sprintf(g_auBuffer, g_szReportSubFreq, g_aszFreqNames[f]);
        print(g_auBuffer);
        print(g_szReportCols);

        for(enum test_variant t=0; t<TEST_COUNT; t++)
        {
            // because SDCC does not come out of the box with support for %f
            // (or %.2f in our case), we manually split it up in two %d
            // float adder (0.005) is as a "round(val, 2)" when we truncate using ints
            float fAvg = g_afFrameInstrResultAvg[f][t] + 0.005;
            float fAvgFr = fAvg - (u16)fAvg;
            u16 nInteger1 = (u16)fAvg;
            u8 uFraction1 = (u8)(fAvgFr*100);

            float fTestCost = (fTotalCyclesPrFrm / g_afFrameInstrResultAvg[f][t]) + 0.005;
            float fTestCostFr = fTestCost - (u8)fTestCost;
            u16 nInteger2 = (u16)fTestCost;
            u8 uFraction2 = (u8)(fTestCostFr*100);

            sprintf(g_auBuffer, g_szReportValues, g_aszTestNames[t], nInteger1, uFraction1, g_anFrameInstrResultMin[f][t], g_anFrameInstrResultMax[f][t], nInteger2, uFraction2);
            print(g_auBuffer);
        }
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
    
    print(g_szGreeting);
    print(g_szWait);

    // changeMode(5);   // changing mode does not seem to matter at all, so we can just ignore for now
    runAllIterations();
    calcStatistics();
    // changeMode(0);
    printReport();

    return 0;
}