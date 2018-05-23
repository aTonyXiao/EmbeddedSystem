
//****************************************************************************

// Standard includes
#include <stdio.h>
#include "simplelink.h"
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
#include "gpio_if.h"
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
#include "common.h"

#define SPI_IF_BIT_RATE  100000
//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************

#define MAX_URI_SIZE 128
#define URI_SIZE MAX_URI_SIZE + 1


#define APPLICATION_NAME        "SSL"
#define APPLICATION_VERSION     "1.1.1.EEC.Spring2018"
#define SERVER_NAME                "a2krsph2pfhhwe.iot.us-east-1.amazonaws.com"
#define GOOGLE_DST_PORT             8443

#define SL_SSL_CA_CERT "/cert/rootCA.der"
#define SL_SSL_PRIVATE "/cert/private.der"
#define SL_SSL_CLIENT  "/cert/client.der"

//NEED TO UPDATE THIS FOR IT TO WORK!
#define DATE                21    /* Current Date */
#define MONTH               5     /* Month 1-12 */
#define YEAR                2018  /* Current year */
#define HOUR                21    /* Time - hours */
#define MINUTE              18    /* Time - minutes */
#define SECOND              0     /* Time - seconds */

#define POSTHEADER "POST /things/Lab5/shadow HTTP/1.1\n\r"
#define HOSTHEADER "Host: a2krsph2pfhhwe.iot.us-east-1.amazonaws.com\r\n"
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

// Application specific status/error codes
typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

typedef struct
{
   /* time */
   unsigned long tm_sec;
   unsigned long tm_min;
   unsigned long tm_hour;
   /* date */
   unsigned long tm_day;
   unsigned long tm_mon;
   unsigned long tm_year;
   unsigned long tm_week_day; //not required
   unsigned long tm_year_day; //not required
   unsigned long reserved[3];
}SlDateTime;


volatile unsigned long  g_ulStatus = 0;//SimpleLink Status
unsigned long  g_ulPingPacketsRecv = 0; //Number of Ping Packets received
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
signed char    *g_Host = SERVER_NAME;
SlDateTime g_time;
#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


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
volatile char DATA1[100] = "\0";

// frequency of clock per tick: 80MHZ = 12.5ns

static long WlanConnect();
static int set_time();
static void BoardInit(void);
static long InitializeAppVariables();
static int tls_connect();
static int connectToAccessPoint();
static int http_post(int);


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

void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent) {
    if(!pWlanEvent) {
        return;
    }

    switch(pWlanEvent->Event) {
        case SL_WLAN_CONNECT_EVENT: {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t'.
            // Applications can use it if required
            //
            //  slWlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //

            // Copy new connection SSID and BSSID to global parameters
            memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
                   STAandP2PModeWlanConnected.ssid_name,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
            memcpy(g_ucConnectionBSSID,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                   SL_BSSID_LENGTH);

            UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
                       "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                       g_ucConnectionSSID,g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT: {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_USER_INITIATED_DISCONNECTION
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code) {
                UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                    "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            else {
                UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s, "
                           "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
            memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
        }
        break;

        default: {
            UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                       pWlanEvent->Event);
        }
        break;
    }
}


void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {
    if(!pNetAppEvent) {
        return;
    }

    switch(pNetAppEvent->Event) {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT: {
            SlIpV4AcquiredAsync_t *pEventData = NULL;

            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            //Ip Acquired Event Data
            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

            //Gateway IP address
            g_ulGatewayIP = pEventData->gateway;

            UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                       "Gateway=%d.%d.%d.%d\n\r",
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,0));
        }
        break;

        default: {
            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Event);
        }
        break;
    }
}


void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent) {
    if(!pDevEvent) {
        return;
    }

    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}


void SimpleLinkSockEventHandler(SlSockEvent_t *pSock) {
    if(!pSock) {
        return;
    }

    switch( pSock->Event ) {
        case SL_SOCKET_TX_FAILED_EVENT:
            switch( pSock->socketAsyncEvent.SockTxFailData.status) {
                case SL_ECLOSE:
                    UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                                "failed to transmit all queued packets\n\n",
                                    pSock->socketAsyncEvent.SockTxFailData.sd);
                    break;
                default:
                    UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                                "(%d) \n\n",
                                pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
                  break;
            }
            break;

        default:
            UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
          break;
    }
}


static long InitializeAppVariables() {
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    g_Host = SERVER_NAME;
    memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
    return SUCCESS;
}

static long ConfigureSimpleLinkToDefaultState() {
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode
    if (ROLE_STA != lMode) {
        if (ROLE_AP == lMode) {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while(!IS_IP_ACQUIRED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
            }
        }

        // Switch to STA role and restart
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Check if the device is in station again
        if (ROLE_STA != lRetVal) {
            // We don't want to proceed if the device is not coming up in STA-mode
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }

    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                                &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                                SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);



    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal) {
        // Wait
        while(IS_CONNECTED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();

    return lRetVal; // Success
}





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

static long WlanConnect() {
    SlSecParams_t secParams = {0};
    long lRetVal = 0;

    secParams.Key = SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    UART_PRINT("Attempting connection to access point: ");
    UART_PRINT(SSID_NAME);
    UART_PRINT("... ...");
    lRetVal = sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT(" Connected!!!\n\r");


    // Wait for WLAN Event
    while((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))) {
        // Toggle LEDs to Indicate Connection Progress
        _SlNonOsMainLoopTask();
        GPIO_IF_LedOff(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
        _SlNonOsMainLoopTask();
        GPIO_IF_LedOn(MCU_IP_ALLOC_IND);
        MAP_UtilsDelay(800000);
    }

    return SUCCESS;

}

static int set_time() {
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}


static int tls_connect() {
    SlSockAddrIn_t    Addr;
    int    iAddrSize;
    unsigned char    ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    unsigned int uiIP,uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
    long lRetVal = -1;
    int iSockID;

    lRetVal = sl_NetAppDnsGetHostByName(g_Host, strlen((const char *)g_Host),
                                    (unsigned long*)&uiIP, SL_AF_INET);

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't retrieve the host name \n\r", lRetVal);
    }

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(GOOGLE_DST_PORT);
    Addr.sin_addr.s_addr = sl_Htonl(uiIP);
    iAddrSize = sizeof(SlSockAddrIn_t);
    //
    // opens a secure socket
    //
    iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, SL_SEC_SOCKET);
    if( iSockID < 0 ) {
        return printErrConvenience("Device unable to create secure socket \n\r", lRetVal);
    }

    //
    // configure the socket as TLS1.2
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod,\
                               sizeof(ucMethod));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }
    //
    //configure the socket as ECDHE RSA WITH AES256 CBC SHA
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &uiCipher,\
                           sizeof(uiCipher));
    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //
    //configure the socket with CA certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                           SL_SO_SECURE_FILES_CA_FILE_NAME, \
                           SL_SSL_CA_CERT, \
                           strlen(SL_SSL_CA_CERT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //configure the socket with Client Certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
                SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME, \
                                    SL_SSL_CLIENT, \
                           strlen(SL_SSL_CLIENT));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }

    //configure the socket with Private Key - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, \
            SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME, \
            SL_SSL_PRIVATE, \
                           strlen(SL_SSL_PRIVATE));

    if(lRetVal < 0) {
        return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
    }


    /* connect to the peer device - Google server */
    lRetVal = sl_Connect(iSockID, ( SlSockAddr_t *)&Addr, iAddrSize);

    if(lRetVal < 0) {
        UART_PRINT("Device couldn't connect to server:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
        return printErrConvenience("Device couldn't connect to server \n\r", lRetVal);
    }
    else {
        UART_PRINT("Device has connected to the website:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
    }

    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
    return iSockID;
}



long printErrConvenience(char * msg, long retVal) {
    UART_PRINT(msg);
    GPIO_IF_LedOn(MCU_RED_LED_GPIO);
    return retVal;
}



int connectToAccessPoint() {
    long lRetVal = -1;
    GPIO_IF_LedConfigure(LED1|LED3);

    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);

    lRetVal = InitializeAppVariables();
    ASSERT_ON_ERROR(lRetVal);

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0) {
      if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
          UART_PRINT("Failed to configure the device in its default state \n\r");

      return lRetVal;
    }

    UART_PRINT("Device is configured in default state \n\r");

    CLR_STATUS_BIT_ALL(g_ulStatus);

    ///
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    //
    UART_PRINT("Opening sl_start\n\r");
    lRetVal = sl_Start(0, 0, 0);
    if (lRetVal < 0 || ROLE_STA != lRetVal) {
        UART_PRINT("Failed to start the device \n\r");
        return lRetVal;
    }

    UART_PRINT("Device started as STATION \n\r");

    //
    //Connecting to WLAN AP
    //
    lRetVal = WlanConnect();
    if(lRetVal < 0) {
        UART_PRINT("Failed to establish connection w/ an AP \n\r");
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }

    UART_PRINT("Connection established w/ AP and IP is aquired \n\r");
    return 0;
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

void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent, SlHttpServerResponse_t *pHttpResponse) {
    // Unused in this application
}

static int http_post(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, POSTHEADER);
    pcBufHeaders += strlen(POSTHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int dataLength = strlen(DATA1);

    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);

    pcBufHeaders += strlen(CLHEADER1);
    sprintf(cCLLength, "%d", dataLength);

    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    strcpy(pcBufHeaders, DATA1);
    pcBufHeaders += strlen(DATA1);

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}

int main()
{
    unsigned long ulStatus;
    long lRetVal = -1;
    int prev = -1;

    BoardInit();

    PinMuxConfig();

    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);

    InitTerm();

    ClearTerm();
    UART_PRINT("My terminal works!\n\r");

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


    lRetVal = connectToAccessPoint();
       //Set time so that encryption can be used
       lRetVal = set_time();
       if(lRetVal < 0) {
           UART_PRINT("Unable to set time in the device");
           LOOP_FOREVER();
       }
       //Connect to the website with TLS encryption
       lRetVal = tls_connect();
       if(lRetVal < 0) {
           ERR_PRINT(lRetVal);
       }



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
                        strcat(DATA1, "{\"state\": {\r\n\"desired\" : {\r\n\"default\" : \"");
                        strcat(DATA1, buffer);
                        strcat(DATA1, "\"\r\n}}}\r\n\r\n");

                        int i;
                        http_post(lRetVal);
                        sl_Stop(SL_STOP_TIMEOUT);

                        for (i = 0; i < strlen(buffer); i++) // clean all buffer
                        {
                            buffer[i] = '\0';
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


