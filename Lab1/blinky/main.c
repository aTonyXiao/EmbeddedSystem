//*****************************************************************************
// Tony Xiao, Kelly Su
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
#include "uart_if.h"

// Common interface includes
#include "gpio_if.h"

#include "pin_mux_config.h"

#define APPLICATION_VERSION     "1.1.1"

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************


//*****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//*****************************************************************************
void LEDBlinkyRoutine();
static void BoardInit(void);

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS
//*****************************************************************************

//*****************************************************************************
//
//! Configures the pins as GPIOs and peroidically toggles the lines
//!
//! \param None
//!
//! This function
//!    1. Configures 3 lines connected to LEDs as GPIO
//!    2. Sets up the GPIO pins as output
//!    3. Periodically toggles each LED one by one by toggling the GPIO line
//!
//! \return None
//
//*****************************************************************************
void LEDBlinkyRoutine()
{
    //
    // Toggle the lines initially to turn off the LEDs.
    // The values driven are as required by the LEDs on the LP.
    //

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
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    //
    // Set vector table base
    //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

// display the Banner message in the console
static void DisplayBanner(char * AppName)
{

    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t        CC3200 %s Application       \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t        Push SW3 to start LED binary counting       \n\r");
    Report("\t\t        Push SW2 to blink LEDS on and off       \n\r");
    Report("\t\t *************************************************\n\r");

}

//****************************************************************************
//
//! Main function
//!
//! \param none
//!
//! This function
//!    keep polling the sw2 and sw3 input
//!    count 000 - 111, set P18 to low when sw3 pressed
//!    blink LEDs in unison, set P18 to low when sw2 pressed
//! \return None.
//
//****************************************************************************

int
main()
{
    //
    // Initialize Board configurations
    //
    BoardInit();

    //
    // Power on the corresponding GPIO port B for 9,10,11.
    // Set up the GPIO lines to mode 0 (GPIO)
    //
    PinMuxConfig();

    InitTerm();
        //
        // Clearing the Terminal.
        //
    ClearTerm();

    DisplayBanner("GPIO"); // DisplayBanner, print to Banner

    GPIO_IF_LedConfigure(LED1|LED2|LED3);// config LEDs
    GPIO_IF_LedOff(MCU_ALL_LED_IND); // Turn off all LEDs

    int flag = 0;

    while(1){
        long sw3 = GPIOPinRead(GPIOA1_BASE, 0x20) >> 5 & 0x1; // read SW3
        long sw2 = GPIOPinRead(GPIOA2_BASE, 0x40) >> 6 & 0x1; // read SW2

        if(sw3 == 1 && flag != 1){ // if SW3 is Pressed && sw3 is not pressed continousely
            flag = 1;
            GPIOPinWrite(GPIOA3_BASE, 0x10, 0); // set P18 as low
            GPIOPinWrite(GPIOA2_BASE, 0x40, 0); // set SW2 as low(unpressed)

            while(1) {
                Report("SW3 Pressed\n\r"); // print to console
                int i;

                for(i = 0; i < 8; i++){ // count 000 to 111
                    int tmp = i;
                    if((tmp & 0x1) == 1) // check the first position of binary counter
                        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                    if((tmp & 0x2) == 2) // check the second position of binary counter
                        GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
                    if((tmp & 0x4) == 4) // check the third position of binary counter
                        GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
                    MAP_UtilsDelay(4000000); // disaply delay
                    GPIO_IF_LedOff(MCU_ALL_LED_IND); // turn off all leds
                    MAP_UtilsDelay(8000000); // disaply delay
                }
                sw2 = GPIOPinRead(GPIOA2_BASE, 0x40) >> 6 & 0x1; //read sw2
                if(sw2 == 1) // if pressed, escape the while loop
                    break;
            }
           }

        if(sw2 == 1 && flag != 2){ // if SW2 is Pressed && SW2 is not pressed continousely
            flag = 2;
<<<<<<< HEAD
            GPIOPinWrite(GPIOA1_BASE, 0x20, 0); // set sw3 as unpressed
            GPIOPinWrite(GPIOA3_BASE, 0x10, 1<<4); // set P18 as high
=======
            GPIOPinWrite(GPIOA1_BASE, 0x20, 0);
            GPIOPinWrite(GPIOA3_BASE, 0x10, 1<<4);
>>>>>>> origin/master
            while(1){
                Report("SW2 Pressed\n\r"); // print to console
                MAP_UtilsDelay(8000000); // disaply delay
                GPIO_IF_LedOn(MCU_ALL_LED_IND); // turn on all LED
                MAP_UtilsDelay(8000000); // disaply delay
                GPIO_IF_LedOff(MCU_ALL_LED_IND); // turn off all LED

                sw3 = GPIOPinRead(GPIOA1_BASE, 0x20) >> 5 & 0x1; // read SW3
                if(sw3 == 1) // if SW3 is pressed, escape the while loop
                    break;

            }
        }

    }
    return 0;
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
