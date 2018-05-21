// Standard includes
#include <string.h>

// Driverlib includes
#include "gpio.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "spi.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"
#include "timer.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "rom.h"
#include "rom_map.h"

// Common interface includes
#include "uart_if.h"
#include "pin_mux_config.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "test.h"

#define SPI_IF_BIT_RATE  400000
#define TR_BUFF_SIZE     100

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

volatile static tBoolean bRxDone;
long int power_all[7];
unsigned char flag;
unsigned char startProcess;

unsigned short sample_num;
unsigned short sample_count;
signed long sample_buffer[400];
int new_digit = 0;
int num;
int bottom_x, bottom_y = 61;
long int coeff_array[7] = {31548, 31281, 30951, 30556, 29144, 28361, 27409};

char textbuffer[20];
unsigned char bufferLength;

char *buttonPtr="\0";
char *printChar="\0";

unsigned char isNewChar;

int top_x = 0;

int prevChar = -1;

#define LOWER_BOUND  20000
#define HIGHER_BOUND  40000

const char button0[]="0 ";
const char button1[]="1";
const char button2[]="ABC";
const char button3[]="DEF";
const char button4[]="GHI";
const char button5[]="JKL";
const char button6[]="MNO";
const char button7[]="PQRS";
const char button8[]="TUV";
const char button9[]="WXYZ";

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************


//*****************************************************************************
//! Board Initialization & Configuration
//*****************************************************************************
static void
BoardInit(void)
{
#ifndef USE_TIRTOS
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

//*****************************************************************************
//
//! Main function for spi demo application
//!
//! \param none
//!
//! \return None.
//
//*****************************************************************************
void printCharbottom(char ch)
{
    drawChar(bottom_x, bottom_y, ch, WHITE, WHITE, 1);
    bottom_x += 6;
    if (bottom_x > 123) {
        bottom_y += 8;
        bottom_x = 0;

    }
    if (bottom_y > 123) {
        fillRect(bottom_x, bottom_y, 128, 63, BLACK);
        bottom_y = 63;
        bottom_x = 0;
    }
}

unsigned short
readADC(void)
{
    unsigned char data[2];
    GPIOPinWrite(GPIOA0_BASE, 0x10, 0x00); // CS
    MAP_SPITransfer(GSPI_BASE, 0, data, 0x2, SPI_CS_ENABLE|SPI_CS_DISABLE); // SPI
    GPIOPinWrite(GPIOA0_BASE, 0x10, 0x10); // CS
    return (data[0] << 5) | ((0xf8 & data[1]) >> 3);
}

// Timer A0
// Used to control sampling
static void
TimerA0IntHandler(void)
{
    unsigned long ulStatus;
    ulStatus = MAP_TimerIntStatus(TIMERA0_BASE, true);
    MAP_TimerIntClear(TIMERA0_BASE, ulStatus);

    flag = 1;
    sample_num++;

    if (sample_num == 400)
        startProcess = 1;
}

// Timer A1
// Used for inter-button
static void
TimerA1IntHandler(void)
{
    unsigned long ulStatus;
    ulStatus = MAP_TimerIntStatus(TIMERA1_BASE, true);
    MAP_TimerIntClear(TIMERA1_BASE, ulStatus);
    isNewChar = 1;
}

static void UARTA1IntHandler(void)
{
    if(bRxDone) {
        MAP_UARTDMADisable(UARTA0_BASE, UART_DMA_TX);
    }else {
        MAP_UARTDMADisable(UARTA0_BASE, UART_DMA_RX);
        bRxDone = true;
    }

    MAP_UARTIntClear(UARTA0_BASE, UART_INT_DMATX|UART_INT_DMARX);
    char tmp = UARTCharGet(UARTA1_BASE);
    printCharbottom(tmp);
}

long goertzel(long coeff)
{
    //initialize variables to be used in the function
    int Q, Q_prev, Q_prev2,i;
    long prod1,prod2,prod3,power;

    Q_prev = 0;         //set delay element1 Q_prev as zero
    Q_prev2 = 0;        //set delay element2 Q_prev2 as zero
    power=0;            //set power as zero

    for (i=0; i<400; i++) // loop 400 times and calculate Q, Q_prev, Q_prev2 at each iteration
        {
            Q = (sample_buffer[i]) + ((coeff* Q_prev)>>14) - (Q_prev2); // >>14 used as the coeff was used in Q15 format
            Q_prev2 = Q_prev;                                    // shuffle delay elements
            Q_prev = Q;
        }

    //calculate the three products used to calculate power
    prod1=((long) Q_prev*Q_prev);
    prod2=((long) Q_prev2*Q_prev2);
    prod3=((long) Q_prev *coeff)>>14;
    prod3=(prod3 * Q_prev2);

    power = ((prod1+prod2-prod3))>>8; //calculate power using the three products and scale the result down

    return power;
}

signed char post_test(void) // post_test() function from the Github example
{
    //init variables
    int col_max = 0, row_max = 0, i, row, col;

    // find the maximum in the column and row frequencies
    for( i = 0; i < 7; i++){
      if(i < 4){
        if (power_all[i] > row_max) {
            row_max = power_all[i];
            row = i;
        }
      }else{
        if (power_all[i] > col_max) {
            col_max = power_all[i];
            col = i;
        }
      }
    }

    if(power_all[col] <= HIGHER_BOUND && power_all[row] <= LOWER_BOUND) //set a threshold to avoid noise in the lab
        new_digit = 1;

    if(power_all[row] > HIGHER_BOUND && power_all[col] > HIGHER_BOUND && (new_digit == 1)) { // check maximum powers of row & column exceed threshold
        new_digit = 0;
        int res = 3 * row + col - 4 + 1;

        if(res == prevChar)
          sample_count ++; // if match the prev button increase the confidence number
        prevChar = res;

        return res;
    }


    return -1;
}

int main() {
    unsigned long ulStatus;
    unsigned long ulStatus;
    char new = -1;
    char old = -1;
    int i;

    bRxDone = false;
    flag = 0;
    isNewChar = 0;
    startProcess = 0;
    sample_num = 0;
    // Initializations
    BoardInit();
    PinMuxConfig();
    //SPI INIT
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_GSPI);
    MAP_SPIReset(GSPI_BASE);
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                           SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                           (SPI_SW_CTRL_CS |
                           SPI_4PIN_MODE |
                           SPI_TURBO_OFF |
                           SPI_CS_ACTIVEHIGH |
                           SPI_WL_8));
    MAP_SPIEnable(GSPI_BASE);

    // UART set up
    MAP_UARTConfigSetExpClk(UARTA1_BASE,MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                          115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                           UART_CONFIG_PAR_NONE));
    MAP_UARTIntRegister(UARTA1_BASE,UARTA1IntHandler);

    Adafruit_Init();
//    lcdTestPattern();
//    lcdTestPattern2();
    fillScreen(BLACK);

    // Init Timer
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA1, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_TIMERA1);
    MAP_TimerConfigure(TIMERA1_BASE, TIMER_CFG_ONE_SHOT);
    MAP_TimerLoadSet(TIMERA1_BASE, TIMER_A, 80000000);
    MAP_TimerIntRegister(TIMERA1_BASE, TIMER_A, TimerA1IntHandler);
    ulStatus = MAP_TimerIntStatus(TIMERA1_BASE, false);
    MAP_TimerIntClear(TIMERA1_BASE, ulStatus);

    // Init Timer
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_TIMERA0);
    MAP_TimerConfigure(TIMERA0_BASE, TIMER_CFG_PERIODIC);
    MAP_TimerLoadSet(TIMERA0_BASE, TIMER_A, 5000);
    MAP_TimerIntRegister(TIMERA0_BASE, TIMER_A, TimerA0IntHandler);
    ulStatus = MAP_TimerIntStatus(TIMERA0_BASE, false);
    MAP_TimerIntClear(TIMERA0_BASE, ulStatus);

    ulStatus = MAP_UARTIntStatus(TIMERA1_BASE, true);
    MAP_UARTIntClear(TIMERA1_BASE, ulStatus);

    MAP_TimerIntEnable(TIMERA0_BASE, TIMER_TIMA_TIMEOUT);
    MAP_TimerEnable(TIMERA0_BASE, TIMER_A);

    MAP_UARTIntEnable(UARTA1_BASE,UART_INT_RX|UART_INT_RT);
    MAP_UARTEnable(UARTA1_BASE);

    while(1) {
        if (flag == 1) {
            long tmp = ((signed long) readADC()) - 372;
            sample_buffer[sample_num-1] = tmp;
            flag = 0;
        }
        if (startProcess == 1) {
            startProcess = 0;
            sample_num = 0;
            // disable timer
            MAP_TimerIntDisable(TIMERA0_BASE, TIMER_TIMA_TIMEOUT);
            MAP_TimerDisable(TIMERA0_BASE, TIMER_A);
            ulStatus = MAP_TimerIntStatus(TIMERA0_BASE, false);
            MAP_TimerIntClear(TIMERA0_BASE, ulStatus);

            for (i = 7; i >= 0; i--)
                power_all[i] = goertzel(coeff_array[i]); // call goertzel

            new = post_test();

            if(sample_count > 4) {//collect 4 sample with good confidence
              sample_count = 0;
            if (new != -1) {
                MAP_TimerDisable(TIMERA1_BASE, TIMER_A);
                MAP_TimerIntDisable(TIMERA1_BASE, TIMER_TIMA_TIMEOUT);
                // disable
                int control = 0;
                if(new == 10){ // MUTE

                  int i;

                  for(i = 0; i < strlen(textbuffer); i++) {
                      MAP_UARTCharPut(UARTA1_BASE, textbuffer[i]);
                  }
                  int len = strlen(textbuffer);
                  for(i = 0; i <len; i++) {
                    textbuffer[i] = '\0';
                  }
                  // clear the screen
                  fillRect(0, 0, 200, 8, BLACK);
                  top_x = 0;

                }else if(new == 12){ //LAST
                  if (bufferLength > 0) {
                      textbuffer[--bufferLength] = '\0';
                      fillRect(top_x, 0, 5, 8, BLACK);
                      top_x -= 6;
                      if (top_x < 0)
                          top_x = 0;
                  }
                }

                switch (new) {
                    case 1:
                      buttonPtr = button1;
                      break;
                    case 2:
                      buttonPtr = button2;
                      break;
                    case 3:
                      buttonPtr = button3;
                      break;
                    case 4:
                      buttonPtr = button4;
                      break;
                    case 5:
                      buttonPtr = button5;
                      break;
                    case 6:
                      buttonPtr = button6;
                      break;
                    case 7:
                      buttonPtr = button7;
                      break;
                    case 8:
                      buttonPtr = button8;
                      break;
                    case 9:
                      buttonPtr = button9;
                      break;
                    case 11:
                      buttonPtr = button0;
                      break;
                }

                if (control != 0){
                  continue;
                }
                else{
                    if(new == old && isNewChar != 1){
                      printChar++;
                      if(*printChar == '\0')
                          printChar = buttonPtr;

                      textbuffer[bufferLength-1] = *printChar;
                      fillRect(top_x,0,5, 8, BLACK);
                    }else{
                      bufferLength ++;
                      top_x += 6;
                      textbuffer[bufferLength - 1] = *buttonPtr;
                      printChar = buttonPtr;
                    }

                    drawChar(top_x, 0, *printChar, WHITE, WHITE, 1);
                }

                MAP_TimerLoadSet(TIMERA1_BASE, TIMER_A, 80000000);
                MAP_TimerIntEnable(TIMERA1_BASE, TIMER_A);
                MAP_TimerEnable(TIMERA1_BASE, TIMER_TIMA_TIMEOUT);

                old = new;
                isNewChar = 0;
            }
          }
            // Re-enable sampling timer
            MAP_TimerLoadSet(TIMERA0_BASE, TIMER_A, 5000);
            MAP_TimerIntEnable(TIMERA0_BASE, TIMER_A);
            MAP_TimerEnable(TIMERA0_BASE, TIMER_TIMA_TIMEOUT);
        }
    }
    return 0;
}
