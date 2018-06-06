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

#define INT_MIN -2147483648
#define INT_MAX +2147483647
#define MAX_DEPTH 8

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
#define DATE                1    /* Current Date */
#define MONTH               6     /* Month 1-12 */
#define YEAR                2018  /* Current year */
#define HOUR                17    /* Time - hours */
#define MINUTE              36    /* Time - minutes */
#define SECOND              0     /* Time - seconds */

#define POSTHEADER "POST /things/Lab5/shadow HTTP/1.1\n\r"
#define HOSTHEADER "Host: a2krsph2pfhhwe.iot.us-east-1.amazonaws.com\r\n"
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

// Application specific status/error codes
typedef enum
{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;

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
} SlDateTime;

volatile unsigned long g_ulStatus = 0; //SimpleLink Status
unsigned long g_ulPingPacketsRecv = 0; //Number of Ping Packets received
unsigned long g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char g_ucConnectionSSID[SSID_LEN_MAX + 1]; //Connection SSID
unsigned char g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
signed char *g_Host = SERVER_NAME;
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
volatile int mainMenu = 1;
volatile unsigned long ulStatus;
volatile long lRetVal = -1;
volatile int moves[6][7];

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

void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    if (!pWlanEvent)
    {
        return;
    }

    switch (pWlanEvent->Event)
    {
    case SL_WLAN_CONNECT_EVENT:
    {
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
        memcpy(g_ucConnectionSSID,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_name,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
        memcpy(g_ucConnectionBSSID,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
               SL_BSSID_LENGTH);

        UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
                   "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                   g_ucConnectionSSID, g_ucConnectionBSSID[0],
                   g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                   g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                   g_ucConnectionBSSID[5]);
    }
        break;

    case SL_WLAN_DISCONNECT_EVENT:
    {
        slWlanConnectAsyncResponse_t* pEventData = NULL;

        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

        pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

        // If the user has initiated 'Disconnect' request,
        //'reason_code' is SL_USER_INITIATED_DISCONNECTION
        if (SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
        {
            UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                       "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                       g_ucConnectionSSID, g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        else
        {
            UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s, "
                       "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                       g_ucConnectionSSID, g_ucConnectionBSSID[0],
                       g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                       g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                       g_ucConnectionBSSID[5]);
        }
        memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
        memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
    }
        break;

    default:
    {
        UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                   pWlanEvent->Event);
    }
        break;
    }
}

void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    if (!pNetAppEvent)
    {
        return;
    }

    switch (pNetAppEvent->Event)
    {
    case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
    {
        SlIpV4AcquiredAsync_t *pEventData = NULL;

        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

        //Ip Acquired Event Data
        pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

        //Gateway IP address
        g_ulGatewayIP = pEventData->gateway;

        UART_PRINT(
                "[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                "Gateway=%d.%d.%d.%d\n\r",
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 3),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 2),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 1),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 0),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 3),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 2),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 1),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 0));
    }
        break;

    default:
    {
        UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                   pNetAppEvent->Event);
    }
        break;
    }
}

void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    if (!pDevEvent)
    {
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

void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    if (!pSock)
    {
        return;
    }

    switch (pSock->Event)
    {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch (pSock->socketAsyncEvent.SockTxFailData.status)
        {
        case SL_ECLOSE:
            UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                       "failed to transmit all queued packets\n\n",
                       pSock->socketAsyncEvent.SockTxFailData.sd);
            break;
        default:
            UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                       "(%d) \n\n",
                       pSock->socketAsyncEvent.SockTxFailData.sd,
                       pSock->socketAsyncEvent.SockTxFailData.status);
            break;
        }
        break;

    default:
        UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n", pSock->Event);
        break;
    }
}

static long InitializeAppVariables()
{
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    g_Host = SERVER_NAME;
    memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
    return SUCCESS;
}

static long ConfigureSimpleLinkToDefaultState()
{
    SlVersionFull ver = { 0 };
    _WlanRxFilterOperationCommandBuff_t RxFilterIdMask = { 0 };

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode
    if (ROLE_STA != lMode)
    {
        if (ROLE_AP == lMode)
        {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while (!IS_IP_ACQUIRED(g_ulStatus))
            {
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
        if (ROLE_STA != lRetVal)
        {
            // We don't want to proceed if the device is not coming up in STA-mode
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }

    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                        &ucConfigLen, (unsigned char *) (&ver));
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT("Host Driver Version: %s\n\r", SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
               ver.NwpVersion[0], ver.NwpVersion[1], ver.NwpVersion[2],
               ver.NwpVersion[3], ver.ChipFwAndPhyVersion.FwVersion[0],
               ver.ChipFwAndPhyVersion.FwVersion[1],
               ver.ChipFwAndPhyVersion.FwVersion[2],
               ver.ChipFwAndPhyVersion.FwVersion[3],
               ver.ChipFwAndPhyVersion.PhyVersion[0],
               ver.ChipFwAndPhyVersion.PhyVersion[1],
               ver.ChipFwAndPhyVersion.PhyVersion[2],
               ver.ChipFwAndPhyVersion.PhyVersion[3]);

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
    if (0 == lRetVal)
    {
        // Wait
        while (IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
            _SlNonOsMainLoopTask();
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN, ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
    WLAN_GENERAL_PARAM_OPT_STA_TX_POWER,
                         1, (unsigned char *) &ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *) &RxFilterIdMask,
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

static long WlanConnect()
{
    SlSecParams_t secParams = { 0 };
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
    while ((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus)))
    {
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

static int set_time()
{
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
    SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                       sizeof(SlDateTime), (unsigned char *) (&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

static int tls_connect()
{
    SlSockAddrIn_t Addr;
    int iAddrSize;
    unsigned char ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    unsigned int uiIP,
            uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
    long lRetVal = -1;
    int iSockID;

    lRetVal = sl_NetAppDnsGetHostByName(g_Host, strlen((const char *) g_Host),
                                        (unsigned long*) &uiIP, SL_AF_INET);

    if (lRetVal < 0)
    {
        return printErrConvenience(
                "Device couldn't retrieve the host name \n\r", lRetVal);
    }

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(GOOGLE_DST_PORT);
    Addr.sin_addr.s_addr = sl_Htonl(uiIP);
    iAddrSize = sizeof(SlSockAddrIn_t);
    //
    // opens a secure socket
    //
    iSockID = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_SEC_SOCKET);
    if (iSockID < 0)
    {
        return printErrConvenience("Device unable to create secure socket \n\r",
                                   lRetVal);
    }

    //
    // configure the socket as TLS1.2
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod,
                            sizeof(ucMethod));
    if (lRetVal < 0)
    {
        return printErrConvenience("Device couldn't set socket options \n\r",
                                   lRetVal);
    }
    //
    //configure the socket as ECDHE RSA WITH AES256 CBC SHA
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECURE_MASK,
                            &uiCipher, sizeof(uiCipher));
    if (lRetVal < 0)
    {
        return printErrConvenience("Device couldn't set socket options \n\r",
                                   lRetVal);
    }

    //
    //configure the socket with CA certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
    SL_SO_SECURE_FILES_CA_FILE_NAME,
                            SL_SSL_CA_CERT, strlen(SL_SSL_CA_CERT));

    if (lRetVal < 0)
    {
        return printErrConvenience("Device couldn't set socket options \n\r",
                                   lRetVal);
    }

    //configure the socket with Client Certificate - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
    SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME,
                            SL_SSL_CLIENT, strlen(SL_SSL_CLIENT));

    if (lRetVal < 0)
    {
        return printErrConvenience("Device couldn't set socket options \n\r",
                                   lRetVal);
    }

    //configure the socket with Private Key - for server verification
    //
    lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
    SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME,
                            SL_SSL_PRIVATE, strlen(SL_SSL_PRIVATE));

    if (lRetVal < 0)
    {
        return printErrConvenience("Device couldn't set socket options \n\r",
                                   lRetVal);
    }

    /* connect to the peer device - Google server */
    lRetVal = sl_Connect(iSockID, (SlSockAddr_t *) &Addr, iAddrSize);

    if (lRetVal < 0)
    {
        UART_PRINT("Device couldn't connect to server:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
        return printErrConvenience("Device couldn't connect to server \n\r",
                                   lRetVal);
    }
    else
    {
        UART_PRINT("Device has connected to the website:");
        UART_PRINT(SERVER_NAME);
        UART_PRINT("\n\r");
    }

    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
    return iSockID;
}

long printErrConvenience(char * msg, long retVal)
{
    UART_PRINT(msg);
    GPIO_IF_LedOn(MCU_RED_LED_GPIO);
    return retVal;
}

int connectToAccessPoint()
{
    long lRetVal = -1;
    GPIO_IF_LedConfigure(LED1 | LED3);

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
    if (lRetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
            UART_PRINT(
                    "Failed to configure the device in its default state \n\r");

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
    if (lRetVal < 0 || ROLE_STA != lRetVal)
    {
        UART_PRINT("Failed to start the device \n\r");
        return lRetVal;
    }

    UART_PRINT("Device started as STATION \n\r");

    //
    //Connecting to WLAN AP
    //
    lRetVal = WlanConnect();
    if (lRetVal < 0)
    {
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

void oledTest()
{
//    lcdTestPattern();
    fillScreen(BLACK);
    setCursor(10, 15); // set cursor
    setTextColor(MAGENTA, BLACK); // set text color
    setTextSize(1);  // set text size
    Outstr("Welcome to ToolBox! "); // print string to the OLED
    setTextColor(WHITE, BLACK); // set another color

    setCursor(0, 30); // reset cursor
    Outstr("1. Music to ");
    setTextColor(GREEN, BLACK);
    Outstr("Spotify");
    setTextColor(WHITE, BLACK);

    setCursor(0, 45); // reset cursor
    Outstr("2. Event to ");
    setTextColor(YELLOW, BLACK);
    Outstr("Calendar");
    setTextColor(WHITE, BLACK);

    setCursor(0, 60); // reset cursor
    Outstr("3. Issue on ");
    setTextColor(BLUE, BLACK);
    Outstr("Github");
    setTextColor(WHITE, BLACK);

    setCursor(0, 75); // reset cursor
    Outstr("4. Play a ");
    setTextColor(PURPLE, BLACK);
    Outstr("Game");
    setTextColor(WHITE, BLACK);

    setCursor(0, 90); // reset cursor
    Outstr("5. Order a ");
    setTextColor(RED, BLACK);
    Outstr("Pizza!");
    setTextColor(WHITE, BLACK);
}

void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    // Unused in this application
}

static int http_post(int iTLSSockID)
{
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
    if (lRetVal < 0)
    {
        UART_PRINT("POST failed. Error Number: %i\n\r", lRetVal);
        sl_Close(iTLSSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if (lRetVal < 0)
    {
        UART_PRINT("Received failed. Error Number: %i\n\r", lRetVal);
        //sl_Close(iSSLSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    else
    {
        acRecvbuff[lRetVal + 1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}

int detectButton(char tag[])
{
    int prev = -1;
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
                    if (mainMenu == 1)
                    {
                        ulStatus = MAP_GPIOIntStatus(sig_input.port, false);
                        MAP_GPIOIntClear(sig_input.port, ulStatus); //clear interrupts
                        MAP_GPIOIntEnable(sig_input.port, sig_input.pin); // enable interrupts
                        Timer_IF_Start(g_ulRefBase, TIMER_A, 1000); // start 2nd timer
                        buttonEndTime = Timer_IF_GetCount(g_ulRefBase, TIMER_A); // recode the current time
                        return button;
                    }

                    if (button == 11) // mute(enter)
                    {
                        strcat(DATA1,
                               "{\"state\": {\r\n\"desired\" : {\r\n\"default\" : \"");
                        strcat(DATA1, buffer);
                        strcat(DATA1, "\\t\\r\\n");
                        strcat(DATA1, tag);
                        strcat(DATA1, "\"\r\n}}}\r\n\r\n");

                        int i;
//                        http_post(lRetVal);
//                        sl_Stop(SL_STOP_TIMEOUT);

                        int len = strlen(buffer);

                        for (i = 0; i < len; i++) // clean all buffer
                        {
                            buffer[i] = '\0';
                        }
                        fillRect(0, 0, 127, 64, BLACK); // clean screen
                        setCursor(0, 0); // reset cursor
                        buffer[0] = '\0'; // clean buffer
                        ulStatus = MAP_GPIOIntStatus(sig_input.port, false);
                        MAP_GPIOIntClear(sig_input.port, ulStatus); //clear interrupts
                        MAP_GPIOIntEnable(sig_input.port, sig_input.pin); // enable interrupts
                        Timer_IF_Start(g_ulRefBase, TIMER_A, 1000); // start 2nd timer
                        buttonEndTime = Timer_IF_GetCount(g_ulRefBase, TIMER_A); // recode the current time
                        return 0;
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

void display()
{
    fillScreen(BLACK);
    int i;

    // vertical
    for (i = 0; i < 8; i++)
    {
        drawLine((i) * 13, 0, (i) * 13, 78, WHITE);
    }
    // horizontal
    for (i = 0; i < 7; i++)
    {
        drawLine(0, (i) * 13, 91, (i) * 13, WHITE);
    }
    for (i = 0; i < 7; i++)
    {
        setCursor((i + 1) * 13 - 8, 80);
        drawChar(cursor_x, cursor_y, '0' + i, WHITE, BLACK, 1);
    }
}

void Spotify()
{
    fillScreen(BLACK);
    setCursor(0, 0);
    setTextColor(GREEN, BLACK);
    Outstr("Enter Song Name Below");
    setTextColor(WHITE, BLACK);
    setCursor(0, 20);
    detectButton("#Spotify");
}

void Calendar()
{
    fillScreen(BLACK);
    setCursor(0, 0);
    setTextColor(YELLOW, BLACK);
    Outstr("Enter Event Below");
    setTextColor(WHITE, BLACK);
    setCursor(0, 20);
    detectButton("#Calendar");
}

void Github()
{
    fillScreen(BLACK);
    setCursor(0, 0);
    setTextColor(BLUE, BLACK);
    Outstr("Report an Issue");
    setTextColor(WHITE, BLACK);
    setCursor(0, 20);
    detectButton("#Github");
}

int check_won(int row, int col)
{
    int count = 0;
    int i = 0;
    int j = 0;

    //vertical
    for (i = row + 1; i < 6; i++)
    {
        if (moves[i - 1][col] == moves[i][col])
        {
            count++;
            if (count == 3)
                return 1;
        }
        else
            count = 0;
    }

    //horizontal
    count = 0;
    for (j = col + 1; j < 7 && moves[row][j - 1] == moves[row][j]; j++) // right
        count++;
    for (j = col - 1; j >= 0 && moves[row][j + 1] == moves[row][j]; j--) // left
        count++;
    if (count >= 3)
        return 1;
    count = 0;

//    rightDiag '/'
    count = 0;
    for (i = row - 1, j = col + 1;
            i >= 0 && j < 7 && moves[i][j] == moves[i + 1][j - 1]; i--, j++) //up
        count++;
    for (i = row + 1, j = col - 1;
            i < 6 && j >= 0 && moves[i][j] == moves[i - 1][j + 1]; i++, j--) //down
        count++;
    if (count == 3)
        return 1;

//    leftDiag '\'
    count = 0;
    for (i = row - 1, j = col - 1;
            i >= 0 && j >= 0 && moves[i][j] == moves[i + 1][j + 1]; i--, j--) //up
        count++;
    for (i = row + 1, j = col + 1;
            i < 6 && j < 7 && moves[i][j] == moves[i - 1][j - 1]; i++, j++) //down
        count++;
    if (count == 3)
        return 1;

    return 0;
}

void connect42P()
{
    int count = 0;
    int player = 1;
    int nextMove;
    char string[100];
    int row;
    int i;

    while (1)
    {
        for (i = 0; i < 100; i++)
            string[i] = '\0';

        fillRect(0, 95, 128, 5, BLACK);
        setCursor(0, 95);
        strcat(string, "Player ");
        char tmp[1] = '0' + player;
        strcat(string, tmp);
        char winInfo[15];
        strcpy(winInfo, string);
        strcat(string, "'s move");
        Outstr(string);
        setCursor(0, 110);
        Outstr("Select column");
        nextMove = detectButton("");

        if (nextMove > 6)
        {
            fillRect(0, 110, 128, 5, BLACK);
            setCursor(0, 110);
            Outstr("Select a valid column");

            while (nextMove > 6)
                nextMove = detectButton("");
        }

        row = drop_and_display(nextMove, player);

        if (row == -1)
        {
            fillRect(0, 110, 128, 5, BLACK);
            setCursor(0, 110);
            Outstr("Select a valid column");

            while (row == -1)
            {
                nextMove = detectButton("");
                row = drop_and_display(nextMove, player);
            }
        }

        count++;

        if (check_won(row, nextMove))
        {
            fillRect(0, 95, 128, 30, BLACK);
            setCursor(0, 110);
            strcat(winInfo, " Win!");
            Outstr(winInfo);
            //            delay(50);
            break;
        }

        if (count == 42)
        {
            fillRect(0, 95, 128, 30, BLACK);
            setCursor(0, 110);
            Outstr("Draw!");
            //            delay(50);
            break;
        }

        player = (player == 1) ? 2 : 1;
    }
}

int get_new_state(int col, int player) // drop player in given col, update board, return row "supposing given col always valid"
{
    int i;
    for (i = 5; i >= 0; i--)
    {
        if (moves[i][col] == 0)
        {
            moves[i][col] = player;
            return i;
        }
    }

    return -1;
}

int getUnitScore(int player, int p1Count, int comCount)
{
    int raw_score[7] = { 0, 1, 4, 32, 128, 512, 2048 };
    if (p1Count == comCount)
    {
        if (player == 2)
            return 1;
        return -1;
    }
    else if (p1Count < comCount)
    {
        if (player == 2)
            return raw_score[comCount + 1] - raw_score[p1Count];
        return raw_score[comCount] - raw_score[p1Count];
    }
    else
    {
        if (player == 2)
            return raw_score[comCount] - raw_score[p1Count];
        return raw_score[comCount] - raw_score[p1Count + 1];
    }
}

int getHeuristicScore(int depth, int col)
{
    int score = 0;
    int comCount = 0; // com
    int p1Count = 0; // p1
    int row = 0;
    int v, h, x = col, y = 0;
    for (row = 5; row >= 0; row--)
    {
        if (moves[row][col] == 0)
        {
            y = row + 1;
            break;
        }
    }

    int vertical = 1, horizontal = 1, diagA = 1, diagB = 1;
    int connect = 1;

    int player = moves[y][x];

    //vertical
    comCount = p1Count = 0;
    for (v = x + 1; v < 6; v++)
    { // up
        if (connect == 1 && moves[y][v] == player)
            vertical++;
        else
            connect = 0;

        if (moves[y][v] == 2)
            comCount++;
        else if (moves[y][v] == 1)
            p1Count++;
    }

    connect = 1;

    for (v = x - 1; v >= 0; v--)
    { // down
        if (connect == 1 && moves[y][v] == player)
            vertical++;
        else
            connect = 0;

        if (moves[y][v] == 2)
            comCount++;
        else if (moves[y][v] == 1)
            p1Count++;
    }

    connect = 1;

    if (vertical >= 4)
    {
        if (depth < 3)
            return player == 2 ? INT_MAX : INT_MIN;
    }
    score += getUnitScore(player, p1Count, comCount);

    //horizontal
    comCount = p1Count = 0;
    for (h = y + 1; h < 7; h++)
    { // right
        if (connect == 1 && moves[h][x] == player)
            horizontal++;
        else
            connect = 0;

        if (moves[h][x] == 2)
            comCount++;
        else if (moves[h][x] == 1)
            p1Count++;
    }

    connect = 1;

    for (h = y - 1; h >= 0; h--)
    { // left
        if (connect == 1 && moves[h][x] == player)
            horizontal++;
        else
            connect = 0;

        if (moves[h][x] == 2)
            comCount++;
        else if (moves[h][x] == 1)
            p1Count++;
    }

    connect = 1;

    if (horizontal >= 4)
    {
        if (depth < 3)
            return player == 2 ? INT_MAX : INT_MIN;
    }
    score += getUnitScore(player, p1Count, comCount);

    //digaA "/"
    comCount = p1Count = 0;
    for (h = y + 1, v = x + 1; h < 6 && v < 7; v++, h++)
    { //up right
        if (connect == 1 && moves[h][v] == player)
            diagA++;
        else
            connect = 0;

        if (moves[h][v] == 2)
            comCount++;
        else if (moves[h][v] == 1)
            p1Count++;
    }

    connect = 1;

    for (h = y - 1, v = x - 1; h >= 0 && v >= 0; h--, v--)
    { // down left
        if (connect == 1 && moves[h][v] == player)
            diagA++;
        else
            connect = 0;

        if (moves[h][v] == 2)
            comCount++;
        else if (moves[h][v] == 1)
            p1Count++;
    }

    connect = 1;

    if (diagA >= 4)
    {
        if (depth < 3)
            return player == 2 ? INT_MAX : INT_MIN;
    }
    score += getUnitScore(player, p1Count, comCount);

    //digaB "\"
    comCount = p1Count = 0;
    for (h = y - 1, v = x + 1; h >= 0 && v < 7; h--, v++)
    { // up left
        if (connect == 1 && moves[h][v] == player)
            diagB++;
        else
            connect = 0;

        if (moves[h][v] == 2)
            comCount++;
        else if (moves[h][v] == 1)
            p1Count++;
    }

    connect = 1;

    for (h = y + 1, v = x - 1; v >= 0 && h < 6; h++, v--)
    { // left
        if (connect == 1 && moves[h][v] == player)
            diagB++;
        else
            connect = 0;

        if (moves[h][v] == 2)
            comCount++;
        else if (moves[h][v] == 1)
            p1Count++;
    }

    connect = 1;

    if (diagB >= 4)
    {
        if (depth < 3)
            return player == 2 ? INT_MAX : INT_MIN;
    }
    score += getUnitScore(player, p1Count, comCount);
    return score;
}

int alphabeta(int alpha, int beta, int depth, int player, int row, int col) // alpha-beta algorithm
{
    int possibleSteps[7] = { -1, -1, -1, -1, -1, -1, -1 };

    int result = check_won(row, col);
    if (result != 0 || depth == MAX_DEPTH)
    {
        return getHeuristicScore(depth, col); // return the score
    }

    for (col = 0; col < 7; col++)
    {
        if (moves[0][col] == 0)
            possibleSteps[col] = col;
    }

    for (col = 0; col < 7; col++)
    {
        int step = possibleSteps[col];
        if (step == -1)
            continue;

        if (alpha >= beta)
            return player > 1 ? alpha : beta; // true: maximize the score, false: minimize the score

        if (player > 1) // COM maximize
        {
            int row = get_new_state(step, 2);
            int score = alphabeta(alpha, beta, depth + 1, 1, row, step);
            moves[row][step] = 0;
            alpha = score > alpha ? score : alpha;
        }
        else // P1 minimize
        {
            int row = get_new_state(step, 1);
            int score = alphabeta(alpha, beta, depth + 1, 2, row, step);
            moves[row][step] = 0;
            beta = score < beta ? score : beta;
        }
    }

    return player > 1 ? alpha : beta;
}

int evaluateMove()
{ //1 for P1, 2 for COM
    int depth = 0;
    int alpha = INT_MIN; // max lower bound
    int beta = INT_MAX; // min upper bound
    int possibleSteps[7] = { -1, -1, -1, -1, -1, -1, -1 };
    int col = 0;
    int bestMove = -1;

    for (col = 0; col < 7; col++)
    {
        if (moves[0][col] == 0)
            possibleSteps[col] = col;
    }

    for (col = 0; col < 7; col++)
    { // check for win (winning purpose)
        int step = possibleSteps[col];
        if (step == -1)
            continue;
        int win = 0;

        int row = get_new_state(step, 2); // test if COM will win in next move, 2 for COM
        if (check_won(row, step))
            win = 1;
        moves[row][step] = 0; // undo the move
        if (win == 1)
            return step;
    }

    for (col = 0; col < 7; col++)
    { // check for lose (blocking purpose)
        int step = possibleSteps[col];
        if (step == -1)
            continue;
        int win = 0;

        int row = get_new_state(step, 1); // test if P1 will win in next move, 1 for P1
        if (check_won(row, step))
            win = 1;
        moves[row][step] = 0; // undo the move
        if (win == 1)
            return step;
    }

    for (col = 0; col < 7; col++)
    {
        if (possibleSteps[col] != -1)
        {
            bestMove = possibleSteps[col];
            break;
        }
    }

    for (col = 0; col < 7; col++)
    {
        int step = possibleSteps[col];

        if (step == -1)
            continue;

        if (alpha >= beta)
            return bestMove;

        int row = get_new_state(step, 2);
        int score = alphabeta(alpha, beta, depth + 1, 2, row, step);
        moves[row][step] = 0;

        if (score > alpha)
        {
            bestMove = step;
            alpha = score;
        }
    }

    return bestMove;
}

int drop_and_display(int column, int player)
{
    int i;

    for (i = 5; i >= 0; i--)
    {
        if (moves[i][column] == 0)
        {
            moves[i][column] = player;

            if (player == 1)
                fillCircle(column * 13 + 6, (i + 1) * 13 - 6, 4, RED);
            else
                fillCircle(column * 13 + 6, (i + 1) * 13 - 6, 4, BLUE);

            return i;
        }
    }

    return -1;
}

void connect4AI()
{
    int count = 0;
    int player = 1;
    int nextMove;
    char string[100];
    int row;
    int i;

    while (1)
    {
        for (i = 0; i < 100; i++)
            string[i] = '\0';

        fillRect(0, 95, 128, 5, BLACK);
        setCursor(0, 95);
        if (player == 1)
        {
            strcat(string, "Your Move");
            Outstr(string);
            setCursor(0, 110);
            Outstr("Select column");
            nextMove = detectButton("");
        }
        else
        {
            fillRect(0, 95, 128, 5, BLACK);
            Outstr("I'm thinking...");
            nextMove = evaluateMove();
        }

        if (nextMove > 6)
        {
            fillRect(0, 110, 128, 5, BLACK);
            setCursor(0, 110);
            Outstr("Select a valid column");

            while (nextMove > 6)
                nextMove = detectButton("");
        }

        row = drop_and_display(nextMove, player);

        if (row == -1)
        {
            fillRect(0, 110, 128, 5, BLACK);
            setCursor(0, 110);
            Outstr("Select a valid column");

            while (row == -1)
            {
                nextMove = detectButton("");
                row = drop_and_display(nextMove, player);
            }
        }

        count++;

        if (check_won(row, nextMove))
        {
            fillRect(0, 95, 128, 30, BLACK);
            setCursor(0, 110);
            if (player == 1)
                Outstr("You Win!");
            else
                Outstr("COM is smarter!");
            //            delay(50);
            break;
        }

        if (count == 42)
        {
            fillRect(0, 95, 128, 30, BLACK);
            setCursor(0, 110);
            Outstr("Draw!");
            //            delay(50);
            break;
        }

        player = (player == 1) ? 2 : 1;
    }

}

void Game()
{
    setCursor(0, 0);
    fillScreen(BLACK);
    setCursor(5, 15); // set cursor
    setTextColor(MAGENTA, BLACK); // set text color
    setTextSize(1);  // set text size
    Outstr("Welcome to Connect4! "); // print string to the OLED
    setTextColor(WHITE, BLACK); // set another color
    setCursor(10, 40); // set cursor
    Outstr("1. P1 vs COM"); // print string to the OLED
    setCursor(10, 70); // set cursor
    Outstr("2. P1 vs P2"); // print string to the OLED

    int selection = detectButton("");
    while (selection < 1 || selection > 2)
    {
        selection = detectButton("");
    }

    display();

    switch (selection)
    {
    case 1:
        connect4AI();
        break;
    case 2:
        connect42P();
        break;
    }

    fillRect(0, 30, 128, 30, BLACK);
    setCursor(0, 40);
    Outstr("Press Enter..");
    int i, j;
    for(i = 0; i < 6; i++)
        for(j = 0; j < 7; j++)
            moves[i][j] = 0;
    strcat(buffer, "I just win the Connect4 on CC3200!");
    mainMenu = 0;
    detectButton("#FB");
}

void Pizza()
{
    fillScreen(BLACK);
    setCursor(0, 0);
    setTextColor(RED, BLACK);
    Outstr("Type any key...");
    setTextColor(WHITE, BLACK);
    setCursor(0, 20);
    detectButton("#Pizza");
}

void loading()
{
    fillScreen(BLACK);
    setCursor(20, 50);
    setTextColor(WHITE, BLACK);
    Outstr("Loading...");
}

int main()
{
    BoardInit();

    PinMuxConfig();
//
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);

    InitTerm();

    ClearTerm();
    UART_PRINT("My terminal works!\n\r");

    MAP_PRCMPeripheralReset(PRCM_GSPI);
    oledInit();
    loading();
//
    MAP_UARTConfigSetExpClk(UARTA1_BASE,
    MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                            UART_BAUD_RATE,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                            UART_CONFIG_PAR_NONE));

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

//    lRetVal = connectToAccessPoint();
//    //Set time so that encryption can be used
//    lRetVal = set_time();
//    if (lRetVal < 0)
//    {
//        UART_PRINT("Unable to set time in the device");
//        LOOP_FOREVER()
//        ;
//    }
//    //Connect to the website with TLS encryption
//    lRetVal = tls_connect();
//    if (lRetVal < 0)
//    {
//        ERR_PRINT(lRetVal);
//    }

    oledTest();

    while (1)
    {
        int choice = detectButton(""); // get choice

        while (choice < 1 || choice > 5)
            choice = detectButton("");

        mainMenu = 0; // disable menu flag enable character typing
        switch (choice)
        {
        case 1:
            Spotify();
            break;
        case 2:
            Calendar();
            break;
        case 3:
            Github();
            break;
        case 4:
            mainMenu = 1;
            Game();
            break;
        case 5:
            Pizza();
            break;
        }

        mainMenu = 1;
        oledTest();
    }

}
