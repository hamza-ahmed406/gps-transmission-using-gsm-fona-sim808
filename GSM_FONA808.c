#include "GSM_FONA808.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/rom.h"
#include "driverlib/debug.h"
#include "utils/ustdlib.h"
#include "inc/tm4c123gh6pge.h"
#include "inc/SysTick.h"
#include "strfunc.h" 
#include "uartConfiguration.h"
#include "utils/uartstdio.h"



//structure object to store the GSM statistics
struct gstats stats;
struct gstats const * const GSMStats=&stats;

//GSM unsolicted response codes.
char URC[11][32]= {"SMS Ready",
									"GPS Ready",
									"Call Ready",
									"RING",
									"+SAPBR 1: DEACT",
									"+CPIN: NOT READY",
									"+CMTI:",
									"+CMT:",
									"UNDER-VOLTAGE",
									"+CBM:",
									"CHARGE-ONLY MODE"};



void waitForUART1()		//waits for the UART1 to receive response
{
	while(!UARTIntStat[1])		//while there is no interrupt
	{
		delay_ms(1);
		if (stats.isTimeout || !stats.isInit)	//break if timeout has occured
			break;
	}
	delay_ms(10);
}

bool URCfound()	//this function checks if the response from GSM uart is URC
{
	for (int i=0; i<11; i++)
		if (ustrstr(UARTResponse[1],URC[i])!=NULL)
			return true;
	
	return false;
}

char* waitAndCheckResp(char *desiredResp)	//waits for the response and eliminates URC from buffer
{
	char *respStart;
	waitForUART1();
	respStart= ustrstr(UARTResponse[1],desiredResp);
	if (respStart!=NULL)	//return successful if desired response is read
		return respStart;
	
	if (URCfound())		//else check for URC and if found
	{
		respStart= ustrstr(UARTResponse[1],desiredResp);
		if (respStart!=NULL)	//check again for desired response
			return respStart;		
		else
		{
			responseRead(UART1_BASE);							//clear buffer and
			return waitAndCheckResp(desiredResp);	//wait for response again
		}
	}
	
	else									//if there is no URC or desired response, return false
		return NULL;
}


void PortHIntHandler(void)
{
	GPIOIntClear(GPIO_PORTH_BASE, GPIO_INT_PIN_0);		//clear the GPIO int status	

	stats.isInit=false;																//clearing init flag
	TO_PC("\r\n\n\n\n\n***************************POWER DOWN!!***************************\r\n\n\n\n\n");
	delay_ms(4000);
}

bool Init_PortH () //initializes Port H, which is going to be used for power failure detection
{	
	//Enable PortH GPIO PIN 0-7	
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOH);			//enabling PORTH peripherals
	GPIOPinTypeGPIOInput(GPIO_PORTH_BASE, GPIO_PIN_0);		//setting Port H pin 0 to input type
	GPIOIntTypeSet(GPIO_PORTH_BASE, GPIO_PIN_0, GPIO_FALLING_EDGE);//setting int to falling edge(will be called at from high-to-low change)
	ROM_IntEnable(INT_GPIOH);
	GPIOIntEnable(GPIO_PORTH_BASE, GPIO_PIN_0);		//enabling the interrupt
	return true;
}


void configTimerGSM() //configure timer0 with time period of 60 sec. for functions timing purposes
{																										
	ROM_IntMasterDisable();														//disabling the master interrupt
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER1);//enabling peripherels for timer
  ROM_IntMasterEnable();														//enabling the master interrupt types
	
	TimerClockSourceSet(WTIMER1_BASE,TIMER_CLOCK_SYSTEM);
	ROM_TimerConfigure(WTIMER1_BASE, TIMER_CFG_A_ONE_SHOT);				//setting it to one shot timer (one time only).

  ROM_TimerIntEnable(WTIMER1_BASE, TIMER_TIMA_TIMEOUT);					//enabling timer interrupts

	ROM_TimerLoadSet64( WTIMER1_BASE,( (uint64_t)(ROM_SysCtlClockGet() )*60) );	//setting timer value

	ROM_IntMasterEnable();
	ROM_IntEnable(INT_WTIMER1A);
	ROM_TimerIntEnable(WTIMER1_BASE,TIMER_TIMA_TIMEOUT);
}

void WTimer1IntHandler(void)//timer interrupt to set timeout to true after one minute
{
	ROM_TimerIntClear(WTIMER1_BASE, TIMER_TIMA_TIMEOUT);	//clearing interrupt status

	TO_PC("*****TIMEOUT*****\r\n");

	stats.isTimeout=true; 															//setting timeout flag
}

void GSMTimerEnable() //to enable timer and re-initialize the value
{
	stats.isTimeout=false;																					 //setting timeout to false	

	ROM_TimerLoadSet(WTIMER1_BASE, TIMER_A, ( (uint64_t)(ROM_SysCtlClockGet() )*60) ); //setting time to one minute
	ROM_TimerEnable(WTIMER1_BASE, TIMER_A);													 //enable Wtimer1A
}

void GSMTimerDisable()	//Disable Wtimer1A    
{ 
	ROM_TimerDisable(WTIMER1_BASE, TIMER_A);
}
	


int Init_GSM(void) //initializes GSM, must be used for configuring FONA properly
{
	static int counterForInit;
	while (!stats.isInit)
	{
		if (isGSMOn())	//check if GSM is on
		{
			do
			{
				responseRead(UART1_BASE);		
				TO_GSM("ATE0\r\n");		//sets echo response to zero (no echo of command signal)
			}
			while(!waitAndCheckResp("OK"));
	
		responseRead(UART1_BASE);
		TO_GSM("AT+CMEE=1\r\n");	//enables detailed error reporting
		waitAndCheckResp("OK");

		responseRead(UART1_BASE);
		TO_GSM("AT+CREG=0\r\n");	//set registration mode to zero
		waitAndCheckResp("OK");

		responseRead(UART1_BASE);
		TO_GSM("AT+CSCS=\"GSM\"\r\n");
		waitAndCheckResp("OK");

		responseRead(UART1_BASE);
		TO_GSM("AT+CMGF=1\r\n"); //setting sms mode to text mode
		waitAndCheckResp("OK");
		
		if (counterForInit==0)
		{
			configTimerGSM();
			Init_PortH();												//initializing port H used by GSM module
		}
		
		stats.isFullFunc=getGSMFunc();				//checking functionality level
		stats.isSimIns=simCardDetectOneTry();	//checking for sim card presence
		stats.netStat=getGSMNetworkReg();			//checking for network registery
		stats.sgnlQlty=getGSMSignalQlty();		//checking for signal quality
		stats.isHTTPInit=false;
		stats.isHTTPConfig=false;
		stats.isAutoMode=true;	//setting auto-mode so that GSM could be reinitialized in case of power failure
		stats.isInit=true;			//setting init flag
		counterForInit++;
		}
		
		else //if GSM is off, wait for a while until it is on again (auto-start functionaity)
			delay_ms(4000);
	}
	return GSM_ACT_OK;
}


bool isGSMOn() //function to check if FONA is responding by sending a test AT command
{
	responseRead(UART1_BASE);
	TO_GSM("AT\r\n"); //sending attention command

	return waitAndCheckResp("OK") ;
}

void GSMRestartChk(void* pvParam) //function to be called by application 
{		//to decide what should happen in case of power failure
	
}


int getGSMFunc()			//get GSM functionality
{
	char *resp;					
	responseRead(UART1_BASE);							//clear buffer
	TO_GSM("AT+CFUN?\r\n");								//send command to check functionality
	resp=waitAndCheckResp("\r\n+CFUN: ");	//if proper response is read
	if (resp)															
		return atoi(resp+9);								//extract the functionality and return it
	else
		return GSM_MIN_POW;
}

int GSMFullFunc() //set full functionality. should be used before sending SMS, DATA, voice call.
{
	
	if (!stats.isInit)		//checking for any previous stats required to proceed further
		return GSM_NOT_INIT; //returning the error codes
	if (stats.isFullFunc)
		return GSM_ACT_OK;
	GSMTimerEnable();

	while (stats.isTimeout==false && !stats.isFullFunc)//loop while timeout has occured due to timer handler or functionality set to high
	{
		responseRead(UART1_BASE);
		TO_GSM("AT+CFUN=1\r\n"); //setting GSM full functionality
		stats.isFullFunc= waitAndCheckResp("OK");	//setting flag if  proper response is received

		if (!stats.isFullFunc)			//else, wait for a while and repeat again
			delay_ms(2000);
	}
	GSMTimerDisable();
		
	if (stats.isFullFunc)	//return the code for success
		return GSM_ACT_OK;		
	else									//return error code related to current function
	return GSM_MIN_POW;	
}

int GSMMinFunc() //sets functionality to minimum. should be called when in idle mode.
{
	if (!stats.isInit)		//checking for any previous stats required to proceed further
		return GSM_NOT_INIT;
	if (!stats.isFullFunc)
		return GSM_ACT_OK;
	
	GSMTimerEnable();
	while (!stats.isTimeout && stats.isFullFunc)//loop while timeout has occured due to timer handler or functionality set to low
	{
		responseRead(UART1_BASE);
		TO_GSM("AT+CFUN=0\r\n"); 			//setting GSM minimum functionality

		if ( waitAndCheckResp("OK") ) //clearing flag if proper response is received
			stats.isFullFunc=false;
		else													//else wait for a while and try again.
			delay_ms(2000);
	}
	
	GSMTimerDisable();
	if (stats.isFullFunc)	//return error code related to current function
		return GSM_FULL_POW;		
	else	//return the code for success
	{
		stats.isSimIns=false;
		stats.netStat= stats.isNetReg=false;
		stats.sgnlQlty= stats.isSgnlSms = false;
		return GSM_ACT_OK;
	}
}


int SimCardDetect()  //function to enforce the user to insert SIM if not present
{
	if (!stats.isInit)
		return GSM_NOT_INIT;
	if (!stats.isFullFunc)	//check for any previous errors,
		return GSM_MIN_POW;		//return the same old error code without proceeding further

	GSMTimerEnable();
	stats.isSimIns=simCardDetectOneTry(); //checking for sim card presence
	if (!stats.isSimIns)									//in case of no sim, prompt the user to insert a sim
	{
		stats.netStat= stats.isNetReg= false;
		stats.sgnlQlty= stats.isSgnlSms = false;
		TO_PC("sim not detected, please insert sim and restart the gsm\r\n");
	}
	
	while(stats.isTimeout==false && !stats.isSimIns) //loop while timeout has occured or till user inserts a sim card
	{
		delay_ms(2000);
		stats.isSimIns=simCardDetectOneTry();
	}	
	GSMTimerDisable();
	
	if (stats.isSimIns)
		return GSM_ACT_OK; //return the code for success
	else
		return SIM_NOT_INS;//return error code related to current function
}

bool simCardDetectOneTry() //detects if SIM is present or not
{	
	responseRead(UART1_BASE);
	TO_GSM("AT+CPIN?\r\n"); //checking for sim card

	return waitAndCheckResp("+CPIN: READY") ;		//checking if SIM is ready
}


int GSMNetworkReg() 			//checks and waits till network is registered
{
	if (!stats.isFullFunc)	//checking for any previous stats required to proceed further and
		return GSM_MIN_POW;		//return those error codes
	if (!stats.isSimIns)
		return SIM_NOT_INS;
		
	GSMTimerEnable();
	stats.netStat=getGSMNetworkReg(); //checking for network registery
	if (stats.netStat==GSM_REG || stats.netStat==GSM_ROAM_REG) //setting flag if GSM is registered to network
		stats.isNetReg=true;
	else
	{		
		stats.isNetReg=false;//clearing flag in case of unsuccessful attempt
		stats.sgnlQlty=stats.isSgnlSms= 0;
	}
		
	while(!stats.isTimeout && !stats.isNetReg) //loop while timeout has occured or GSM is connected to an operator
	{
		delay_ms(2000);	
		stats.netStat=getGSMNetworkReg();
		if (stats.netStat==GSM_REG || stats.netStat==GSM_ROAM_REG)
			stats.isNetReg=true;
	}
	
	GSMTimerDisable();
	
	if (stats.isNetReg)	
		return GSM_ACT_OK;			//return the code for success
	else
		return GSM_NET_NOT_REG;	//return error code related to current function
}

int getGSMNetworkReg() //query and get network status
{	
	char *resp;
	responseRead(UART1_BASE);
	TO_GSM("AT+CREG?\r\n"); //code for resgistery checking

	resp=waitAndCheckResp("\r\n+CREG: "); // response is: \r\n+CREG: <mode><stat> where mode should be 0 and stat should be 1

	if (	resp )
		return atoi(resp+11);
	else
		return GSM_NOT_REG;
}


int GSMSignalQlty()				//checks and wait until minimum signal for transmission is received
{	
	if (!stats.isFullFunc)	//checking for any previous stats required to proceed further and
		return GSM_MIN_POW;		//return those error codes
	if (!stats.isSimIns)
		return SIM_NOT_INS;
	if (!stats.isNetReg)
		return GSM_NET_NOT_REG;

	GSMTimerEnable();
	stats.sgnlQlty=getGSMSignalQlty(); //checking for signal quality

	while(stats.isTimeout==false && (stats.sgnlQlty<GSM_MIN_SGNL_SMS || stats.sgnlQlty==GSM_UNKNOWN_SGNL) ) 
	{																	//trying until timeout has occured or minimum signal required for transmission is acheived
		delay_ms(2000);
		stats.sgnlQlty=getGSMSignalQlty();
			if (stats.sgnlQlty==GSM_UNKNOWN_SGNL)
				stats.sgnlQlty=0;
	}

	GSMTimerDisable();
	if (stats.sgnlQlty<GSM_MIN_SGNL_SMS) 
	{	
		stats.isSgnlSms=false;			//clearing flag if signal quality is less than required and
		return GSM_NOT_ENGH_SGNL;		//return error code related to current function 
	}
	else
	{
		stats.isSgnlSms=true;		//setting flag if signal quality is acheived and
		return GSM_ACT_OK;			//return the code for success
	}
}

int getGSMSignalQlty() //query and gets signal quality strength
{
	char * resp;
	responseRead(UART1_BASE);
	TO_GSM("AT+CSQ\r\n");//command for signal quality	
	resp=waitAndCheckResp("\r\n+CSQ:");

	if ( resp ) // response is: \r\n+CSQ: <rssi>,<ber> where rssi should be atleast GSM_MIN_SGNL
		return atoi(resp+8);
	else
		return 0;
}


int sendSMS(char *number, char* message) //sends SMS provided by  message character array
{
	
	stats.isSmsSent=false;	//clearing the SmsSent flag

	if (!stats.isFullFunc) 	//checking for any previous stats required to proceed further and
		return GSM_MIN_POW;		//return those error codes
	if (!stats.isSimIns)
		return SIM_NOT_INS;
	if (!stats.isNetReg)
		return GSM_NET_NOT_REG;
	if (!stats.isSgnlSms)
		return GSM_NOT_ENGH_SGNL;
		
	TO_PC("\n\rSending SMS..");
	char cmd1[32] = "AT+CMGS=\"";			// Compose AT command	
	stringAppend(cmd1,number); //command formation for entering phone numbers
	stringAppend(cmd1,"\",129\r\n");

	GSMTimerEnable();			//enabling timer	

	//loop while timeout has occured or SMS has been sent successfully
	while (!stats.isSmsSent && stats.isTimeout==false)
	{
		responseRead(UART1_BASE);
		TO_GSM(cmd1); //sending phone number
		
		if ( !waitAndCheckResp("\r\n> ") )	//waiting and checking for reply prompting to send the message
		{
			delay_ms(2000);
			continue;
		}

		responseRead(UART1_BASE);
		TO_GSM(message); //sending message
		UARTCharPut(UART1_BASE,0x1A); //sending ascii ctrl <z> (26)  for sending message
	
		stats.isSmsSent= waitAndCheckResp("+CMGS: ");	//checking response originated after successful transmission and 
																										//setting the flag if sending is successful
		if (!stats.isSmsSent)
			delay_ms(2000);
	}
		//end of while

	GSMTimerDisable();	
	
	if (stats.isSmsSent)
	{		
		TO_PC("\n\rMessage sent to "); //msg sent status on PC
		TO_PC(number);

		return GSM_ACT_OK;			//return the code for success
	}
	else
		return GSM_SMS_FAILED; 	//return error code related to current function 
}


int sendDataCommands(char dataMsg[],int cases)		//to send data command defined by cases variable 
{																									//and return the new command number to be sent
	char* resp; //to store HTTPACTION response
	
	switch (cases)
	{
					
		case 0:												//initial stage
			responseRead(UART1_BASE);
			TO_GSM("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");	//setting configuration parameters to GPRS
			if ( waitAndCheckResp("OK") )					// increment cases if proper response is received 
				cases++;
			delay_ms(1500);												//delay required by GSM to configure
		break;
	
		case 1:
			responseRead(UART1_BASE);
			TO_GSM("AT+SAPBR=3,1,\"APN\",\"zonginternet\"\r\n");	//setting APN to zong. for (ufone: ufone.pinternet)

			if ( waitAndCheckResp("OK") )					// increment cases if proper response is received
			{
				responseRead(UART1_BASE);
				TO_GSM("AT+SAPBR=3,1,\"USER\",\"\"\r\n");
				if ( waitAndCheckResp("OK") )
				{
					responseRead(UART1_BASE);				
					TO_GSM("AT+SAPBR=3,1,\"PWD\",\"\"\r\n");
					if (waitAndCheckResp("OK") )
					{
						delay_ms(1500);								//delay required by GSM to configure
						cases++;
						stats.isHTTPConfig=true;
					}
				}
			}				
		break;
	
		case 2:
			responseRead(UART1_BASE);
			TO_GSM("AT+CGATT=1\r\n");			//turn on GPRS for proper configuration		
			if ( waitAndCheckResp("OK") )	//increment cases if OK is received
				cases++;
		break;

		case 3:
			responseRead(UART1_BASE);
			TO_GSM("AT+SAPBR=1,1\r\n");							//setting BEARER on

			if ( waitAndCheckResp("OK") ) //increment cases if OK is received
			{
				delay_ms(2000);												//delay required by GSM for proper configuration
				cases++;
			}
			else
				sendDataCommands(dataMsg,11);
				
			break;
		
		case 4:
			responseRead(UART1_BASE);
			TO_GSM("AT+SAPBR=2,1\r\n");
			resp = waitAndCheckResp("\r\n+SAPBR: ");
			if (resp)
			{
				int i;
				for (i=15; resp[i]!='\"'; i++)
					stats.HTTPIPAdd[i-15]=resp[i];
				stats.HTTPIPAdd[i-15]=0;
				if ( ustrcmp(stats.HTTPIPAdd, "0.0.0.0") )
					cases++;
			}
				
			break;
		
	
		case 5:
			responseRead(UART1_BASE);
			TO_GSM("AT+HTTPINIT\r\n");				//initializing HTTP
			if ( waitAndCheckResp("OK") )			//increment cases if OK is received
			{
				stats.isHTTPInit=true;					//set HTTPinit flag
				cases++;
			}
			else															//else terminate HTTP and try again
				sendDataCommands(dataMsg,10);
			break;
	
		case 6:
			responseRead(UART1_BASE);
			TO_GSM("AT+HTTPPARA=\"CID\",1\r\n");	//setting bearer profile identifier
			if ( waitAndCheckResp("OK") )					//increment cases if OK is received
				cases++;
			break;
	
		case 7:
			responseRead(UART1_BASE);
			TO_GSM(dataMsg);							//send URL and DATA
			if (waitAndCheckResp("OK") )	//increment cases if OK is received
				cases++;
			break;
	
		case 8:
			responseRead(UART1_BASE);
			TO_GSM("AT+HTTPACTION=0\r\n"); //send command via GET (0) method
			if ( waitAndCheckResp("OK") ) //if OK is received, wait for HTTPACTION response
			{
				if (!ustrstr(UARTResponse[1],"\r\n+HTTPACTION:") )
					responseRead(UART1_BASE);
				resp= waitAndCheckResp("\r\n+HTTPACTION: ");
				if ( resp )									//proceed if proper response is received		
				{
					stats.HTTPActionResp= atoi(resp+17);	//save the response code.
					if (stats.HTTPActionResp==200)				// increment cases if 200 (means successful)  is received
						cases++;
				}
			}	
			break;
			
		case 9:
			responseRead(UART1_BASE);
			TO_GSM("AT+HTTPREAD\r\n");		//read the number of bytes sent (used just as a check to ensure data is sent)
			resp= waitAndCheckResp("\r\n+HTTPREAD: ");
			if ( resp ) //proceed if proper response is received
			{
				stats.HTTPBytesSent=atoi(resp +13);	//save the no. of bytes sent
				if (stats.HTTPBytesSent ==0)				//send data again if bytes sent is 0
					cases=8;
				else																//increment cases if some bytes are sent
					cases++;
			}
			break;
	
		case 10:
			responseRead(UART1_BASE);		
			TO_GSM("AT+HTTPTERM\r\n");		//terminate HTTP mode
			if ( waitAndCheckResp("OK") )	//increment cases if OK is received
			{
				stats.isHTTPInit=false;			//clear HTTPinit flag
				cases++;
			}
			break;
			
		case 11:
			responseRead(UART1_BASE);
			TO_GSM("AT+SAPBR=0,1\r\n");		//turning off conncetion.
			if ( waitAndCheckResp("OK") )
				cases++;
			break;
			
		case 12:
			responseRead(UART1_BASE);
			TO_GSM("AT+CGATT=0\r\n");			//turn off GPRS		
			if ( waitAndCheckResp("OK") )	//increment cases if OK is received
				cases++;
			break;

	}
	return cases;
}

int sendDataMsg(char link[], int transType)
{
	char dataMsg[256]="AT+HTTPPARA=\"URL\",\"";
	stringAppend(dataMsg,link);
	stringAppend(dataMsg,"\"\r\n");

	stats.isDataSent=false;	//clearing the DataSent flag
	if (!stats.isInit)			//checking for any previous stats required to proceed further and
		return GSM_NOT_INIT;	//returning those error codes
	if (!stats.isSimIns)
		return SIM_NOT_INS;
	if (!stats.isNetReg)
		return GSM_NET_NOT_REG;
	if (!stats.isSgnlSms)
		return GSM_NOT_ENGH_SGNL;

	int currStage=0,newStage=0, finalStage=13;//to store the data commands stages

	if  (stats.isHTTPConfig)
		currStage=newStage=2;
	if (stats.isHTTPInit)										//if HTTP has been initialized, set stage to 6
		currStage=newStage=6;
	if (transType==HTTPInitAndSend)					//if transmissionType is set to send only,
		finalStage=10;												//the program won't terminate HTTP.
		
	GSMTimerEnable();												//enable timer
	while (currStage!=finalStage && stats.isTimeout==false)		//trying until timeout has occured
	{																													//or it  has reached to the final stage
		newStage=sendDataCommands(dataMsg,currStage);
		if (newStage==currStage)
			delay_ms(1000);
		else
		{
			GSMTimerDisable();
			GSMTimerEnable();
		}
		currStage=newStage;
		
	}
	GSMTimerDisable();

	if (currStage==finalStage) //if all the commands are executed properly, set flag and return code for success
	{
		stats.isDataSent=true;
		return GSM_ACT_OK;
	}
	else																//else, return the error code
	{
		if (currStage>4)									//if HHTP has been initialized, terminate it for proper shutdown 
		{
			if (transType==HTTPComplete)		//terminate HTTP only if complete process is required
			{
				GSMTimerEnable();
				newStage=10;
				while (newStage!=finalStage && stats.isInit && !stats.isTimeout) //trying to terminate it until timeout.
				{
					newStage= sendDataCommands(dataMsg,newStage);
					delay_ms(400);
				}
				GSMTimerDisable();
			}
		}
		
		if (currStage>=9)
			return GSM_ACT_OK;
		else
			return (CONTYPE_NOT_SET+currStage); //return error code
	}
	
}


int getBatstat()				//TO get battery stats
{
	if (!stats.isInit)
		return GSM_NOT_INIT;
	
	char *resp;
	char batStat[3][32]={"\0","\0","\0"};	//temporary char buffer to store battery status
	int row=0,col=0;											//row, column to move the buffer
	responseRead(UART1_BASE);

	TO_GSM("AT+CBC\r\n");										//	sending command to get battery state
	resp= waitAndCheckResp("\r\n+CBC: ");		//response is \r\n+CBC: <bcs>,<bcl>,<voltage> where bcs: battery charging state(0-1)
	delay_ms(100);													//bcl: bat. connection level(1-100), voltage:bat voltage in mV
	if ( resp )	
	{
		for (int i=8;  resp[i]!='\r' && resp[i]!='\0'; i++)	//parse the response until \r or \0 is received
		{
			if (resp[i]==',' || col>=30 )						//move to new row if comma is received or	
			{																				// array index is out of limit.
				batStat[row][col]=0;									//append a null-character
				row++;
				col=0;
			}
			else
				batStat[row][col++]=resp[i];					//else, copy the data to buffer
		}

		batStat[row][col]=0;											//append a null-character
		stats.isBatCharging=atoi(batStat[0]);			//convert string into integers
		stats.batPercentage=atoi(batStat[1]);
		stats.batVoltage=atoi(batStat[2]);
		
		return GSM_ACT_OK;
	}
	else
		return 1;
}
