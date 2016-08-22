////=================================
///	Code by: Muneeb Abid
//	Dated: 25 Mar 2015
///	Details: GPS NMEA Functions
////=================================
#include <stdint.h>
#define USR_LED				(*((volatile uint32_t *)0x40026010))


void GPS_WaitForFixData(int UARTn){
	while(1){
		while(UARTn_RxCh(UARTn)!='G'){}
		if(UARTn_RxCh(UARTn)!='P'){
			if(UARTn_RxCh(UARTn)!='G'){
				if(UARTn_RxCh(UARTn)!='G'){
					if(UARTn_RxCh(UARTn)!='A'){
						if(UARTn_RxCh(UARTn)!=','){
								return;
						}
					}
				}
			}
		}
	}
}



void GPS_GetParam(int UARTn, char* str, int NoOfParametersToReturn){
	int i = 0;
	int n = 0;
	char c;
	for(n = 0; n < NoOfParametersToReturn; n++){
		while((c = UARTn_RxCh(UARTn))!=','){
			str[i]=c;
			i++;
		}
	}
}
