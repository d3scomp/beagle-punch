/*
 * Button.cpp
 *
 *  Created on: 15. 9. 2013
 *      Author: Tomas Bures <bures@d3s.mff.cuni.cz>
 */

#include "Button.h"

Button::Button(Properties& initProps) : props(initProps) {
}

Button::~Button() {
}

void Button::setPriority(uint8_t irqPreemptionPriority, uint8_t irqSubPriority) {
	this->irqPreemptionPriority = irqPreemptionPriority;
	this->irqSubPriority = irqSubPriority;
}

void Button::init() {
	GPIO_InitTypeDef gpioInitStruct;
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/* Enable the Button Clock */
	RCC_AHB1PeriphClockCmd(props.clk, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	/* Configure Button pin as input */
	gpioInitStruct.GPIO_Mode = GPIO_Mode_IN;
	gpioInitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	gpioInitStruct.GPIO_Pin = props.pin;
	GPIO_Init(props.gpio, &gpioInitStruct);

	SYSCFG_EXTILineConfig(props.extiPortSource, props.extiPinSource);

	/* Configure Button EXTI line */
	EXTI_InitStructure.EXTI_Line = props.extiLine;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	/* Enable and set Button EXTI Interrupt to the given priority */
	NVIC_InitStructure.NVIC_IRQChannel = props.irqn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = irqPreemptionPriority;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = irqSubPriority;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&NVIC_InitStructure);
}


bool Button::isPressed() {
	return GPIO_ReadInputDataBit(props.gpio, props.pin);
}

void Button::setPressedListener(Listener pressedListener, void* obj) {
	this->pressedListener = pressedListener;
	pressedListenerObj = obj;
}

void Button::pressedInterruptHandler() {
	EXTI_ClearITPendingBit(props.extiLine);

	assert_param(pressedListener);
	pressedListener(pressedListenerObj);
}

