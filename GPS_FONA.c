#include "GSM_FONA808.h"
#include "GPS_FONA.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "utils/ustdlib.h"
#include "utils/uartstdio.h"
#include "driverlib/rom.h"
#include "inc/tm4c123gh6pge.h"
#include "inc/SysTick.h"
#include "strfunc.h"
#include "uartConfiguration.h"

		
struct GPSDATA data; //create GPSDATA object for this .c file
struct GPSDATA const * const GPSData=&data;	//create constant pointer so that Apllication can (only)read the data

struct GPSSTATS gstats; //create GPSSTATS object for this .c file
struct GPSSTATS const * const GPSStats=&gstats;//create constant pointer so that Apllication can (only)read the data

int GPSPower()	//turning GPS on
{
	if(!GSMStats->isInit) //checking for any previous error codes and (if so,) returning those codes
		return GSM_NOT_INIT;
	
	GSMTimerEnable(); //Enable timer (one minute) 
	gstats.isPwrOn= GPSPowerOneTry(); //checking for GPS and storing the response in isPwrOn
	while (!gstats.isPwrOn && !GSMStats->isTimeout) //trying until its on or timeout has occured
	{
		delay_ms(2000);												//delay for some time for 2nd attempt (for not to send continuously and overflowing fona)
		TO_PC("GPS couldnt be turned on, trying again\r\n");
		gstats.isPwrOn=GPSPowerOneTry(); //retry
	}
	if (gstats.isPwrOn) //return success code if GPS is powered on, else return error code
	{
		TO_PC("GPS is ON\r\n\0");
		return GSM_ACT_OK;
	}
	else
		return GPS_POWERED_OFF;
}

bool GPSPowerOneTry()		
{
	responseRead(UART1_BASE);
	TO_GSM("AT+CGPSPWR?\r\n");//command to check GPS power
	if ( waitAndCheckResp("\r\n+CGPSPWR:") ) 													//Wait and check for the proper response.
	{
		if ( ustrstr(UARTResponse[1],"\r\n+CGPSPWR: 0\r\n\r\nOK\r\n") ) //response if GPS is off
		{
			responseRead(UART1_BASE);
		 
			TO_GSM("AT+CGPSPWR=1\r\n"); //command for turning it on
			return (waitAndCheckResp("\r\nOK\r\n") ); //return true if OK is received, else return false
		}
			else 
				return true;
	}
	 else
		 return false; 
}


int GPSFix() //for GPS signal info
{
	if (!GSMStats->isInit)
		return GSM_NOT_INIT;
	if (!gstats.isPwrOn)
		return GPS_POWERED_OFF;
	
	GSMTimerEnable();
	gstats.fixVal=getGPSFix();//checking GPS fix and store the fix value
	
	while (gstats.fixVal<GPS_NO_FIX && !GSMStats->isTimeout) //trying until it gets atleast GPS_NOT_FIX, ot timeout has occured.
	{																												//NOTE: it actually should be GPS2DFix, but its altered for testing phase 
		if (gstats.fixVal==GPS_LOC_UNKNOWN)
			GPSPower();
		TO_PC("GPS didn't get fix,trying again\r\n"); //notify the user
		delay_ms(2000);
		gstats.fixVal=getGPSFix();								//retry and store the fix value
	}
	if (gstats.fixVal>=GPS_NO_FIX) //set the isFixed flag if fix is received and return success code (NOTE:should be ">GPS_NOT_FIX")
	{
				gstats.isFixed=true;
		TO_PC("GPS got fix\r\n\0");
		return GSM_ACT_OK;
	}														//else return the error code
	else
	{
		gstats.isFixed=false;
		return GPS_NOT_FIXED;
	}
}

int getGPSFix()
{
	responseRead(UART1_BASE);
	TO_GSM("AT+CGPSSTATUS?\r\n"); //command for checking gps fix
	if ( waitAndCheckResp("+CGPSSTATUS:") )
	{
	if  (ustrstr(UARTResponse[1], "Location Unknown") ) //response in case its off
		return GPS_LOC_UNKNOWN;
	if  (ustrstr(UARTResponse[1], "Location Not Fix") ) //response if there os no fix
		return GPS_NO_FIX;
	else if (ustrstr(UARTResponse[1], "Location 2D Fix") ) //if there is 2Dfix
		return GPS_2D_FIX;
	else if (ustrstr(UARTResponse[1],"Location 3D Fix") ) //if there is 3Dfix
		return GPS_3D_FIX;
	else
		return GPS_NO_FIX;
	}
	else
		return GPS_LOC_UNKNOWN;
}


void fuseDataGPS(char GPS_TEMP_DATA[12][32],int mode, char *dataBuffer) //fusing data
{
	int dataRow=0, dataCol=0;
	for (int i=12; dataBuffer[i]!='\r' && dataBuffer[i]!='\0'; i++) //continuing till \r or \0
	{																																// shift to new row, if there is comma or
		if (dataBuffer[i]==',' || dataCol>=30 || (mode==0 && dataRow==4 && dataCol>7))//array limit exceeds or  
		{																															// date and time has to be separated.
			GPS_TEMP_DATA[dataRow][dataCol]=0;										//insert a null-character
			dataRow++;																						//shift to new row
			dataCol=0;
			if (dataBuffer[i]!=',')							//if date has ended, move time value to new row
				GPS_TEMP_DATA[dataRow][dataCol++]=dataBuffer[i];
		}
			
		else
			GPS_TEMP_DATA[dataRow][dataCol++]=dataBuffer[i];
	}
		GPS_TEMP_DATA[dataRow][dataCol]=0;
	
}


void GPSParseLat(char GPS_TEMP_DATA[12][32],int ind) //Parse latitude function
{
	float			res_fLatitude=0;
	float degrees=0,minutes=0;
	res_fLatitude = string2float(GPS_TEMP_DATA[ind]);   //convert to float
	degrees = truncate(res_fLatitude / 100.0f); 				//convert the latitudes to degress (originally was ddMM.xxx)
	minutes = res_fLatitude - (degrees * 100.0f);
	res_fLatitude = degrees + minutes / 60.0f;
	float2str(data.Lat,res_fLatitude);				//convert back to string and store it in GPSLat
}

void GPSParseLong(char GPS_TEMP_DATA[12][32],int ind)//Parse longitude function
{
	float			res_fLongitude=0;
	float degrees=0,minutes=0;
	res_fLongitude = string2float(GPS_TEMP_DATA[ind]);//convert to float
		// get decimal format

		degrees = truncate(res_fLongitude / 100.0f);				////
		minutes = res_fLongitude - (degrees * 100.0f); 			//converting to degrees
		res_fLongitude = degrees + minutes / 60.0f;					////
		float2str(data.Long,res_fLongitude);		//convert back to string and store it in GPSLat
}
	
void GPSParseSpeed(char GPS_DATA[12][32],int ind) //parse speed
{
		float		res_fSpeed=0;													//parse speed
	res_fSpeed = ustrtof(GPS_DATA[8], NULL); //converting to double
		res_fSpeed /= 1.852f; // convert to km/h	(from knots)
		sprintf(data.Speed,"%f",res_fSpeed); //convert back to string and store it in GPSSpeed

}


void parseDataGPS(char GPS_TEMP_DATA[12][32],int mode) //parse the data received
{

	if (mode==0) //parse if mode=0
	{
		GPSParseLong(GPS_TEMP_DATA,1);
		GPSParseLat(GPS_TEMP_DATA,2);
		strcpy(data.Alt,GPS_TEMP_DATA[3]);
		strcpy(data.Date,GPS_TEMP_DATA[4]);
		strcpy(data.Time,GPS_TEMP_DATA[5]);
		strcpy(data.Sat,GPS_TEMP_DATA[7]);		
		GPSParseSpeed(GPS_TEMP_DATA,8);
		strcpy(data.Bearing,GPS_TEMP_DATA[9]);
		
	}
}


int GPSSendInf(int mode, char ** buff) //for data request
{
	gstats.isInfoRcvd=false;	//clearing infoRcvd flag
	if (!GSMStats->isInit)			//check for previous errors and return those error codes
		return GSM_NOT_INIT;
	if (!gstats.isPwrOn)
		return GPS_POWERED_OFF;
	if (!gstats.isFixed)
		return GPS_NOT_FIXED;
	
	GSMTimerEnable();		//enabling the timer
	do									//while info  is not received or timeout has occured
	{
		responseRead(UART1_BASE);
		if (mode==0)
			TO_GSM("AT+CGPSINF=0\r\n");//command for data request with mode 0
		*buff= waitAndCheckResp("\r\n+CGPSINF: ");		//wait and check for proper response
		gstats.isInfoRcvd = *buff;
		delay_ms(400);
		if (!gstats.isInfoRcvd)
			delay_ms(2000);
	}
	while (!gstats.isInfoRcvd && !GSMStats->isTimeout); //response is shown below

	GSMTimerDisable();
	if (gstats.isInfoRcvd) //set flag if response is received and return success code
		return GSM_ACT_OK;
	
	else																								//else send error code
		return GPS_INFO_NOT_RCVD;
}
/*
response of CGPSINf=0 command:

+CGPSINF: <mode>,<longitude>,<latitude>,<altitude>,<UTC time>,<TTFF>,<num>,<speed>,<course>

OK

where:
mode=0 in our case
longitude, latitude and altitudes are sent in ddmmss.xxx  (d=degrees,m=minutes, s=sec)
<utc time>: contains time and date info in form of yyyymmddHHMMSS.xxxx (y=year, m=month,d=days,H=hour,M=minute,S=seconds)
TTFF=time to first fix, not relavant to us
num= number of satellites in fix
speed=speed over ground
course=course over ground  (bearing)
*/

int GPSGetData(int mode)
{
	gstats.isInfoRcvd=false;
	if (!gstats.isPwrOn)
		return GPS_POWERED_OFF;
	if (!gstats.isFixed)
		return GPS_NOT_FIXED;
	
	int infoRes=0;							//to store info error code
	char GPSTempData[12][32]={"\0","\0","\0","\0","\0","\0","\0","\0","\0","\0","\0","\0"}; //for data storage
	char *buffer=NULL;

	infoRes=GPSSendInf(mode, &buffer); //sending attention command and storing response in infoRes
	
	if (infoRes==GSM_ACT_OK) //continue if info is received and return error code
	{
		ROM_IntDisable(INT_UART1); //enable interrupt
		fuseDataGPS(GPSTempData,mode,buffer); //fuse data into rows and columns on the bais of comma
		ROM_IntEnable(INT_UART1); //enable interrupt
		parseDataGPS(GPSTempData,mode); //parse data i.e lat, long, alt, time and speed
	}
	responseRead(UART1_BASE);
	return infoRes;
}
