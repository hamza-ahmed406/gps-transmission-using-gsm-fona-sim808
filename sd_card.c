/*****************************************************************************

 sd_card.c - This file was Created by Tiva and is modified for this project.
 SD card using FAT file system (sd_card)

	This Library stores all the data that is being displayed on UART0
	in the "mainDir"/"project"/"testno"/"time".txt file.
	another file named log.txt is included in this directory which
	stores the brief description of the of the status/errors and 
	total transmission success/failure count along with time. This can
	help in quick overview of the reason of failure of transmission.

 Moreover, The first UART, which is connected to the USB debug virtual
 serial port on the evaluation board, is configured for 115,200 bits
 per second, and 8-N-1 mode. When the user enters "sd" on a serial monitor,
 it starts reading a file system from an SD card.It provides a
 simple command console via a serial port for issuing commands to view and
 navigate the file system on the SD card. The commands include
 cd, cat, ls,pwd, return, help.  Type ``help'' for further command help.
 
supporitng files: uartconfiguration.c
NOTE: set _USE_LFN to 1 in line 98 of ffconf.h for it to work.
Editted by: Syed Hamza for RDS Research Lab, NUST university.
*****************************************************************************/
#include "sd_card.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "utils/cmdline.h"
#include "utils/uartstdio.h"
#include "fatfs/src/ff.h"
#include "fatfs/src/diskio.h"
#include "strfunc.h"
#include "inc/lcd.h"
#include "utils/ustdlib.h"
#include "inc/SysTick.h"


FIL wrFile, logFile;		//FIL type variable to store info about write file and log file
DIR wrFileDir;					// to store directory information in which out files reside
UINT bWritten, bRead;		//variables passed to f_write to F_read functions to
													//inform how many bytes are Read/Written
		
char wrFilepath[128];  	//to store complete path and file name(+extension) of write file
char logFilepath[128];		//to store complete path and file name(+extension) of write file
char filepath[128];


char succStr[8], failStr[8], totalStr[8]; //to store log data from log.txt file
int success, failure, total;	//store total number of tests, successes and failures

int fileRes;
int writeRes;
int logRes;
bool sdPeripheralEnable=false;
bool returnToMain=false;
bool dirCreated=false;

//*****************************************************************************
//
// This buffer holds the full path to the current working directory.  Initially
// it is root ("/").
//
//*****************************************************************************
static char g_pcCwdBuf[PATH_BUF_SIZE] = "/";

//*****************************************************************************
//
// A temporary data buffer used when manipulating file paths, or reading data
// from the SD card.
//
//*****************************************************************************
static char g_pcTmpBuf[PATH_BUF_SIZE];

//*****************************************************************************
//
// The buffer that holds the command line.
//
//*****************************************************************************
static char g_pcCmdBuf[CMD_BUF_SIZE];

//*****************************************************************************
//
// The following are data structures used by FatFs.
//
//*****************************************************************************
static FATFS g_sFatFs;
static DIR g_sDirObject;
static FILINFO g_sFileInfo;
static FIL g_sFileObject;

//*****************************************************************************
//
// A table that holds a mapping between the numerical FRESULT code and it's
// name as a string.  This is used for looking up error codes for printing to
// the console.
//
//*****************************************************************************
tFResultString g_psFResultStrings[] =
{
    FRESULT_ENTRY(FR_OK),
    FRESULT_ENTRY(FR_DISK_ERR),
    FRESULT_ENTRY(FR_INT_ERR),
    FRESULT_ENTRY(FR_NOT_READY),
    FRESULT_ENTRY(FR_NO_FILE),
    FRESULT_ENTRY(FR_NO_PATH),
    FRESULT_ENTRY(FR_INVALID_NAME),
    FRESULT_ENTRY(FR_DENIED),
    FRESULT_ENTRY(FR_EXIST),
    FRESULT_ENTRY(FR_INVALID_OBJECT),
    FRESULT_ENTRY(FR_WRITE_PROTECTED),
    FRESULT_ENTRY(FR_INVALID_DRIVE),
    FRESULT_ENTRY(FR_NOT_ENABLED),
    FRESULT_ENTRY(FR_NO_FILESYSTEM),
    FRESULT_ENTRY(FR_MKFS_ABORTED),
    FRESULT_ENTRY(FR_TIMEOUT),
    FRESULT_ENTRY(FR_LOCKED),
    FRESULT_ENTRY(FR_NOT_ENOUGH_CORE),
    FRESULT_ENTRY(FR_TOO_MANY_OPEN_FILES),
    FRESULT_ENTRY(FR_INVALID_PARAMETER),
};


//*****************************************************************************
//
// This function returns a string representation of an error code that was
// returned from a function call to FatFs.  It can be used for printing human
// readable error messages.
//
//*****************************************************************************
const char *
StringFromFResult(FRESULT iFResult)
{
    uint_fast8_t ui8Idx;

    //
    // Enter a loop to search the error code table for a matching error code.
    //
    for(ui8Idx = 0; ui8Idx < NUM_FRESULT_CODES; ui8Idx++)
    {
        //
        // If a match is found, then return the string name of the error code.
        //
        if(g_psFResultStrings[ui8Idx].iFResult == iFResult)
        {
            return(g_psFResultStrings[ui8Idx].pcResultStr);
        }
    }

    //
    // At this point no matching code was found, so return a string indicating
    // an unknown error.
    //
    return("UNKNOWN ERROR CODE");
}

//*****************************************************************************
//
// This is the handler for this SysTick interrupt.  FatFs requires a timer tick
// every 10 ms for internal timing purposes.
//
//*****************************************************************************

void
FatFsTickTimer(void *pvparam)
{
    //
    // Call the FatFs tick timer.
    //
    disk_timerproc();
}

//*****************************************************************************
//
// This function implements the "ls" command.  It opens the current directory
// and enumerates through the contents, and prints a line for each item it
// finds.  It shows details such as file attributes, time and date, and the
// file size, along with the name.  It shows a summary of file sizes at the end
// along with free space.
//
//*****************************************************************************
int
Cmd_ls(int argc, char *argv[])
{
    uint32_t ui32TotalSize;
    uint32_t ui32FileCount;
    uint32_t ui32DirCount;
    FRESULT iFResult;
    FATFS *psFatFs;
    char *pcFileName;
#if _USE_LFN
    char pucLfn[_MAX_LFN + 1];
    g_sFileInfo.lfname = pucLfn;
    g_sFileInfo.lfsize = sizeof(pucLfn);
#endif


    //
    // Open the current directory for access.
    //
    iFResult = f_opendir(&g_sDirObject, g_pcCwdBuf);

    //
    // Check for error and return if there is a problem.
    //
    if(iFResult != FR_OK)
    {
        return((int)iFResult);
    }

    ui32TotalSize = 0;
    ui32FileCount = 0;
    ui32DirCount = 0;

    //
    // Give an extra blank line before the listing.
    //
    UARTprintf("\n");

    //
    // Enter loop to enumerate through all directory entries.
    //
    for(;;)
    {
        //
        // Read an entry from the directory.
        //
        iFResult = f_readdir(&g_sDirObject, &g_sFileInfo);

        //
        // Check for error and return if there is a problem.
        //
        if(iFResult != FR_OK)
        {
            return((int)iFResult);
        }

        //
        // If the file name is blank, then this is the end of the listing.
        //
        if(!g_sFileInfo.fname[0])
        {
            break;
        }

        //
        // If the attribue is directory, then increment the directory count.
        //
        if(g_sFileInfo.fattrib & AM_DIR)
        {
            ui32DirCount++;
        }

        //
        // Otherwise, it is a file.  Increment the file count, and add in the
        // file size to the total.
        //
        else
        {
            ui32FileCount++;
            ui32TotalSize += g_sFileInfo.fsize;
        }

#if _USE_LFN
        pcFileName = ((*g_sFileInfo.lfname)?g_sFileInfo.lfname:g_sFileInfo.fname);
#else
        pcFileName = g_sFileInfo.fname;
#endif
        //
        // Print the entry information on a single line with formatting to show
        // the attributes, date, time, size, and name.
        //
        UARTprintf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9u  %s\n",
                   (g_sFileInfo.fattrib & AM_DIR) ? 'D' : '-',
                   (g_sFileInfo.fattrib & AM_RDO) ? 'R' : '-',
                   (g_sFileInfo.fattrib & AM_HID) ? 'H' : '-',
                   (g_sFileInfo.fattrib & AM_SYS) ? 'S' : '-',
                   (g_sFileInfo.fattrib & AM_ARC) ? 'A' : '-',
                   (g_sFileInfo.fdate >> 9) + 1980,
                   (g_sFileInfo.fdate >> 5) & 15,
                   g_sFileInfo.fdate & 31,
                   (g_sFileInfo.ftime >> 11),
                   (g_sFileInfo.ftime >> 5) & 63,
                   g_sFileInfo.fsize,
                   pcFileName);
    }

    //
    // Print summary lines showing the file, dir, and size totals.
    //
    UARTprintf("\n%4u File(s),%10u bytes total\n%4u Dir(s)",
                ui32FileCount, ui32TotalSize, ui32DirCount);

    //
    // Get the free space.
    //
    iFResult = f_getfree("/", (DWORD *)&ui32TotalSize, &psFatFs);

    //
    // Check for error and return if there is a problem.
    //
    if(iFResult != FR_OK)
    {
        return((int)iFResult);
    }

    //
    // Display the amount of free space that was calculated.
    //
    UARTprintf(", %10uK bytes free\n", (ui32TotalSize *
                                        psFatFs->free_clust / 2));

    //
    // Made it to here, return with no errors.
    //
    return(0);
}

//*****************************************************************************
//
// This function implements the "cd" command.  It takes an argument that
// specifies the directory to make the current working directory.  Path
// separators must use a forward slash "/".  The argument to cd can be one of
// the following:
//
// * root ("/")
// * a fully specified path ("/my/path/to/mydir")
// * a single directory name that is in the current directory ("mydir")
// * parent directory ("..")
//
// It does not understand relative paths, so dont try something like this:
// ("../my/new/path")
//
// Once the new directory is specified, it attempts to open the directory to
// make sure it exists.  If the new path is opened successfully, then the
// current working directory (cwd) is changed to the new path.
//
//*****************************************************************************
int
Cmd_cd(int argc, char *argv[])
{
    uint_fast8_t ui8Idx;
    FRESULT iFResult;

    //
    // Copy the current working path into a temporary buffer so it can be
    // manipulated.
    //
    strcpy(g_pcTmpBuf, g_pcCwdBuf);

    //
    // If the first character is /, then this is a fully specified path, and it
    // should just be used as-is.
    //
    if(argv[1][0] == '/')
    {
        //
        // Make sure the new path is not bigger than the cwd buffer.
        //
        if(strlen(argv[1]) + 1 > sizeof(g_pcCwdBuf))
        {
            UARTprintf("Resulting path name is too long\n");
            return(0);
        }

        //
        // If the new path name (in argv[1])  is not too long, then copy it
        // into the temporary buffer so it can be checked.
        //
        else
        {
            strncpy(g_pcTmpBuf, argv[1], sizeof(g_pcTmpBuf));
        }
    }

    //
    // If the argument is .. then attempt to remove the lowest level on the
    // CWD.
    //
    else if(!strcmp(argv[1], ".."))
    {
        //
        // Get the index to the last character in the current path.
        //
        ui8Idx = strlen(g_pcTmpBuf) - 1;

        //
        // Back up from the end of the path name until a separator (/) is
        // found, or until we bump up to the start of the path.
        //
        while((g_pcTmpBuf[ui8Idx] != '/') && (ui8Idx > 1))
        {
            //
            // Back up one character.
            //
            ui8Idx--;
        }

        //
        // Now we are either at the lowest level separator in the current path,
        // or at the beginning of the string (root).  So set the new end of
        // string here, effectively removing that last part of the path.
        //
        g_pcTmpBuf[ui8Idx] = 0;
    }

    //
    // Otherwise this is just a normal path name from the current directory,
    // and it needs to be appended to the current path.
    //
    else
    {
        //
        // Test to make sure that when the new additional path is added on to
        // the current path, there is room in the buffer for the full new path.
        // It needs to include a new separator, and a trailing null character.
        //
        if(strlen(g_pcTmpBuf) + strlen(argv[1]) + 1 + 1 > sizeof(g_pcCwdBuf))
        {
            UARTprintf("Resulting path name is too long\n");
            return(0);
        }

        //
        // The new path is okay, so add the separator and then append the new
        // directory to the path.
        //
        else
        {
            //
            // If not already at the root level, then append a /
            //
            if(strcmp(g_pcTmpBuf, "/"))
            {
                strcat(g_pcTmpBuf, "/");
            }

            //
            // Append the new directory to the path.
            //
            strcat(g_pcTmpBuf, argv[1]);
        }
    }

    //
    // At this point, a candidate new directory path is in chTmpBuf.  Try to
    // open it to make sure it is valid.
    //
    iFResult = f_opendir(&g_sDirObject, g_pcTmpBuf);

    //
    // If it can't be opened, then it is a bad path.  Inform the user and
    // return.
    //
    if(iFResult != FR_OK)
    {
        UARTprintf("cd: %s\n", g_pcTmpBuf);
        return((int)iFResult);
    }

    //
    // Otherwise, it is a valid new path, so copy it into the CWD.
    //
    else
    {
        strncpy(g_pcCwdBuf, g_pcTmpBuf, sizeof(g_pcCwdBuf));
    }

    //
    // Return success.
    //
    return(0);
}

//*****************************************************************************
//
// This function implements the "pwd" command.  It simply prints the current
// working directory.
//
//*****************************************************************************
int
Cmd_pwd(int argc, char *argv[])
{
    //
    // Print the CWD to the console.
    //
    UARTprintf("%s\n", g_pcCwdBuf);

    //
    // Return success.
    //
    return(0);
}

//*****************************************************************************
//
// This function implements the "cat" command.  It reads the contents of a file
// and prints it to the console.  This should only be used on text files.  If
// it is used on a binary file, then a bunch of garbage is likely to printed on
// the console.
//
//*****************************************************************************
int
Cmd_cat(int argc, char *argv[])
{
    FRESULT iFResult;
    uint32_t ui32BytesRead;

    //
    // First, check to make sure that the current path (CWD), plus the file
    // name, plus a separator and trailing null, will all fit in the temporary
    // buffer that will be used to hold the file name.  The file name must be
    // fully specified, with path, to FatFs.
    //
    if(strlen(g_pcCwdBuf) + strlen(argv[1]) + 1 + 1 > sizeof(g_pcTmpBuf))
    {
        UARTprintf("Resulting path name is too long\n");
        return(0);
    }

    //
    // Copy the current path to the temporary buffer so it can be manipulated.
    //
    strcpy(g_pcTmpBuf, g_pcCwdBuf);

    //
    // If not already at the root level, then append a separator.
    //
    if(strcmp("/", g_pcCwdBuf))
    {
        strcat(g_pcTmpBuf, "/");
    }

    //
    // Now finally, append the file name to result in a fully specified file.
    //
    strcat(g_pcTmpBuf, argv[1]);

    //
    // Open the file for reading.
    //
    iFResult = f_open(&g_sFileObject, g_pcTmpBuf, FA_READ);

    //
    // If there was some problem opening the file, then return an error.
    //
    if(iFResult != FR_OK)
    {
        return((int)iFResult);
    }

    //
    // Enter a loop to repeatedly read data from the file and display it, until
    // the end of the file is reached.
    //
    do
    {
        //
        // Read a block of data from the file.  Read as much as can fit in the
        // temporary buffer, including a space for the trailing null.
        //
        iFResult = f_read(&g_sFileObject, g_pcTmpBuf, sizeof(g_pcTmpBuf) - 1,
                         &ui32BytesRead);

        //
        // If there was an error reading, then print a newline and return the
        // error to the user.
        //
        if(iFResult != FR_OK)
        {
            UARTprintf("\n");
            return((int)iFResult);
        }

        //
        // Null terminate the last block that was read to make it a null
        // terminated string that can be used with printf.
        //
        g_pcTmpBuf[ui32BytesRead] = 0;

        //
        // Print the last chunk of the file that was received.
        //
        UARTprintf("%s", g_pcTmpBuf);
    }
    while(ui32BytesRead == sizeof(g_pcTmpBuf) - 1);

    //
    // Return success.
    //
    return(0);
}

//*****************************************************************************
//
// This function implements the "help" command.  It prints a simple list of the
// available commands with a brief description.
//
//*****************************************************************************
int
Cmd_help(int argc, char *argv[])
{
    tCmdLineEntry *psEntry;

    //
    // Print some header text.
    //
    UARTprintf("\nAvailable commands\n");
    UARTprintf("------------------\n");

    //
    // Point at the beginning of the command table.
    //
    psEntry = &g_psCmdTable[0];

    //
    // Enter a loop to read each entry from the command table.  The end of the
    // table has been reached when the command name is NULL.
    //
    while(psEntry->pcCmd)
    {
        //
        // Print the command name and the brief description.
        //
        UARTprintf("%6s: %s\n", psEntry->pcCmd, psEntry->pcHelp);

        //
        // Advance to the next entry in the table.
        //
        psEntry++;
    }

    //
    // Return success.
    //
    return(0);
}

int Cmd_return(int argc, char *argv[])
{
	returnToMain=true;
//	if (ROM_WatchdogRunning(WATCHDOG0_BASE))
//		ROM_WatchdogResetEnable(WATCHDOG0_BASE);

	return 0;
}

//*****************************************************************************
//
// This is the table that holds the command names, implementing functions, and
// brief description.
//
//*****************************************************************************
tCmdLineEntry g_psCmdTable[] =
{
    { "help",   Cmd_help,   "Display list of commands" },
    { "h",      Cmd_help,   "alias for help" },
    { "?",      Cmd_help,   "alias for help" },
    { "ls",     Cmd_ls,     "Display list of files" },
    { "chdir",  Cmd_cd,     "Change directory" },
    { "cd",     Cmd_cd,     "alias for chdir" },
    { "pwd",    Cmd_pwd,    "Show current working directory" },
    { "cat",    Cmd_cat,    "Show contents of a text file" },
		{ "return", Cmd_return, "Return To the Main process" },
		{ "ret", 		Cmd_return, "Alias for return" },
    { 0, 0, 0 }
};


//*****************************************************************************
//
// The program main function.  It performs initialization, then runs a command
// processing loop to read commands from the console.
//
//*****************************************************************************
int
sdCard(void)
{
	//if watchDog is enabled, disable its reset functionality.
//	if (ROM_WatchdogRunning(WATCHDOG0_BASE)){
//		ROM_WatchdogResetDisable(WATCHDOG0_BASE);
//	}
    int nStatus;
	
		returnToMain=false;
    UARTprintf("\n\nYou Have Entered SD Card Section Of Program\n");
    UARTprintf("Type \'help\' for help.\n");

    //
    // Enter in a loop for reading and processing commands from the
    // user.
    //
    while(!returnToMain)
    {
        //
        // Print a prompt to the console.  Show the CWD.
        //
        UARTprintf("\n%s> ", g_pcCwdBuf);

        //
        // Get a line of text from the user.
        //
        UARTgets(g_pcCmdBuf, sizeof(g_pcCmdBuf));

        //
        // Pass the line from the user to the command processor.  It will be
        // parsed and valid commands executed.
        //
        nStatus = CmdLineProcess(g_pcCmdBuf);

        //
        // Handle the case of bad command.
        //
        if(nStatus == CMDLINE_BAD_CMD)
        {
            UARTprintf("Bad command!\n");
        }

        //
        // Handle the case of too many arguments.
        //
        else if(nStatus == CMDLINE_TOO_MANY_ARGS)
        {
            UARTprintf("Too many arguments for command processor!\n");
        }

        //
        // Otherwise the command was executed.  Print the error code if one was
        // returned.
        //
        else if(nStatus != 0)
        {
            UARTprintf("Command returned error code %s\n",
                        StringFromFResult((FRESULT)nStatus));
        }
    }
		return true;
}



DWORD sdInit(char *mainDir, char *project, char *testDir, char *fileName)
{
	char createTime[32]="\0";
	strcpy(createTime,fileName);
		FRESULT iFResult=FR_OK;
	
	if (!sdPeripheralEnable)
	{
    //
    // Enable the peripherals used by this example.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);

		// Mount the file system, using logical disk 0.
    //
    iFResult = f_mount(0, &g_sFatFs);
    if(iFResult != FR_OK)
    {
        UARTprintf("f_mount error: %s\n", StringFromFResult(iFResult));
        return(1);
    }
		sdPeripheralEnable=true;
	}

	if (!dirCreated)
	{
		f_mkdir(mainDir);									//Make main project directory if non-existing
		strcpy(filepath,mainDir);				//copy the mainDir to filepath
		stringAppend(filepath, "/");		//append a '/' for sub-folders
		stringAppend(filepath,project);	//copy the project name to filepath
		f_mkdir(filepath);							//Make current repository directory if non-existing
		stringAppend(filepath,"/");			//append a '/' for sub-folders
		stringAppend(filepath, testDir);//copy the testDir to filepath
		f_mkdir(filepath);							//Make test no.folder directory if non-existing

		if ( f_opendir(&wrFileDir,filepath) ==FR_OK)	//open this directory for further process
			dirCreated=true;
		
		stringAppend(filepath,"/");			//append a '/' for keeping files inside
	}

		strcpy(wrFilepath,filepath);			//copy filepath to writeFile path
		stringAppend(wrFilepath,fileName);//make a complete directory for wrFile.
		stringAppend(wrFilepath, ".txt");	//append extension with name
		UARTprintf(wrFilepath);						//display the current working file for user ease.
		writeRes = openFile();						//open the Files.
		fileRes=writeRes* 10;

		strcpy(logFilepath,filepath);		//copy filepath to logFile path (both reside in same directory)
		stringAppend(logFilepath,"log.txt");			//make a complete directory for logFile.
	
		logRes= f_open(&logFile,logFilepath,FA_OPEN_ALWAYS | FA_WRITE |FA_READ);	//open log file R/W access.
		fileRes+= logRes;
		if (logRes)
			return fileRes;
		
		if (logFile.fsize==0) //if log file is empty (at the start of testing)
		{
			f_write(&logFile,"Success: 00000\r\n", 16,&bWritten);	//write success=0, failure=0 and total=1
			f_write(&logFile,"Failure: 00000\r\n", 16, &bWritten);
			f_write(&logFile,"Total  : 00001\r\n",16,&bWritten);
			f_write(&logFile, "\r\nTime\t\t\tINIT\tGPS\tSMS\tDATA\r\n",27,&bWritten);	//table heading
		
			success=0;																									//setting respective values
			failure=0; 
			total=1;
		}
		
		else																//else if file has already been created (used)
		{
			f_lseek(&logFile,9);							//move 9 spaces from start of file 
			f_read(&logFile,succStr,5,&bRead);//to read success values (5 bytes data)
			success=atoi(succStr);						//convert to number and store it
			
			f_lseek(&logFile,(logFile.fptr+11));	//move 11 spaces forward
			f_read(&logFile,failStr,5,&bRead);		//to read failure values (5 bytes data)
			failure=atoi(failStr);								//convert to int type ad store it
			
			f_lseek(&logFile,(logFile.fptr+11));	//move 11 spaces forward
			f_read(&logFile,totalStr,5,&bRead);		//to read total number of tries values (5 bytes data)
			total=atoi(totalStr);									//convert to int type ad store it
			
			total++;															//increment total
			usprintf(totalStr,"%05d",total);			//convert it to string
			f_lseek(&logFile,logFile.fptr-5);			//and write new value of total to indicate the
			
			f_write(&logFile,totalStr,stringLength(totalStr),&bWritten);	// beginning of new test
			
			f_sync(&logFile);											//flush the data into file to avoid data loss
			f_lseek(&logFile,logFile.fsize);			//move to the end of the file to write new log	
		}
		
		f_write(&logFile,"\r\n",2,&bWritten);		//enter a new line
		logData(createTime, "%c");							//enter the time of logging data
		
		return fileRes;																//return successful
}


//opens the write file with R/W mode and display any error if occured
//All the data that is being displayed to user is also gonna
//get stored in this file.
int openFile()
{
	int res =	f_open(&wrFile,wrFilepath,FA_OPEN_ALWAYS | FA_WRITE |FA_READ); //opens writeFile
	char errorStr[8]="\0";
	usprintf(errorStr, "%d", res);
	UARTprintf("\r\n\nFILE error ==", "\n\n\n\n\r");
	UARTprintf(errorStr);

	return res;
}


//write to file. This is going to be used by the application to 
//write the displayed data to the wrFile.
void writeToFile(char *data)
{
	if (writeRes)
		return;

	f_write(&wrFile,data, ustrlen(data),&bWritten);	//write data
	f_sync(&wrFile);										//flush cached data to avoid data loss
}



//log the data. it will log the data passed to constant void pointer:
//it supports 3 data types passed by char* type:
// "%c" to for string types, "%i" for int, and "%b" for bool data type
void logData(const void * const data, char *type) 
{
	if (logRes)
		return;
	delay_ms(100);
	char cptr[16]="\0";			//to store the converted pointer.
	if (strcmp(type,"%c")==0)		//if it is a string type
	{														//type cast  *data to char * and write as it is.
			f_write(&logFile,(char *) data,stringLength((char *) data),&bWritten);
	}
	
	else if (strcmp(type,"%i")==0) //if it is a int type
	{															//type cast  *data to int * and then convert it to string
		usprintf(cptr,type,*((int *) data));
		f_write(&logFile,cptr,stringLength(cptr),&bWritten);	//write the converted string
	}
	
	else if (strcmp(type,"%b")==0)		 //if it is a bool type
	{																	//type cast  *data to int * and then convert it to string
		usprintf(cptr,"%i", *((bool *) data));
		cptr[2]=0;
		f_write(&logFile,cptr,stringLength(cptr),&bWritten);//write the converted string
	}
	
	f_write(&logFile,"\t",1,&bWritten);//write a tab
	f_sync(&logFile);									//flush cashed data to avoid data loss

}


//closes all the files opened currently.
//logs the data and increments success/failure
//passed by the application
void closeFiles(int isSuccess)
{
	char errorStr[4];		//to display error codes
	int Fres;						//to store error codes

	if (!logRes)
	{
		if (isSuccess==1)			//if transmission is successful
		{
			f_lseek(&logFile,9);			//seek 9 places from start of file
			success++;								//increment success,
			usprintf(succStr,"%05d",success);//convert it to string
			f_write(&logFile,succStr, ustrlen(succStr),&bWritten);// and update it in log.
		}
		else if (isSuccess==0)							//else
		{
			failure++;												//increment failure,
			f_lseek(&logFile,25);   				 	//seek 25 places from start of file
			usprintf(failStr,"%05d",failure);	//convert it to string
			f_write(&logFile,failStr, ustrlen(failStr),&bWritten);// and update it in log.
		}
		f_sync(&logFile);
		Fres=f_close(&logFile);					//close log file, and store error code
	
		usprintf(errorStr, "%d", Fres);			//convert it into string	
		UARTprintf("\r\n\nlog closed ==", "\n\n\n\n\r");//and display it.
		UARTprintf(errorStr);
	}	
	
	if (!writeRes)
	{
		Fres=f_close(&wrFile);														//close write file, and store error code
		usprintf(errorStr, "%d", Fres);										//convert it into string	
		UARTprintf("\r\n\nfile closed ==", "\n\n\n\n\r");	//and display it.
		UARTprintf(errorStr);	
	}
	
}
