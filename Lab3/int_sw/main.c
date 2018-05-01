
//*****************************************************************************
//
// Application Name     - int_sw
// Application Overview - The objective of this application is to demonstrate
//                          GPIO interrupts using SW2 and SW3.
//                          NOTE: the switches are not debounced!
//
//*****************************************************************************

//****************************************************************************
//
//! \addtogroup int_sw
//! @{
//
//****************************************************************************

// Standard includes
#include <stdio.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "interrupt.h"
#include "hw_apps_rcm.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
#include "gpio.h"
#include "utils.h"
#include "timer.h"
#include "timer_if.h"
// Common interface includes
#include "uart_if.h"

#include "pin_mux_config.h"


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
extern void (* const g_pfnVectors[])(void);

static volatile unsigned long g_ulBase;
static volatile unsigned long g_ulRefBase;

volatile unsigned long counter = 0;
volatile unsigned long timeSeries[35];
const unsigned long middleValue = 100000 - 50; // convert to tick
volatile int timeOut = 0;

// frequency of clock per tick: 80MHZ = 12.5ns

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

// an example of how you can use structs to organize your pin settings for easier maintenance
typedef struct PinSetting {
    unsigned long port;
    unsigned int pin;
} PinSetting;


// pin45 are using
static PinSetting sig_input = { .port = GPIOA3_BASE, .pin = 0x80};
//static PinSetting sig_input = { .port = GPIOA3_BASE, .pin = 0x10};

//*****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//*****************************************************************************
static void BoardInit(void);

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS
//*****************************************************************************


static void GPIOA2IntHandler(void) {    // SW2 handler
    unsigned long ulStatus;
    ulStatus = MAP_GPIOIntStatus (sig_input.port, true);
    MAP_GPIOIntClear(sig_input.port, ulStatus);     // clear interrupts on GPIOA2

    if(counter == 0){
        Timer_IF_Start(g_ulBase, TIMER_A, 500);
    }

    timeSeries[counter] = Timer_IF_GetCount(g_ulBase, TIMER_A); //get current time
    counter ++;
//    if(timeSeries[1] - timeSeries[0] == startTime)
//        counter = 0;

}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void) {
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
    
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

void TimerRefIntHandler(void)
{
    Timer_IF_InterruptClear(g_ulRefBase);
    timeOut = 1;
}


long decodeSignal() {
    int i;
    long binary = 0;
    for(i = 18; i < 34; i++) {
        int tmp = (int)(timeSeries[i] - timeSeries[i - 1]);
        if(tmp < middleValue)
            binary <<= 1;
        else {
            binary <<= 1;
            binary ++;
        }
    }
    return binary;
}




int decodeBinary(long binary_val) {
    int c = -1;
    switch(binary_val) {
        case 0x807f : // button 1
            c = 1;
            break;
        case 0x40BF : // button 2
            c = 2;
            break;
        case 0xC03F : // button 3
            c = 3;
            break;
        case 0x20DF :
            c = 4;
            break;
        case 0xA05F:
            c = 5;
            break;
        case 0x609F:
            c = 6;
            break;
        case 0xE01F:
            c = 7;
            break;
        case 0x10EF:
            c = 8;
            break;
        case 0x906F:
            c = 9;
            break;
        case 0xFF:
            c = 0;
            break;
        case 0x8F7:
            c = 11; // mute
            break;
        case 0x2FD:
            c = 12; // last
            break;
        default:
            c = -1;
            break;
        }
    return c;
}



char decodeChar(int button, int pressTime) {
    char c;
    switch(button) {
           case 2 : // button 2
               c = 'A' + (pressTime - 1) % 3;
               break;
           case 3 : // button 3
               c = 'D' + (pressTime - 1) % 3;
               break;
           case 4 :
               c = 'G' + (pressTime - 1) % 3;
               break;
           case 5:
               c = 'J' + (pressTime - 1) % 3;
               break;
           case 6:
               c = 'M' + (pressTime - 1) % 3;
               break;
           case 7:
               c = 'P' + (pressTime - 1) % 4;
               break;
           case 8:
               c = 'T' + (pressTime - 1) % 3;
               break;
           case 9:
               c = 'W' + (pressTime - 1) % 4;
               break;
           default:
               c = '0';
           }
    return c;
}

int main() {

    unsigned long ulStatus;

    int prev = -1;

    BoardInit();
    
    PinMuxConfig();
    
    InitTerm();

    ClearTerm();

    //
    // Register the interrupt handlers
    //
    MAP_GPIOIntRegister(sig_input.port, GPIOA2IntHandler);

    MAP_GPIOIntTypeSet(sig_input.port, sig_input.pin, GPIO_FALLING_EDGE);

    ulStatus = MAP_GPIOIntStatus (sig_input.port, false);
    MAP_GPIOIntClear(sig_input.port, ulStatus);
    MAP_GPIOIntEnable(sig_input.port, sig_input.pin);


    g_ulBase = TIMERA0_BASE;
    g_ulRefBase = TIMERA1_BASE;
    //
    // Base address for second timer
    //
    //
    // Configuring the timers
    //
    Timer_IF_Init(PRCM_TIMERA0, g_ulBase, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_Init(PRCM_TIMERA1, g_ulRefBase, TIMER_CFG_PERIODIC, TIMER_A, 0);

//    TimerEnable(TIMERA2_BASE, TIMER_A);

    while(1) {
        int pressTime = 0;

       while(1){
           if(counter < 35) { // still reading signals from 1 button
               continue;
           }else{
               MAP_GPIOIntDisable(sig_input.port, sig_input.pin); //disable interrupt
               Timer_IF_Stop(g_ulBase, TIMER_A); // disable timer
               counter = 0; // reset counter
               long binary = decodeSignal(); // decode signal to binary
               int button = decodeBinary(binary); // convert binary to button
               printf("Button %d Pressed\n", button);

               Timer_IF_DeInit(g_ulRefBase, TIMER_A);

               if(button == prev && !timeOut)
                   pressTime ++;
               else
                   pressTime = 1;

               prev = button;
               timeOut = 0;

               char letter = decodeChar(button, pressTime);
               printf("Char is %c\n", letter);


               ulStatus = MAP_GPIOIntStatus (sig_input.port, false);
               MAP_GPIOIntClear(sig_input.port, ulStatus);
               MAP_GPIOIntEnable(sig_input.port, sig_input.pin);
               Timer_IF_IntSetup(g_ulRefBase, TIMER_A, TimerRefIntHandler);
               Timer_IF_Start(g_ulRefBase, TIMER_A, 300);
           }
       }

    }
