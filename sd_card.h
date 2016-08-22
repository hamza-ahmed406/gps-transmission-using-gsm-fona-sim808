#ifndef sd_card_h
#define sd_card_h

/*****************************************************************************

 sd_card.h - This file was Created by Tiva and is modified for this project.
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


#include <stdint.h>
#include <stdbool.h>
#include "utils/cmdline.h"
//#include "utils/uartstdio.h"
#include "third_party/fatfs/src/ff.h"
#include "third_party/fatfs/src/diskio.h"


//*****************************************************************************
//
// Defines the size of the buffers that hold the path, or temporary data from
// the SD card.  There are two buffers allocated of this size.  The buffer size
// must be large enough to hold the longest expected full path name, including
// the file name, and a trailing null character.
//
//*****************************************************************************
#define PATH_BUF_SIZE           80

//*****************************************************************************
//
// Defines the size of the buffer that holds the command line.
//
//*****************************************************************************
#define CMD_BUF_SIZE            64

//*****************************************************************************
//
// A structure that holds a mapping between an FRESULT numerical code, and a
// string representation.  FRESULT codes are returned from the FatFs FAT file
// system driver.
//
//*****************************************************************************
typedef struct
{
    FRESULT iFResult;
    char *pcResultStr;
}
tFResultString;


//*****************************************************************************
//
// A macro to make it easy to add result codes to the table.
//
//*****************************************************************************
#define FRESULT_ENTRY(f)        { (f), (#f) }
//*****************************************************************************
//
// A macro that holds the number of result codes.
//
//*****************************************************************************
#define NUM_FRESULT_CODES       (sizeof(g_psFResultStrings) /                 \
                                 sizeof(tFResultString))






DWORD sdInit(char *mainDir, char *project, char *testDir, char * fileName);
int openFile(void);
void writeToFile(char *data);
void logData(const void * const data, char *type);
void closeFiles(int isSuccess);

const char * StringFromFResult(FRESULT iFResult);
int Cmd_ls(int argc, char *argv[]);
int Cmd_cd(int argc, char *argv[]);
int Cmd_pwd(int argc, char *argv[]);
int Cmd_cat(int argc, char *argv[]);
int Cmd_help(int argc, char *argv[]);
int sdCard(void);
void FatFsTickTimer(void*);
#endif

