 /************************************************
	Main application for VMS project.
	This application uses module of ADAFRUIT (FONA 808)
	and sends GPS data via text message and to a URL link.
 
	flagSet(),called periodically by scheduler utility,is used
	to update program status by setting flags in (statusFlag)
	structure. A watchdog timer is configured to reset program
	on system hangs.
	
	All the data that is being displayed through UART0 is being
	saved on SDcard in "mainDir"/"project"/"testNo."/"time".txt
	and a brief log file is created to store the total success/
	failure transmissions along with time.
	
	All statuses are being displayed on graphic display which
	updates itself every 3 seconds. It includes battery status
	and GPS/GSM flags.On scrolling left/right,a little	detail
	would be shown of GPS/GSM work & error codes.

	After performing tasks, it has two options:
	1: To go to hibernation (for an hour),in which it can also
	be woken again by pressing wake button.
	2: to wait some time (set by wide timer0)for transmission
	while collecting latest GPS data. It is set to option 2.
		
	Connections: PC4->FONA TX, PC5->FONA RX, PH0->PWR STAT
************************************************/


#include <stdint.h>
#include <stdbool.h>
#include <String.h>
#include <stdlib.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include <time.h>
#include "driverlib/timer.h"
#include "driverlib/hibernate.h"
#include "drivers/buttons.h"
#include "inc/tm4c123gh6pge.h"
#include "inc/SysTick.h"
#include "inc/lcd.h"
#include "strfunc.h"
#include "uartConfiguration.h"
#include "GPS_FONA.h"
#include "GSM_FONA808.h"
#include "utils/scheduler.h"
#include "utils/ustdlib.h"
#include "grlib/grlib.h"
#include "drivers/cfal96x64x16.h"
#include "rdsllogo.c"
#include "utils/uartstdio.h"
#include "sd_card.h"


//systick frequency(for systick interrupt timing), set to 200Hz
#define TICKS_PER_SECOND 200				

//macro for composeMessage(), appending a string with GPSData variables
#define ADD_TO_MSG(par) stringAppend(msg,par);\
												stringAppend(msg,GPSData->par)

#define USR_LED			(*((volatile uint32_t *)0x40026010)) //LED (on board) to check program working visually


#define TIME_TO_TRANSMIT		10			//time(in minutes) after which transmission is going to occur

//phone numbers to send SMS to
#define PHONE_NUMBER1			"03437647452"
#define PHONE_NUMBER2			"03356601703"

//writes a number on display given by coordinates 
#define DRAW_INT(intVal,xVal,yVal,minLimit)			do																				\
																		{																							\
																			char c[8];																	\
																			usprintf(c,"%0"minLimit"d",intVal);					\
																			GrStringDraw(&g_sContext,c,-1,xVal,yVal,1);	\
																		}																							\
																		while (0)

//---------------------
// Function prototypes
//---------------------

bool Init_PortG (void);
uint32_t configHibernate(void);
void configWtimer(void);
uint64_t getTime(char * timeStr);
bool displayInit(void);
void dispUpdate(void*);
void buttonHandler(void*);
bool composeMessage(void);
void composeDataMsg(void);
void hibernateActivate(void);
void configWatchDog(void);
void flagsSet(void *);
void GPSWork(void);
void GSMWork(void);


//------------------
// Global Variables/
//------------------

char msg[256]; 		//To store Text message
char URL[256];		//to store Data link and message


tContext g_sContext;			//object for display context

int scroll=0;							//to store the scroll value being used by display update function.
bool bScrollPressed=true;	//flag to check if scroll button has been pressed
static volatile bool bSelectPressed = 0; //button status for Wake button


//Struct for all the statuses and reports for this program
typedef struct 
{
	bool disp, UART, PortG, Hib, initDone,
			 compMsg, GPSDone, GSMDone;
	int success;
	int sdErrCode, GPSErrCode,SMSErrCode,DATAErrCode;
}statusFlag;

statusFlag sFlag;


tSchedulerTask g_psSchedulerTable[] =	//Table consisting of tasks(functions) for scheduler to perform
{
	{FatFsTickTimer, (void*)0, TICKS_PER_SECOND*0,0, true}, //fatfs require atleast 100Hz clock
	{buttonHandler, (void*)0, TICKS_PER_SECOND/100, 0, true},		//checks button status every 100th sec.
	{flagsSet	, (void*)0, TICKS_PER_SECOND/10, 0, true},			//checks flags status every 10th of sec
	{dispUpdate,(void*)0, TICKS_PER_SECOND*3,0,true}          //updates display every 3 sec	
};

uint32_t g_ui32SchedulerNumTasks = (sizeof(g_psSchedulerTable) / sizeof(tSchedulerTask)); //no of tasks

int timerCounter=TIME_TO_TRANSMIT;		//timer counter used by wide timer 0.



//------------------------------
//////////Main Function\\\\\\\\\\
//-------------------------------

int main()
{
	
	//initializations
	char time[32]="\0";
  ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | 
                       SYSCTL_XTAL_16MHZ);   //setting system frequency 50MHz
  ROM_FPULazyStackingEnable();

	SysTick_Init();
	SchedulerInit(TICKS_PER_SECOND);				//initialize scheduler
	configWatchDog();												//initialize watchdog
	sFlag.UART=configUART();								//initializing UART used by the application
//	sFlag.PortG=Init_PortG ();
	ButtonsInit();													//configuring buttons
	configWtimer();													//configuring wide timer0 for timing purposes
	sFlag.Hib= configHibernate();						//configuring hibernate.
	sFlag.disp= displayInit();							//initializing display

	while (1)
	{
		scroll=0;
		bScrollPressed=true;
		dispUpdate(0);
		uint64_t ui_time= getTime(time);					//getting system time for file creation

		while ( ui_time >= 20160918000000 )				//display fatal error if time limit of package ends
		{
			scroll=5;
			bScrollPressed=true;
			dispUpdate(0);
			delay_ms(5000);
		}
		
		sFlag.sdErrCode= sdInit("VMS","GSM","TEST23", time);		//initializing sd ports and files

		if (!GSMStats->isInit)
			Init_GSM();
		logData(&sFlag.initDone,"%b");					//logging init results

		getBatstat();	
		GPSWork();

		if (sFlag.GPSDone)											//move forward if gps work succeeds.
		{	
			if (timerCounter >=TIME_TO_TRANSMIT)	//if it is time to transmit
			{
				GSMWork();													//send the gps data
				if (!sFlag.GSMDone)									//if not successful,    
				{}																	// place alternative way to transmit
			}
		}
		
		TO_PC("\r\n\n\n\n\n***************************PROGRAM END***************************\r\n\n\n\n\n");
		closeFiles(sFlag.success);

		if (sFlag.success==1)
			timerCounter-=TIME_TO_TRANSMIT;									//decrement counter

		delay_ms(30000);
			//	hibernateActivate();
			//	SysCtlReset();
	}	
}//end main


//----------------------
// Function definitions
//----------------------

void configWtimer()
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER0);
	ROM_TimerDisable(WTIMER0_BASE, TIMER_BOTH);
	ROM_TimerIntDisable(WTIMER0_BASE, TIMER_TIMA_TIMEOUT);
	TimerClockSourceSet(WTIMER0_BASE,TIMER_CLOCK_SYSTEM);
	
	ROM_TimerConfigure(WTIMER0_BASE, TIMER_CFG_PERIODIC);
	
	ROM_TimerLoadSet64(WTIMER0_BASE, ROM_SysCtlClockGet()*60);
	ROM_IntMasterEnable();
	ROM_IntEnable(INT_WTIMER0A);
	ROM_TimerIntEnable(WTIMER0_BASE,TIMER_TIMA_TIMEOUT);
	ROM_TimerEnable(WTIMER0_BASE, TIMER_BOTH);
}

char tmstrr[10];
void WTimer0IntHandler()
{
	 ROM_TimerIntClear(WTIMER0_BASE, TIMER_TIMA_TIMEOUT);
	timerCounter++;
	usprintf(tmstrr,"%i",timerCounter);
	TO_PC("\r\n\ntimer==");
	TO_PC(tmstrr);
	TO_PC("\r\n\n");
}


uint64_t getTime(char * timeStr)
{
	timeStr[0]=0;
	static uint64_t ui64time;
	struct tm initTime={2,32,20,2,7,116,false};			//starting time; RTC started counting from 20:32:02, 02 August 2016 
	char tempTime[32]="\0";													//to store temporary time conversion data
	struct tm newTime;															//to store actual time(=initial+elapsed time)
 	time_t initsec=umktime(&initTime);							//convert init time data to number of seconds
	uint64_t elapsedSec=HibernateRTCGet();					//get elapsed time in seconds from hibernate RTCC register

	ulocaltime(initsec+elapsedSec,&newTime);				//converting total seconds to yyyymmddhhmmss format

	usprintf(tempTime,"%u",(newTime.tm_year+1900));
	stringAppend(timeStr,tempTime);
	stringAppend(timeStr,"-");

	usprintf(tempTime,"%02u",(newTime.tm_mon+1));
	stringAppend(timeStr,tempTime);
	stringAppend(timeStr,"-");

	usprintf(tempTime,"%02u",newTime.tm_mday);
	stringAppend(timeStr,tempTime);
	stringAppend(timeStr,"-");

	usprintf(tempTime,"%02u",newTime.tm_hour);
	stringAppend(timeStr,tempTime);
	stringAppend(timeStr,"-");

	usprintf(tempTime,"%02u",newTime.tm_min);
	stringAppend(timeStr,tempTime);
	stringAppend(timeStr,"-");

	usprintf(tempTime,"%02u",newTime.tm_sec);
	stringAppend(timeStr,tempTime);

	removeInArray(tempTime,timeStr,'-');
	ui64time= strtoull(tempTime,0,0);
	return ui64time;
}


bool Init_PortG ()
{
		//Enable PortG GPIO PIN 0-7
	SYSCTL_RCGC2_R |= 0x00000040;
	GPIO_PORTG_AMSEL_R = 0x00;
	GPIO_PORTG_PCTL_R = 0x00000000;
	GPIO_PORTG_AFSEL_R = 0x00;
	GPIO_PORTG_DEN_R |= 0xFF;
	GPIO_PORTG_DIR_R |= 0xFF;
	return true;
}


void buttonHandler(void* param)//for the detection of wake button pressing and releasing, called every time with schedulerSystickIntHandler
{
	uint8_t ui8Data, ui8Delta;
  ui8Data = ButtonsPoll(&ui8Delta, 0);

  if(BUTTON_PRESSED(SELECT_BUTTON, ui8Data, ui8Delta))	//if select button is pressed
		bSelectPressed = 1;																	//set bSelectPressed      
  if(BUTTON_RELEASED(SELECT_BUTTON, ui8Data, ui8Delta))	//if select button is released
   	bSelectPressed = 0;																	//clear bSelectPressed
    if (BUTTON_PRESSED(RIGHT_BUTTON,ui8Data,ui8Delta))  //if right button is pressed
		{
			bScrollPressed=true;															//set scoll to true so that display could be updated
			if (++scroll==5)																	//increment scroll and if scroll gets equal to 5
				scroll=0;																				//set scroll to 0 for circular scrolling
			dispUpdate(0);
		}
		if (BUTTON_PRESSED(LEFT_BUTTON,ui8Data,ui8Delta))		//if left button is pressed
		{
			bScrollPressed=true;															//set scoll to true so that display could be updated
			if (--scroll==-1)																	//decrement scroll and if scroll gets equal to -1
				scroll=4;																				//set scroll to 4 for circular scrolling
			dispUpdate(0);
		}
    
}


uint32_t configHibernate() //for hibernation configuration, 
{
	uint32_t ui32Status;
	uint32_t pui32NVData[64];

	if (HibernateIsActive())
	{
		ui32Status = HibernateIntStatus(0);
		HibernateIntClear(ui32Status);
	}
	else
	{
		ui32Status=0;
		SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);	//enable peripheral
		HibernateEnableExpClk(SysCtlClockGet());
		delay_ms(2000);
		HibernateClockConfig(HIBERNATE_OSC_LOWDRIVE);
		HibernateRTCEnable();
		HibernateRTCSet(0);

		HibernateDataSet(pui32NVData, 16);
		HibernateWakeSet(HIBERNATE_WAKE_RESET | HIBERNATE_WAKE_PIN | HIBERNATE_WAKE_RTC );//setting wake button and RTC wake
	}
  HibernateRTCTrimSet(0x8080);		//(0x80E8);
	HibernateRTCMatchSet(0, HibernateRTCGet() + 3600); //setting hibernate time
	return true;
}


void hibernateActivate(void) //go to hibernation state
{
	bSelectPressed = 0;																	
	HibernateRequest();																	
  bSelectPressed=0;																		
  
	while(!bSelectPressed){} //hibernate until wake button is pressed or RTC has matched
}


void configWatchDog() //configure watchdog timer, with resetting capability after 2nd timeout
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG0);
	ROM_IntMasterEnable();
	ROM_IntEnable(INT_WATCHDOG);
	ROM_WatchdogReloadSet(WATCHDOG0_BASE, ROM_SysCtlClockGet()*20);//setting watcdog timeout to 4 seconds
	ROM_WatchdogResetEnable(WATCHDOG0_BASE);
	ROM_WatchdogEnable(WATCHDOG0_BASE);
	ROM_IntPrioritySet(INT_WATCHDOG,3);
}


void WatchdogIntHandler(void)
{    
  ROM_WatchdogIntClear(WATCHDOG0_BASE); // Clear the watchdog interrupt
	delay_us(1);
}


///	.:GPS Functions:.

void GPSWork()
{
	sFlag.GPSErrCode=0;								//to store error code
	char errorCodeStr[8]="\0"; 				//to display error code
	GPSPower();												//turn on gps power
	GPSFix();													//wait for fix
	sFlag.GPSErrCode= GPSGetData(0);	//get gps data of mode 0

	composeDataMsg();									//compose URL and message
	sFlag.compMsg= composeMessage();	//compose text message


	usprintf(errorCodeStr,"%d",sFlag.GPSErrCode);
	TO_PC("\r\n\n\n\n****************GPS_ERROR=");
	TO_PC(errorCodeStr);
	TO_PC("****************\r\n\n\n\n");	
	
	logData(&sFlag.GPSDone,"%b");	
}


bool composeMessage()
{
	UARTSend(UART0_BASE,(uint8_t *)"\n\rComposing SMS..",17);
	
	char Time[] = "GPS Data\n\nTime: ";
	char Lat[] = "\nLat: ";
	char Long[] = "\nLong: ";
	char Alt[] = "\nAltitude: ";
	char Fix[] = "\nFix Value: ";
	char Sat[] = "\nSat Num: ";
	char Speed[] = "\nSpeed: ";
	char Bearing[] = "\nBearing: ";
	char Date[] = "\nDate: ";
	char fixStr[3]="\0";
	msg[0] = '\0';
	
	//add respective parameters
	ADD_TO_MSG(Time); 
	ADD_TO_MSG(Date);
	ADD_TO_MSG(Lat);
	ADD_TO_MSG(Long);
	ADD_TO_MSG(Alt);
	ADD_TO_MSG(Sat);
	ADD_TO_MSG(Speed);
	ADD_TO_MSG(Bearing);
	
	stringAppend(msg,Fix);
	usprintf(fixStr,"%i",GPSStats->fixVal);
	stringAppend(msg,fixStr);

	UARTSend(UART0_BASE,(uint8_t *) msg,(uint32_t) stringLength(msg));
	
	return true;
}


void composeDataMsg()
{
	URL[0]=0;
	// add URL
	stringAppend(URL,"http://data.sparkfun.com/input/WGwdNbZYjYFb92VX3x5o?private_key=XR0Wz8jGMGf7A5E1Vklq&latitude=");
	stringAppend(URL,GPSData->Lat); //add latitude
	//stringAppend(dataCommands[5],"&&longitude=\0");
	stringAppend(URL,"&longtitude=");
	stringAppend(URL,GPSData->Long); //adding longitude
	//stringAppend(dataCommands[5],"&&HBR=77&&OL=94&&speed=35&&fuel=2.5&&car=1&&sign=0");
}


///	.:GSM Functions:.

void GSMWork()
{
	sFlag.SMSErrCode=0;						//variable for error code storage
	char errorCodeStr[8];					//to display error code

	GSMFullFunc();								//set functionality to 1 (enable network registration)
	SimCardDetect();							//check if sim card is present (till 1 minute)
	GSMNetworkReg();							//check for network registration (till 1 minute)
	GSMSignalQlty();							//check for signal quality (till 1 minute)

	sFlag.SMSErrCode= sendSMS(PHONE_NUMBER1, msg);	//send SMS and retrieve status code

	usprintf(errorCodeStr,"%d",sFlag.SMSErrCode);		//convert error code to string
	TO_PC("\r\n\n\n\n****************SMS_ERROR=");
	TO_PC(errorCodeStr);														//display error code
	TO_PC("****************\r\n\n\n\n");

	logData(&GSMStats->isSmsSent,"%b");

	sFlag.DATAErrCode=0;
	sFlag.DATAErrCode=sendDataMsg(URL, HTTPComplete);	//send data and retrieve error code

	usprintf(errorCodeStr,"%d",sFlag.DATAErrCode);		//convert error code to string
	TO_PC("\r\n\n\n\n****************DATA_ERROR=");
	TO_PC(errorCodeStr);															//display error code
	TO_PC("****************\r\n\n\n\n");
		
	if (GSMStats->isDataSent && GSMStats->isSmsSent)	//if all gsm work is done,
		GSMMinFunc();																		//set gsm to min functionality
	
	logData(&GSMStats->isDataSent,"%b");
}


void flagsSet(void *pvParam)		//sets the main status flags (called by scheduler)
{
	
	if (GSMStats->isInit
							&& sFlag.UART && sFlag.Hib)				 //if all initializations are made
		sFlag.initDone=true;												//set initDone true
	else
		sFlag.initDone=false;
	
	if (GPSStats->isInfoRcvd && sFlag.compMsg)		//set GPSdone if GPS work is done and msg is composed
		sFlag.GPSDone=true;
	else
		sFlag.GPSDone=false;
	
	if (GSMStats->isSmsSent && !GSMStats->isFullFunc && GSMStats->isDataSent)//if all GSM work is done
		sFlag.GSMDone=true;																										//set GSMDone true
	else
		sFlag.GSMDone=false;
	
	if (timerCounter<10)										//set success flag to -1 if it isn't time to transmit
		sFlag.success=-1;
	else
	{
		if (sFlag.GPSDone && sFlag.GSMDone)		//else, set success to 1 if transmission is successful
			sFlag.success=1;
		else
			sFlag.success=0;										//else, set success to 0 (i.e transmission failed)	
	}
}


//	.:Display Functions:.

bool displayInit()
{
    CFAL96x64x16Init();		//initialinzing display driver
    GrContextInit(&g_sContext, &g_sCFAL96x64x16);	//initializing graphic context
	return true;

}

void dispClear(int xMin,int xMax,int yMin, int yMax) //clear the display section provided by coordinates
{
	if (!sFlag.disp)
		return;
		
	tRectangle sRect;																	//object for rectangle

	GrContextForegroundSet(&g_sContext, ClrBlack);		//set fore ground to black
  sRect.i16XMin = xMin;
  sRect.i16XMax = xMax;
  sRect.i16YMin = yMin;
  sRect.i16YMax = yMax;
  GrRectFill(&g_sContext, &sRect);		//fill the display with black color.
}

void dispTitle(char *psTitle)
{
	if (!sFlag.disp)
		return;

	tRectangle sRect;			//object for rectangle
	//setting rectangle pixels from (0,0) to (95, 11)
	sRect.i16XMin = 0;
  sRect.i16YMin = 0;
  sRect.i16XMax = GrContextDpyWidthGet(&g_sContext) - 1;
  sRect.i16YMax = 11;
  GrContextForegroundSet(&g_sContext, ClrDarkBlue);	//set foreground color to blue
  GrRectFill(&g_sContext, &sRect);									//fill the space with blue rectangle
	
  GrContextForegroundSet(&g_sContext, ClrWhite);		//set the foreground to white
  GrContextFontSet(&g_sContext, g_psFontFixed6x8);	//set font to 6x8
  GrStringDrawCentered(&g_sContext, psTitle, -1, GrContextDpyWidthGet(&g_sContext) / 2, 5, 0);//display the title centered.
}


void dispUpdate(void *pvParam)
{
	if (!sFlag.disp)
		return;
	
	switch (scroll)
	{
		case 0:								//if scroll is 0, display the RDS logo
			if (bScrollPressed)
			{
				dispClear(0,95,0,63);					//clear the old display.
				dispTitle("RDS RESEARCH LAB");//set the title 
				GrImageDraw(&g_sContext,g_pui8rdsllogo, 0, 15); //draw the RDS logo
				GrStringDrawCentered(&g_sContext, "<          >", -1, GrContextDpyWidthGet(&g_sContext) / 2, 55, 0);
				bScrollPressed=false;		//clear bScrollPressed
			}
		break;
			
		case 1:								//if scroll is 1, display the overall progress.
			if (bScrollPressed) //if scroll button is pressed (i.e new diplay has to be shown)
			{
				dispClear(0,95,0,63);	//clear the old display.
				dispTitle("PROGRESS");	//set the title to progress

				//display the headings.
				GrStringDrawCentered(&g_sContext, " INIT: , GPS:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 20, 0);
				GrStringDrawCentered(&g_sContext, " DATA: , SMS:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 30, 0);
				GrStringDrawCentered(&g_sContext, "TIME:  ,BAT%:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 40, 0);
				GrStringDrawCentered(&g_sContext, "BATV:          ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 50, 0);
				sFlag.disp=true;
				bScrollPressed=false;		//clear bScrollPressed
			}
				
			DRAW_INT(sFlag.initDone, 40,17,"1");			//display the init flag as 0-1
			DRAW_INT(sFlag.GPSDone,86,17,"1");				//display the gps flag as 0-1
			DRAW_INT(GSMStats->isDataSent,40,27,"1");	//display the data flag as 0-1
			DRAW_INT(GSMStats->isSmsSent,86,27,"1");	//display the sms flag as 0-1
			DRAW_INT(timerCounter,34,37,"2");					//display the timer counter.

			DRAW_INT(GSMStats->batPercentage,80,37,"2");	//display gsm battery percentage
			DRAW_INT(GSMStats->batVoltage,40,47,"4");			//display gsm battery voltage
		break;
	
		case 2:								//if scroll is 2, display the initialization progress.
			if (bScrollPressed)	//if scroll button is pressed (i.e new diplay has to be shown)
			{
				bScrollPressed=false;		//clear bScrollPressed
				dispClear(0,95,0,63);		//clear the old display.
				dispTitle("INITILIZATIONS");	//set the title to INITILIZATIONS

				//display the headings.
				GrStringDrawCentered(&g_sContext, " UART: , LOG:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 20, 0);
				GrStringDrawCentered(&g_sContext, " HIB:  , GSM:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 30, 0);
			}
			DRAW_INT(sFlag.UART,40,17,"1");				//display the uart flag as 0-1
			DRAW_INT(sFlag.sdErrCode,80,17,"2");	//display the portG flag as 0-99
			DRAW_INT(sFlag.Hib,40,27,"1");				//display the hibernation flag as 0-1
			DRAW_INT(GSMStats->isInit,86,27,"1");	//display the gsm init flag as 0-1
		break;
	
				
		case 3:								//if scroll is 3, display the GPS progress.
			if (bScrollPressed)	//if scroll button is pressed (i.e new diplay has to be shown)
			{
				bScrollPressed=false;		//clear bScrollPressed
				dispClear(0,95,0,63);		//clear the old display.
				dispTitle("GPS");				//set the title to GPS

				//display the headings.
				GrStringDrawCentered(&g_sContext, " PWR : , FIX:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 20, 0);
				GrStringDrawCentered(&g_sContext, " INFO: , MSG:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 30, 0);
				GrStringDrawCentered(&g_sContext, "ERROR:         ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 40, 0);
			}
				
			DRAW_INT(GPSStats->isPwrOn,40,17,"1");		//display the GPS power flag as 0-1
			DRAW_INT(GPSStats->fixVal,86,17,"1");			//display the GPS fix value flag from 0-3
			DRAW_INT(GPSStats->isInfoRcvd,40,27,"1");	//display the GPS info received flag as 0-1
			DRAW_INT(sFlag.compMsg,86,27,"1");				//display the msg composed flag as 0-1
			DRAW_INT(sFlag.GPSErrCode,40,37,"4");			//display the GPS error code
		break;
				
		case 4:								//if scroll is 4, display the GSM progress.
			if (bScrollPressed)	//if scroll button is pressed (i.e new diplay has to be shown)
			{
				bScrollPressed=false;			//clear bScrollPressed
				dispClear(0,95,0,63);			//clear the old display.
				dispTitle("GSM");					//set the title to GSM

				//display the headings.
				GrStringDrawCentered(&g_sContext, " FUNC: , SIM:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 20, 0);
				GrStringDrawCentered(&g_sContext, " REG : , SGN:  ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 30, 0);
				GrStringDrawCentered(&g_sContext, "SMS ERR :      ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 40, 0);
				GrStringDrawCentered(&g_sContext, "DATA ERR:      ", -1, GrContextDpyWidthGet(&g_sContext) / 2, 50, 0);
			}
				
			DRAW_INT(GSMStats->isFullFunc,40,17,"1");		//display the GSM full func flag as 0-1
			DRAW_INT(GSMStats->isSimIns,86,17,"1");			//display the GSM sim insertion flag as 0-1
			DRAW_INT(GSMStats->isNetReg,40,27,"1");			//display the GSM network register flag as 0-1
			DRAW_INT(GSMStats->sgnlQlty,80,27,"2");			//display the GSM signal quality from 0-99
				
			DRAW_INT(sFlag.SMSErrCode,58,37,"4");							//display the SMS error code
			DRAW_INT(sFlag.DATAErrCode,58,47,"4");						//display the DATA error code
			break;
		
		case 5:
			if (bScrollPressed)	//if scroll button is pressed (i.e new diplay has to be shown)
			{
				bScrollPressed=false;				//clear bScrollPressed
				dispClear(0,95,0,63);				//clear the old display.
				dispTitle("FATAL ERROR");		//set the title to GSM

				//display the warnings.
				GrStringDrawCentered(&g_sContext, "YOU DO NOT HAVE", -1, GrContextDpyWidthGet(&g_sContext) / 2, 30, 0);
				GrStringDrawCentered(&g_sContext, "SUFFICIENT", -1, GrContextDpyWidthGet(&g_sContext) / 2, 40, 0);
				GrStringDrawCentered(&g_sContext, "BALANCE", -1, GrContextDpyWidthGet(&g_sContext) / 2, 50, 0);
			}

		}
		
}			
