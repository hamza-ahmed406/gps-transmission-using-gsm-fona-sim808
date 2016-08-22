
#ifndef GSM_FONA_h
#define GSM_FONA_h
/*********************************************************** 
	This is a library for ADAFRUIT FONA 808 GSM module
	to be used in Texas TM4C123G controllers
		
	This library uses total of 5 pins:
	2 for Vcc & Gnd, 2 for Tx & Rx, and 1 for power status
	By default, it sends and receives commands through UART1
	of TIVA(Port C pin 4 and 5) and power status pin is to
	be connected with PH0.
		
	It utilizes Timer0 of TM4C123G for its internal timing purposes
	So, it is recommended not to use timer0 for other tasks.
	All the functions have timing limitations which is 60 sec.
	and will continue to retry (in case of unsuccessful attempt)
	to accomplish the task until there is a timeout.
	
	All Functions return some error/status codes.
	Functions first check for any previous (required) states/errors
	and if found return that without proceeding further.
	After that success or error code due to current task is returned.
	
	Written by Syed Hamza for RDS Research Lab, National university of Science and Technology.
***********************************************************/

#include <stdint.h>
#include <stdbool.h>

#ifndef delay_us
#define delay_us(x)     SysTick_delay_us(x)
#endif

#ifndef delay_ms
#define delay_ms(x)     SysTick_delay_ms(x)
#endif

//defines for network registration... These are the responses received by network registraion query command (AT+CREG?)
#define GSM_NOT_REG					0
#define GSM_REG							1
#define GSM_SRCH_REG				2
#define GSM_DEN_REG					3
#define GSM_UNKNWN_REG			4
#define GSM_ROAM_REG				5

//defines for signal quality, these are minimum CSQ required for successful transmisssion
#define GSM_MIN_SGNL_SMS		5
#define GSM_MIN_SGNL_CALL		10
#define GSM_MIN_SGNL_DATA		20
#define GSM_UNKNOWN_SGNL		99

//defines for sendDAtaMsg() function,these should be passed in transType parameter
#define HTTPInitAndSend			0
#define HTTPComplete				1

//GSM error codes... These codes are returned by GSM functions for application readability
#define GSM_ACT_OK					0
#define GSM_OFF							1000
#define GSM_MIN_POW					1002
#define GSM_FULL_POW				1003
#define GSM_ECHO_OFF				1004
#define GSM_ECHO_ON					1005
#define GSM_CMEE_OFF				1006
#define GSM_CMEE_ON					1007
#define GSM_NOT_INIT				1008
#define SIM_NOT_INS					1010

#define GSM_NET_NOT_REG			1020
#define GSM_NOT_ENGH_SGNL		1022

#define GSM_SMS_TXT					1030
#define GSM_SMS_PDU					1031
#define GSM_SMS_FAILED			1038

#define CONTYPE_NOT_SET			1070
#define APN_NOT_SET					1071
#define GPRS_OFF						1072
#define BEARER_CLOSED				1073
#define GPRS_IP_NOT_SET			1074
#define HTTP_NOT_INIT				1075
#define BEARER_ID_NOT_SET		1076
#define URL_NOT_SET					1077
#define HTTP_ACT_FAILED			1078
#define HTTP_READ_FAILED		1079
#define HTTP_NOT_TERM				1080
#define BEARER_OPENED				1081
#define GPRS_ON							1082

//macros to send commands to GSM or PC
#define TO_PC(str) UARTSend(UART0_BASE,(uint8_t*)str,stringLength(str))

#define TO_GSM(str)	UARTprintf(str);	\
										UARTSend(UART1_BASE,(uint8_t*)str,stringLength(str));

//structure which store all the required status of GSM
struct gstats
{
	bool isInit;
	bool isFullFunc;
	bool isSimIns;
	bool isNetReg;
	bool isSgnlSms;
	bool isSmsSent;
	bool isDataSent;
	bool isbusy;
	bool isTimeout;
	bool isAutoMode;
	int netStat;
	int sgnlQlty;
	
	bool isHTTPInit;
	bool isHTTPConfig;
	char HTTPIPAdd[32];
	int HTTPBytesSent;
	int HTTPActionResp;

	bool isBatCharging;	
	int batPercentage;
	int batVoltage;
};

//constant pointer of struct gstats, declared so that user can READ the GSM current statistics
extern struct gstats const * const GSMStats; 

char* waitAndCheckResp(char *desiredResp);

void GSMTimerEnable(void ); //functions for enabling and disabling timers.
void GSMTimerDisable(void );

bool Init_PortH (void);
int Init_GSM(void);
void GSMPower(void);
bool isGSMOn(void);
void GSMRestartChk(void* pvParam);
int getGSMFunc(void);
int GSMFullFunc(void);
int GSMMinFunc(void);

bool simCardDetectOneTry(void);
int SimCardDetect(void);
int getGSMSignalQlty(void);
int GSMSignalQlty(void);
int getGSMNetworkReg(void);
int GSMNetworkReg(void);
void GSMTroubleshoot(void);

int sendSMS(char *number, char* message);
int sendDataMsg(char dataMsg[], int transType);

int getBatstat(void);
#endif
