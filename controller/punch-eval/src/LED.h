/*
 * LED.h
 *
 *  Created on: 15. 9. 2013
 *      Author: Tomas Bures <bures@d3s.mff.cuni.cz>
 */

#ifndef LED_H_
#define LED_H_

#include "stm32f4xx.h"

class LED {
public:
	struct Properties {
		GPIO_TypeDef* gpio;
		uint32_t pin;
		uint32_t clk;
	};

	LED(Properties& initProps);
	~LED();

	void on();
	void off();
	void init();

	void test(uint32_t pin);

private:
	Properties props;
};

class PulseLED {
public:
	PulseLED(LED& led, int minimalOnTimeTicks);
	~PulseLED();

	void pulse();
	void init();

	static void tickInterruptHandler();

private:
	LED& led;
	int minimalOnTimeTicks;
	int onTicks;

	void tick();

	static constexpr auto MAX_TICK_LISTENERS = 5;
	static PulseLED* tickListeners[MAX_TICK_LISTENERS];
	static int tickListenersNum;
};

#endif /* LED_H_ */
