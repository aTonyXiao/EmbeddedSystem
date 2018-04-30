
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

// Common interface includes
#include "uart_if.h"

#include "pin_mux_config.h"


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
extern void (* const g_pfnVectors[])(void);

volatile unsigned long SW2_intcount;
volatile unsigned long SW3_intcount;
volatile unsigned char SW2_intflag;
volatile unsigned char SW3_intflag;

volatile unsigned long previous_time;
volatile unsigned long ZERO_INTERVAL = 106667;
volatile unsigned long encoded_value;
volatile unsigned long last_button;
volatile unsigned long ONE_BUTTON_TIMER_OUT = 80000*18;
volatile unsigned long end_length = 40 * 80000;
volatile unsigned long init_length = 11 * 80000;
volatile unsigned long current_time;

volatile unsigned long counter = 0;
volatile unsigned long timeSeries[35];

volatile unsigned long startTime = 13.512; // minimum of the start time
volatile unsigned long endTime = 40.52; // min
//volatile unsigned long oneInterval = 2.254; // min
//volatile unsigned long zeroInterval = 1.154; // min
volatile unsigned long middleValue = 1.704; // convert to tick

// frequency of clock per tick: 80MHZ = 12.5ns

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

// an example of how you can use structs to organize your pin settings for easier maintenance
typedef struct PinSetting {
    unsigned long port;
    unsigned int pin;
} PinSetting;


// pin 15
static PinSetting sig_input = { .port = GPIOA2_BASE, .pin = 0x40};

//*****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES                           
//*****************************************************************************
static void BoardInit(void);

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS                         
//*****************************************************************************


static void GPIOA2IntHandler(void) {    // SW2 handler
    timeSeries[counter] = Timer_IF_GetCount(TIMERA2_BASE, TIMER_A); //get current time
    count ++;
    if(timeSeries[1] - timeSeries[0] == startTime)
        counter = 0;
    ulStatus = MAP_GPIOIntStatus (sig_input.port, true);
    MAP_GPIOIntClear(sig_input.port, ulStatus);     // clear interrupts on GPIOA2
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
//****************************************************************************
//
//! Main function
//!
//! \param none
//! 
//!
//! \return None.
//
//****************************************************************************
int decodeSignal(unsigned long* timeSeries) {
    int i;
    int binary = 0;
    for(i = 2; i < 34; i++) {
        int tmp = (int)(timeSeries[i] - timeSeries[i - 1]);
        if(tmp < endTime)
            continue;
        else if(tmp < middleValue)
            binary << 1;
        else
            binary << 1;
            binary ++;
    }
    return binary;
}




char decodeMapping(int binary_val) {

    char c;

    switch(binary_val) {
        case 0x807f : // button 1
            c = '1';
            break;
        case 0x40BF : // button 2
            c = '2';
            break;
        case 0xC03F : // button 3
            c = '3';
            break;
        case 0x20DF :
            c = '4';
            break;
        case 0xA05F:
            c = '5';
            break;
        }

    return c;

}

int main() {
    unsigned long ulStatus;
    unsigned long timer_start = 100;
//    char [] message = '';
    char c;


    BoardInit();
    
    PinMuxConfig();
    
    InitTerm();

    ClearTerm();

    //
    // Register the interrupt handlers
    //
    MAP_GPIOIntRegister(sig_input.port, GPIOA2IntHandler);

    MAP_GPIOIntTypeSet(sig_input.port, sig_input.pin, GPIO_FALLING_EDGE);

    MAP_TimerConfigure(TIMERA2_BASE, TIMER_CFG_PERIODIC_UP);

    // counts up
//    TimerValueSet(TIMERA2_BASE, TIMER_A, 0);

    TimerEnable(TIMERA2_BASE, TIMER_A);

    while(1) {

       while(1){
           if(counter < 35) { // still reading signals from 1 button
               continue;
           }else{
               //TODO: disable interrupt
               counter = 0;
               int binary = decodeSignal(timeSeries);
               char button = decodeMapping(binary);
               printf("%c\n", button);
           }
       }

    }

//    ulStatus = MAP_GPIOIntStatus (GPIOA1_BASE, false);
//    MAP_GPIOIntClear(GPIOA1_BASE, ulStatus);          // clear interrupts on GPIOA1
//    ulStatus = MAP_GPIOIntStatus (switch2.port, false);
//    MAP_GPIOIntClear(switch2.port, ulStatus);         // clear interrupts on GPIOA2
//
//    // clear global variables
//    SW2_intcount=0;
//    SW3_intcount=0;
//    SW2_intflag=0;
//    SW3_intflag=0;

    // Enable SW2 and SW3 interrupts
//    MAP_GPIOIntEnable(GPIOA1_BASE, 0x20);
//    MAP_GPIOIntEnable(switch2.port, switch2.pin);
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
