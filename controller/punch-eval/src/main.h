/*
 * main.h
 *
 *  Created on: 15. 9. 2013
 *      Author: Tomas Bures <bures@d3s.mff.cuni.cz>
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "stm32f4xx.h"

#include "UART.h"
#include "LED.h"
#include "Button.h"
#include "TODQueue.h"
#include "TOHQueue.h"
#include "MsgHandler.h"
#include "MRF24J40.h"
#include "Timer.h"
#include "GPS.h"

extern uint32_t mainCycles;

extern Button infoButton;
extern UART uartTOHD;
extern MsgHandler msgHandler;
extern Timer delayTimer;
extern MRF24J40 mrf;
extern UART uartGPS;
extern GPSL30 gps;


#endif /* MAIN_H_ */
