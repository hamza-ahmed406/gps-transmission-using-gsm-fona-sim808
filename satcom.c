//#include <stdio.h>
//#include <stdint.h>
//#include <stdlib.h>
//#include <math.h>
//#include "inc/tm4c123gh6pge.h"
//#include "inc/SysTick.h"
//#include "inc/lcd.h"
//#include "inc/uart.h"
//#include "inc/gps.h"
//#define SATCOM_UART			3

//int s;
//int Checkf;
//int SatChecksum;
//char chSatChecksum[2];
//void int2char(char str[], int dcm)
//{
//	int i;
//	int hex[2];
//	hex[0]=dcm/16;
//	hex[1]=dcm%16;
//	for (i=0; i<2; i++)
//	{
//		if(hex[i]<10)
//			str[i]=hex[i]+48;
//		else
//			str[i]=hex[i]+55;
//	}
//}
//		
//	
//int main()
//{
//			s=0;
//	//Place this at the very start of the final program after merging
//	char msg[800]="GPS Data\n\nTime: 123519\nLat: 48.11730\nLong: 11.51666\nAltitude: 545.4M\nSig Qual: 1\nSat Num: 08\nDOP: 0.9\0";
//	char OkSignal[10];
//			
//	while(1)
//		{
//				
//				SatChecksum=msg[0];
//			for(Checkf=0;msg[Checkf+1]!='\0';Checkf++)
//		{
//			SatChecksum=SatChecksum^msg[Checkf+1];
//		}
//		int2char(chSatChecksum, SatChecksum);
//	
//		
//		switch(s)
//		{
//			case '0':
//			{
////				if(!isdataready())
////				{
////					break;
////					}
//		UARTn_TxStr(SATCOM_UART,"AT+SBDWB=103");
//			s++;
//			break;	
//			}
//			case '1':
//			{
//				int g;
//		for(g=0;g<5;g++)        
//		{
//			OkSignal[g]=UARTn_RxCh(SATCOM_UART);
//		
//		}
//		
//			if(strcmp(OkSignal,"READY")==0)
//		{
//			s++;

//		}
//		else

//		{
//			lcd_puts("wrong command returned at s=1 stage");
//			s=0;
//		}
//break;
//	}
//	
//	case '2':
//	{
//		UARTn_TxStr(SATCOM_UART,msg);
//		s++;
//		break;
//	}
//	
//	case '3':
//	{
//		if(UARTn_RxCh(SATCOM_UART)=='\0')
//		{
//			s++;
//		}
//		
//	else
//	{
//		lcd_puts("error in sending message in MO buffer");
//		s=2;
//		
//	}
//break;
//}
//case '4':
//{
//	
//	
//	UARTn_TxStr(SATCOM_UART,"AT+SBDI");
//	s++;
//break;
//}
//case '5':
//{
//	//do parsing of response from satcom and display on lcd
//	s++;
//break;
//}
//case '6':
//{

//	UARTn_TxStr(SATCOM_UART,"AT+SBDD0");
//	

//	if(UARTn_RxCh(SATCOM_UART)=='\0')
//	{
//		lcd_puts("buffer cleared successfully");
//	}
//	else
//	{
//lcd_puts("buffer not cleared successfully");		
//	}
//	s=0;
//break;
//}
//default:
//{
//	lcd_puts("Error in sending message,repeat the process again");
//s=0;
//}
//}
//}
//}

