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
#include "SPI.h"

extern Button infoButton;
extern UART pcUART;
extern SPI punchSPI;
extern LED greenLed;
extern LED redLed;
extern LED orangeLed;
extern LED blueLed;
extern PulseLED greenPulseLed;

#endif /* MAIN_H_ */
