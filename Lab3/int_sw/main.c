
//****************************************************************************

// Standard includes
#include <stdio.h>
#include <string.h>
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
#include "test.h"
#include "spi.h"
#include "udma.h"
#include "hw_uart.h"
#include "pin_mux_config.h"
#include "uart.h"

#define SPI_IF_BIT_RATE  100000
//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
extern void (* const g_pfnVectors[])(void);

static volatile unsigned long g_ulBase;
static volatile unsigned long g_ulRefBase;

volatile unsigned long counter = 0;
volatile unsigned long timeSeries[35];
const unsigned long middleValue = 100000; // middle value of 0 and 1 width
volatile unsigned long prev = 0;
volatile unsigned long current = 0;
volatile int startFlag = 0;
volatile unsigned long buttonEndTime = 0;
volatile unsigned long buttonStartTime = 0;
volatile int position;
volatile int buttom_x, buttom_y;
char buffer[60] = "\0";
volatile static tBoolean bRxDone;

// frequency of clock per tick: 80MHZ = 12.5ns

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

typedef struct PinSetting
{
    unsigned long port;
    unsigned int pin;
} PinSetting;

// pin45 are using
static PinSetting sig_input = { .port = GPIOA3_BASE, .pin = 0x80 };
//static PinSetting sig_input = { .port = GPIOA3_BASE, .pin = 0x10};

//*****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//*****************************************************************************
static void BoardInit(void);

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS
//*****************************************************************************

static void GPIOA2IntHandler(void)
{    // SW2 handler
    unsigned long ulStatus;
    ulStatus = MAP_GPIOIntStatus(sig_input.port, true);
    MAP_GPIOIntClear(sig_input.port, ulStatus);    // clear interrupts on GPIOA2

    if (counter == 0)
    {
        Timer_IF_Start(g_ulBase, TIMER_A, 500);
        counter = 2;
    }

    current = Timer_IF_GetCount(g_ulBase, TIMER_A);

    long diff = current - prev;

    if (abs(diff - 1077325) < 77325) // detect the starting signal
    {
        timeSeries[0] = prev;
        counter = 1;
        startFlag = 1; // start
        buttonStartTime = Timer_IF_GetCount(g_ulRefBase, TIMER_A); // record the start time
        Timer_IF_Stop(g_ulRefBase, TIMER_A);
    }

    prev = current;
    if (startFlag == 1 && diff > 50000) // start and the time frame is correct
    {
        timeSeries[counter] = current; // store current time in time frame
        counter++; // read up to 35 time frame
    }

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
static void BoardInit(void)
{
    MAP_IntVTableBaseSet((unsigned long) &g_pfnVectors[0]);

    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

long decodeSignal() // decode time frame to 0s and 1s
{
    int i;
    long binary = 0;
    long duration[36];

    for (i = 1; i < 36; i++)
    {
        duration[i - 1] = timeSeries[i] - timeSeries[i - 1];
    }

    for (i = 17; i < 33; i++) // only need get 16 bits
    {
        if (duration[i] < middleValue) // zero
            binary <<= 1;
        else // one
        {
            binary <<= 1;
            binary++;
        }
    }

    return binary;
}

int decodeBinary(long binary_val) // convert 0s and 1s into button
{
    int c = -1;
    switch (binary_val)
    {
    case 0x807f: // button 1
        c = 1;
        break;
    case 0x40BF: // button 2
        c = 2;
        break;
    case 0xC03F: // button 3
        c = 3;
        break;
    case 0x20DF:
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
    default: // detect wrong singal
        c = -1;
        break;
    }
    return c;
}

char decodeChar(int button, int pressTime) // decode button into char according to pressTime
{
    char c;
    switch (button)
    {
    case 2: // button 2
        c = 'A' + (pressTime - 1) % 3;
        break;
    case 3: // button 3
        c = 'D' + (pressTime - 1) % 3;
        break;
    case 4:
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
    case 0:
        c = ' ';
        break;
    default:
        c = '0';
    }
    return c;
}

void oledInit()
{
    // Reset SPI
    //
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
    setCursor(0, 0);
    buttom_x = 0;
    buttom_y = 64;
    position = 0;
}

void printChar(char ch, short position) //print char to OLED
{
    int textsize = 1;

    if (position < 1) // delete the previous one
        fillRect(cursor_x, cursor_y, 6, 8, BLACK);

    cursor_x += 6 * position; // move cursor to next char position

    if (cursor_x < 0)
    {
        if (cursor_y != 0)
        {
          cursor_x = 121;
          cursor_y -= 8;
        }
        else
            cursor_x = 0;
    }
    drawChar(cursor_x, cursor_y, ch, 0xFFFF, 0xFFFF, textsize); // draw the char
    if (cursor_x > 124) // change line
    {
        cursor_x = 0;
        cursor_y += 8;
    }
}

void printCharButtom(char ch) // draw char on the buttom part of screen when receive char from UART
{
    int textsize = 1;
    buttom_x += 6;
    if (buttom_x < 0)
    {
        if (buttom_y != 0)
        {
          buttom_x = 121;
          buttom_y -= 8;

        }
        else
            buttom_x = 0;
    }
    drawChar(buttom_x, buttom_y, ch, 0xFFFF, 0xFFFF, textsize);
    if (buttom_x > 124) // change line
    {
        buttom_x = 0;
        buttom_y += 8;
    }
}

static void UARTIntHandler()
{
   if (!bRxDone)
   {
       MAP_UARTDMADisable(UARTA0_BASE, UART_DMA_RX);
       bRxDone = true;
   }
   else
       MAP_UARTDMADisable(UARTA0_BASE, UART_DMA_TX); // disable DMA

   MAP_UARTIntClear(UARTA0_BASE, UART_INT_DMATX | UART_INT_DMARX);

    char tmp = UARTCharGet(UARTA1_BASE); // get char through uart
    printCharButtom(tmp); // print char
}

void oledTest()
{
    lcdTestPattern();
    fillScreen(BLACK);
}

int main()
{
    unsigned long ulStatus;

    int prev = -1;

    BoardInit();

    PinMuxConfig();

    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);

    InitTerm();

    ClearTerm();
    MAP_PRCMPeripheralReset(PRCM_GSPI);
    oledInit();
    oledTest();

    MAP_UARTConfigSetExpClk(UARTA1_BASE,
    MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                            UART_BAUD_RATE,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                            UART_CONFIG_PAR_NONE));

    MAP_UARTIntRegister(UARTA1_BASE, UARTIntHandler);
    MAP_UARTIntEnable(UARTA1_BASE, UART_INT_RX);

   MAP_UARTIntEnable(UARTA1_BASE, UART_INT_DMARX);

   unsigned long uartStatus = UARTIntStatus(UARTA1_BASE, false);
   UARTIntClear(UARTA1_BASE, uartStatus);

   MAP_UARTDMAEnable(UARTA1_BASE, UART_DMA_TX);

    //
    // Register the interrupt handlers
    //
    MAP_GPIOIntRegister(sig_input.port, GPIOA2IntHandler);

    MAP_GPIOIntTypeSet(sig_input.port, sig_input.pin, GPIO_FALLING_EDGE);

    ulStatus = MAP_GPIOIntStatus(sig_input.port, false);
    MAP_GPIOIntClear(sig_input.port, ulStatus);
    MAP_GPIOIntEnable(sig_input.port, sig_input.pin);

    g_ulBase = TIMERA0_BASE;
    g_ulRefBase = TIMERA1_BASE;
    //
    //
    // Configuring the timers
    //
    Timer_IF_Init(PRCM_TIMERA0, g_ulBase, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_Init(PRCM_TIMERA1, g_ulRefBase, TIMER_CFG_PERIODIC, TIMER_A, 0);

    while (1)
    {
        int pressTime = 0;

        while (1)
        {
            if (counter < 35)
            { // still reading signals from 1 button
                continue;
            }
            else
            {
                MAP_GPIOIntDisable(sig_input.port, sig_input.pin); //disable interrupt
                Timer_IF_Stop(g_ulBase, TIMER_A); // disable timer
                counter = 0; // reset counter
                startFlag = 0; // clear the start flag
                long binary = decodeSignal(); // decode signal to binary
                int button = decodeBinary(binary); // convert binary to button
                // printf("Button %d Pressed\n", button);

                long interval = buttonStartTime - buttonEndTime;
                if (button != -1)
                {
                    if (button == 11) // mute(enter)
                    {
                        int i;
                        for (i = 0; i < strlen(buffer); i++) // sent all char in buffer through UART
                        {
                            UARTCharPut(UARTA1_BASE, buffer[i]);
                        }
                        fillRect(0, 0, 127, 64, BLACK); // clean screen
                        setCursor(0, 0); // reset cursor
                        buffer[0] = '\0'; // clean buffer
                    }
                    else if (button == 12) // last(del)
                    {
                        position = -1;
                        printChar(button + '0', position); // delete prev char
                        buffer[strlen(buffer) - 1] = '\0'; // buffer remove the prev one
                    }
                    else // 0 - 9 been pressed
                    {
                        if (button == prev && interval < 30000000) //same button pressed twice or more
                        {
                            pressTime++;
                            position = 0;
                        }
                        else // different button or second timer timeout
                        {
                            pressTime = 1;
                            position = 1;
                        }

                        prev = button;

                        char letter = decodeChar(button, pressTime); // decode button
                        // printf("Char is %c\n", letter);

                        printChar(letter, position); //print to screen

                        if (position > 0) // add to buffer
                        {
                            strcat(buffer, &letter);
                        }
                        else // replace the last char in the buffer with new char
                        {
                            buffer[strlen(buffer) - 1] = letter;
                            buffer[strlen(buffer)] = '\0';
                        }
                    }
                }

                ulStatus = MAP_GPIOIntStatus(sig_input.port, false);
                MAP_GPIOIntClear(sig_input.port, ulStatus); //clear interrupts
                MAP_GPIOIntEnable(sig_input.port, sig_input.pin); // enable interrupts
                Timer_IF_Start(g_ulRefBase, TIMER_A, 1000); // start 2nd timer
                buttonEndTime = Timer_IF_GetCount(g_ulRefBase, TIMER_A); // recode the current time

            }
        }

    }
}
