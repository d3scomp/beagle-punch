/*
 * Button.h
 *
 *  Created on: 15. 9. 2013
 *      Author: Tomas Bures <bures@d3s.mff.cuni.cz>
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#include "stm32f4xx.h"

class Button {
public:
	struct Properties {
		GPIO_TypeDef* gpio;
		uint32_t pin;
		uint32_t clk;

		uint32_t extiLine;
		uint8_t extiPortSource;
		uint8_t extiPinSource;
		IRQn irqn;
	};

	Button(Properties &initProps);
	~Button();

	typedef void (*Listener)(void *);

	bool isPressed();
	void setPressedListener(Listener pressedListener, void* obj);

	void setPriority(uint8_t irqPreemptionPriority, uint8_t irqSubPriority);
	void init();

	void pressedInterruptHandler();

private:
	Properties props;

	uint8_t irqPreemptionPriority;
	uint8_t irqSubPriority;

	Listener pressedListener;
	void *pressedListenerObj;
};

#endif /* BUTTON_H_ */
