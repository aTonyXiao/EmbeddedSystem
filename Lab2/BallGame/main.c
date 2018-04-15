// Standard includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include "gpio.h"

// Common interface includes
#include "uart_if.h"
#include "i2c_if.h"
#include "test.h"
#include "spi.h"

#include "pin_mux_config.h"

//*****************************************************************************
//                      MACRO DEFINITIONS
//*****************************************************************************
#define APPLICATION_VERSION     "1.1.1"
#define APP_NAME                "I2C Demo"
#define UART_PRINT              Report
#define FOREVER                 1
#define CONSOLE                 UARTA0_BASE
#define FAILURE                 -1
#define SUCCESS                 0
#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}
#define SPI_IF_BIT_RATE  100000

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

//****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS                          
//****************************************************************************

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void)
{
    /* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    //
    // Set vector table base
    //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long) &g_pfnVectors[0]);
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

//*****************************************************************************
//
//! Main function handling the I2C example
//!
//! \param  None
//!
//! \return None
//! 
//*****************************************************************************

void startGame()
{
    int x, y;
    int xCoord = 64;
    int yCoord = 64;
    unsigned char offSetX = 0x03;
    unsigned char offSetY = 0x05;
    unsigned char xSpeed, ySpeed;

    int time2Win = 3;
    int i;

    fillScreen(BLACK);
    setTextColor(BLUE, BLACK);
    setTextSize(2);
    setCursor(5, 0);
    Outstr("Welcome!");
    setTextColor(WHITE, BLACK);
    setTextSize(1);
    setCursor(0, 40);
    Outstr("find red ball 3 times");
    setTextColor(WHITE, BLACK);
    setTextSize(1);
    setCursor(0, 60);
    Outstr("to win");
    delay(100);

    for (i = 0; i < time2Win; i++)
    {

        int ballPosX;
        int ballPosY;

        ballPosX = rand() % 123;
        ballPosY = rand() % 123;

        fillScreen(BLACK);
        fillCircle(ballPosX, ballPosY, 4, RED);
        fillScreen(BLACK);

        int timer = 0;

        while (1)
        {
            I2C_IF_Write(0x18, &offSetX, 1, 0);
            I2C_IF_Read(0x18, &xSpeed, 1);
            I2C_IF_Write(0x18, &offSetY, 1, 0);
            I2C_IF_Read(0x18, &ySpeed, 1);
            y = (int) ((signed char) xSpeed);       // - 128;
            x = (int) ((signed char) ySpeed);       // - 128;
            fillCircle(xCoord, yCoord, 2, BLACK);
            xCoord = xCoord + x * 0.1;
            if (xCoord >= 123)
            {
                xCoord = 123;
            }
            if (xCoord <= 4)
            {
                xCoord = 4;
            }
            yCoord = yCoord + y * 0.1;
            if (yCoord >= 123)
            {
                yCoord = 123;
            }
            if (yCoord <= 4)
            {
                yCoord = 4;
            }

            fillCircle(xCoord, yCoord, 2, BLUE);

            if ((abs(ballPosX - xCoord) <= 2) && (abs(ballPosY - yCoord) <= 2))
            {
                char* message = "0 Times to Win";
                message[0] = time2Win - i - 1 + '0';
                setCursor(0, 0);
                Outstr(message);
                delay(100);
                break;
            }

            if (timer > 200)
            {
                fillScreen(BLACK);
                setTextColor(BLUE, BLACK);
                setTextSize(2);
                setCursor(5, 0);
                Outstr("Time out!");
                setTextColor(RED, BLACK);
                setTextSize(1);
                setCursor(30, 40);
                Outstr("You Lost!");
                setTextColor(WHITE, BLACK);
                setTextSize(1);
                setCursor(30, 70);
                delay(100);
                return;
            }
            timer++;
        }
        timer = 0;
    }

    fillScreen(BLACK);
    setTextColor(BLUE, BLACK);
    setTextSize(2);
    setCursor(15, 0);
    Outstr("Congrats!");
    setTextColor(CYAN, BLACK);
    setTextSize(1);
    setCursor(30, 40);
    Outstr("You Win!");
    delay(100);
}

void main()
{
    // Initialize board configurations
    //
    BoardInit();
    //
    // Configure the pinmux settings for the peripherals exercised
    //
    PinMuxConfig();

    //
    // Configuring UART
    //
    InitTerm();

    //
    // I2C Init
    //
    I2C_IF_Open(I2C_MASTER_MODE_FST);

    MAP_SPIReset(GSPI_BASE);

    //
    // Configure SPI interface
    //
    MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI),
    SPI_IF_BIT_RATE,
                           SPI_MODE_MASTER, SPI_SUB_MODE_0, (SPI_SW_CTRL_CS |
                           SPI_4PIN_MODE |
                           SPI_TURBO_OFF |
                           SPI_CS_ACTIVEHIGH |
                           SPI_WL_8));

    //
    // Enable SPI for communication
    //
    MAP_SPIEnable(GSPI_BASE);

    Adafruit_Init();
    //
    fillScreen(BLACK);

    startGame();
    fillScreen(BLACK);
    setTextColor(WHITE, BLACK);
    setTextSize(2);
    setCursor(0, 20);
    Outstr("Press sw3");
    setTextColor(WHITE, BLACK);
    setTextSize(2);
    setCursor(0, 40);
    Outstr("try again");
    delay(100);

    GPIOPinWrite(GPIOA2_BASE, 0x40, 0);
    while (1)
    {
        long sw3 = GPIOPinRead(GPIOA1_BASE, 0x20) >> 5 & 0x1;
        if (sw3 == 1)
        {
            GPIOPinWrite(GPIOA1_BASE, 0x20, 0);
            startGame();
            fillScreen(BLACK);
            setTextColor(WHITE, BLACK);
            setTextSize(2);
            setCursor(0, 20);
            Outstr("Press sw3");
            setTextColor(WHITE, BLACK);
            setTextSize(2);
            setCursor(0, 40);
            Outstr("try again");
            delay(100);
        }
    }

}

//*****************************************************************************
//
// Close the Doxygen group.
//! @
//
//*****************************************************************************

