
#ifndef GPS_FONA_h
#define GPS_FONA_h
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
	
	Written by Syed Hamza for RDS Research Lab, NUST university.
***********************************************************/

#include <stdbool.h>

//defines for GPS fix, these are the responses given by fona on GPSSTATUS command
#define GPS_LOC_UNKNOWN				0
#define GPS_NO_FIX						1
#define GPS_2D_FIX						2
#define GPS_3D_FIX						3

//error codes for GPS... These are returned by the functions in case of unsuccessful attempt
#define GPS_POWERED_OFF				1100
#define GPS_POWERED_ON				1101
#define GPS_NOT_FIXED					1102
#define GPS_INFO_NOT_RCVD			1103




//structure which store all the required status of GPS
struct GPSSTATS
{
	bool isPwrOn;		//flag to store GPS power (on or off)
	bool isFixed;		//flag to store either GPS got fix or not
	bool isInfoRcvd;//flag to store if info is received
	int fixVal;			//int type variable to store fix value
	
};
//constant pointer of struct GPSSTATS, declared so that user can READ (only) in the main application
extern struct GPSSTATS const * const GPSStats;


struct GPSDATA				//structure for storing GPS data acquired by NMEA responses.
{
	char Time[30];			//to store time given by GPS
	char Date[30];			//to store date
	char Lat[30];			//to store latitude
	char Long[30];			//to store longitude
	char Alt[30];			//to store altitude
	char Quality[13];		//to store signal quality
	char Sat[15];			//to store number of satellites from which response was generated
		char Speed[15];		//to store speed
	char Dop[15];			//to store dilusion of precision
	char Bearing[15];	//to store bearing
};
//constant pointer of struct GPSDATA, declared so that user can READ (only) in the main application
extern struct GPSDATA const * const GPSData;



//macro to declare GPSParse functions
#define GPS_P_FUNC(val)  void GPSParse##val (char GPS_TEMP_DATA[12][32],int ind)

	GPS_P_FUNC(Lat);
	GPS_P_FUNC(Long);
	GPS_P_FUNC(Speed);
#undef GPS_P_FUNC  //undefine GPS_P_FUNC


int GPSPower(void);
bool GPSPowerOneTry(void);
int getGPSFix(void);
int GPSFix(void);
int GPSGetData(int);
#endif
