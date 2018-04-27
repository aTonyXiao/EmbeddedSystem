
//*****************************************************************************
//
// Application Name     - int_sw
// Application Overview - The objective of this application is to demonstrate
//							GPIO interrupts using SW2 and SW3.
//							NOTE: the switches are not debounced!
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
static void GPIOA1IntHandler(void) { // SW3 handler
    unsigned long ulStatus;

    ulStatus = MAP_GPIOIntStatus (GPIOA1_BASE, true);
    MAP_GPIOIntClear(GPIOA1_BASE, ulStatus);		// clear interrupts on GPIOA1
    SW3_intcount++;
    SW3_intflag=1;
}



static void GPIOA2IntHandler(void) {	// SW2 handler
	unsigned long ulStatus;
	unsigned long current_time;
	unsigned long diff_time;

    ulStatus = MAP_GPIOIntStatus (sig_input.port, true);
    MAP_GPIOIntClear(sig_input.port, ulStatus);		// clear interrupts on GPIOA2
    current_time = TimerValueGet(TIMERA2_BASE, TIMER_A);
    diff_time = current_time - previous_time;
    previous_time = current_time;

    TimerValueSet(TIMERA2_BASE, TIMER_A);

    if (diff_time >= end_length)
        last_button = encoded_value;
        encoded_value = 0;
        previous_time = 0;
    else if (diff_time >= init_length)
        return;
    else if (diff_time >= ZERO_INTERVAL)
        (encoded_value << 1)++;
    else
        encoded_value << 1;
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

char decodeMapping(unsigned long binary_val) {

    char c;

    switch(binary_val) {
        case 0000 0010 1111 1101 1000 0000 0111 1111: // button 1
            c = '1';
            break;
        case 0000 0010 1111 1101 0100 0000 1011 1111: // button 2
            c = '2';
            break;
        case 0000 0010 1111 1101 1100 0000 0011 1111: // button 3
            c = '3';
            break;
        case 0000 0010 1111 1101 0010 0000 1101 1111:
            c = '4';
            break;
        case 0000 0010 1111 1101 1010 0000 0101 1111:
            c = '5';
            break;
        }

    return c;

}

int main() {
	unsigned long ulStatus;
	unsigned long timer_start 100;
	char [] message = '';
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
    TimerValueSet(TIMERA2_BASE, TIMER_A, timer_start);

    TimerEnable(TIMERA2_BASE, TIMER_A);

    while(1) {

        // one button time
        while(1){

            if (current_time < ONE_BUTTON_TIMER_OUT) {
                // disable interrupts
                break;
            }
        }

        c = decodeMapping(encoded_value);
        printf("%c\n", c);

    }

//    ulStatus = MAP_GPIOIntStatus (GPIOA1_BASE, false);
//    MAP_GPIOIntClear(GPIOA1_BASE, ulStatus);			// clear interrupts on GPIOA1
//    ulStatus = MAP_GPIOIntStatus (switch2.port, false);
//    MAP_GPIOIntClear(switch2.port, ulStatus);			// clear interrupts on GPIOA2
//
//    // clear global variables
//    SW2_intcount=0;
//    SW3_intcount=0;
//    SW2_intflag=0;
//    SW3_intflag=0;

    // Enable SW2 and SW3 interrupts
//    MAP_GPIOIntEnable(GPIOA1_BASE, 0x20);
//    MAP_GPIOIntEnable(switch2.port, switch2.pin);
//
//
//    Message("\t\t****************************************************\n\r");
//    Message("\t\t\tPush SW3 or SW2 to generate an interrupt\n\r");
//    Message("\t\t ****************************************************\n\r");
//    Message("\n\n\n\r");
//	Report("SW2 ints = %d\tSW3 ints = %d\r\n",SW2_intcount,SW3_intcount);
//
//    while (1) {
//    	while ((SW2_intflag==0) && (SW3_intflag==0)) {;}
//    	if (SW2_intflag) {
//    		SW2_intflag=0;	// clear flag
//    		Report("SW2 ints = %d\tSW3 ints = %d\r\n",SW2_intcount,SW3_intcount);
//    	}
//    	if (SW3_intflag) {
//    		SW3_intflag=0;	// clear flag
//    		Report("SW2 ints = %d\tSW3 ints = %d\r\n",SW2_intcount,SW3_intcount);
//    	}
//    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
