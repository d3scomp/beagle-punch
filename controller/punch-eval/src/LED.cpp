/*
 * LED.cpp
 *
 *  Created on: 15. 9. 2013
 *      Author: Tomas Bures <bures@d3s.mff.cuni.cz>
 */

#include "LED.h"

LED::LED(Properties& initProps) : props(initProps) {
}

LED::~LED() {
}

void LED::init() {
	GPIO_InitTypeDef  gpioInitStruct;

	/* Enable the GPIO_LED Clock */
	RCC_AHB1PeriphClockCmd(props.clk, ENABLE);

	/* Configure the GPIO_LED pin */
	gpioInitStruct.GPIO_Pin = props.pin;
	gpioInitStruct.GPIO_Mode = GPIO_Mode_OUT;
	gpioInitStruct.GPIO_OType = GPIO_OType_PP;
	gpioInitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	gpioInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(props.gpio, &gpioInitStruct);
}

void LED::on() {
	props.gpio->BSRRL = props.pin;
}

void LED::off() {
	props.gpio->BSRRH = props.pin;
}

PulseLED* PulseLED::tickListeners[MAX_TICK_LISTENERS];
int PulseLED::tickListenersNum = 0;

void PulseLED::tickInterruptHandler() {
	for (int idx = 0; idx < tickListenersNum; idx++) {
		tickListeners[idx]->tick();
	}
}


PulseLED::PulseLED(LED& led, int minimalOnTimeTicks) : led(led), minimalOnTimeTicks(minimalOnTimeTicks), onTicks(-1) {
}

PulseLED::~PulseLED() {
}

void PulseLED::init() {
	assert_param(tickListenersNum < MAX_TICK_LISTENERS);
	tickListeners[tickListenersNum++] = this;
}

void PulseLED::pulse() {
	onTicks = 0;
	led.on();
}

void PulseLED::tick() {
	if (onTicks != -1) {
		onTicks++;

		if (onTicks > minimalOnTimeTicks) {
			onTicks = -1;
			led.off();
		}
	}
}

