	This application uses module of ADAFRUIT (FONA 808)
	and sends GPS data via text message and to a URL link.

	Application uses USER LED (on board) to verify program
	working visually and a watchdog timer is configured
	to resolve issue and reset controller on system hangs.

	A structure (statusFlag) is used to update program
	current status.This is set by flagSet() function
	which is called peiodically by scheduler utility.
	
	For transmission of data through GSM,
	It uses a library for ADAFRUIT FONA 808 GSM module
	to be used in Texas TM4C123G controllers.
	This library uses total of 5 pins:
	2 for Vcc & Gnd, 2 for Tx & Rx, and 1 for power status
	By default, it sends and receives commands through UART1
	of TIVA(Port C pin 4 and 5) and power status pin is to
	be connected with PH0.	
	It utilizes Timer0 of TM4C123G for its internal timing purposes
	All the functions have timing limitations which is 60 sec.
	and will continue to retry (in case of unsuccessful attempt)
	to accomplish the task until there is a timeout.
	All Functions return some error/status codes.
	Functions first check for any previous (required) states/errors
	and if found return that without proceeding further.
	After that success or error code due to current task is returned.
	
	All the data that is being displayed on UART0 is being
	saved on SDcard in "mainDir"/"project"/"testNo."/"time".txt
	and a brief log file is created to store the total
	success/failure transmissions along with time. This can
	help in quick overview of the reason of failure of transmission.
	Moreover, The first UART, which is connected to the USB debug virtual
	serial port on the evaluation board, is configured for 115,200 bits
	per second, and 8-N-1 mode. When the user enters "sd" on a serial monitor,
	it starts reading a file system from an SD card.It provides a
	simple command console via a serial port for issuing commands to view and
	navigate the file system on the SD card. The commands include
	cd, cat, ls,pwd, return, help.  Type ``help'' for further command help.
	It is recommended to use the "sd" feature after program finishes so that all
	the data is properly logged and closed.
	
	A brieg log is also being displayed on graphic display
	which updates itself every 2 seconds. The display includes
	battery status, and GPS/GSM flags.On scrolling left/right,
	a little detail would be shown of GPS/GSM work & error codes.
	
	After performing tasks, it has two options:
	1: To go to hibernation (for an hour), in which it can also
	be woken again by pressing wake button whose status is
	continuously checked via scheduler (systick) handler utility.
	2: to wait some time for transmission while collecting
	latest GPS data. timings has been set by wide timer 0.
	It is set to option 2.

	*************************************************************
	**************************************************************
	NOTE: set _USE_LFN to 1 in line 98 of ffconf.h for it to work.
	**************************************************************
	***************************************************************