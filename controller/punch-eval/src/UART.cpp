/*
 * UART.cpp
 *
 *  Created on: 14. 9. 2013
 *      Author: Tomas Bures <bures@d3s.mff.cuni.cz>
 */

#include "UART.h"

void UART::clearBreakOrError() {
	volatile int32_t dummy;
	dummy = props.usart->SR;
	dummy = props.usart->DR;
}

void UART::enableSendEvents() {
	txIntEnabled = true;
	USART_ITConfig(props.usart, USART_IT_TXE, ENABLE);
}

void UART::disableSendEvents() {
	USART_ITConfig(props.usart, USART_IT_TXE, DISABLE);
	txIntEnabled = false;
}

void UART::setSendListener(Listener sendReadyListener, void *obj) {
	this->sendListener = sendReadyListener;
	sendListenerObj = obj;
}

void UART::enableRecvEvents() {
	rxIntEnabled = true;
	USART_ITConfig(props.usart, USART_IT_RXNE, ENABLE);
}

void UART::disableRecvEvents() {
	USART_ITConfig(props.usart, USART_IT_RXNE, DISABLE);
	rxIntEnabled = false;
}

void UART::setRecvListener(Listener recvReadyListener, void *obj) {
	this->recvListener = recvReadyListener;
	recvListenerObj = obj;
}

void UART::txrxInterruptHandler() {
	if (rxIntEnabled && canRecv()) {
		assert_param(recvListener);
		recvListener(recvListenerObj);
	}

	if (txIntEnabled && canSend()) {
		assert_param(sendListener);
		sendListener(sendListenerObj);
	}
}


UART::UART(Properties& initProps) : props(initProps) {
}

UART::~UART() {
	USART_DeInit(props.usart);
}


void UART::setPriority(uint8_t irqPreemptionPriority, uint8_t irqSubPriority) {
	this->irqPreemptionPriority = irqPreemptionPriority;
	this->irqSubPriority = irqSubPriority;
}


void UART::init() {
	RCC_AHB1PeriphClockCmd(props.clkGPIO, ENABLE);

	GPIO_PinAFConfig(props.gpio, props.pinSourceTX, props.afConfig); // alternative function USARTx_TX
	GPIO_PinAFConfig(props.gpio, props.pinSourceRX, props.afConfig); // alternative function USARTx_RX

	GPIO_InitTypeDef gpioInitStruct;
	gpioInitStruct.GPIO_Pin = props.pinTX | props.pinRX;
	gpioInitStruct.GPIO_Mode = GPIO_Mode_AF;
	gpioInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	gpioInitStruct.GPIO_OType = GPIO_OType_PP;
	gpioInitStruct.GPIO_PuPd = GPIO_PuPd_UP;

	GPIO_Init(props.gpio, &gpioInitStruct);


	props.clkUSARTCmdFun(props.clkUSART, ENABLE);

	USART_InitTypeDef usartInitStruct;
	usartInitStruct.USART_BaudRate = props.baudRate;
	usartInitStruct.USART_WordLength = USART_WordLength_8b;
	usartInitStruct.USART_StopBits = USART_StopBits_1;
	usartInitStruct.USART_Parity = USART_Parity_No;
	usartInitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	usartInitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

	USART_Init(props.usart, &usartInitStruct);

	NVIC_InitTypeDef nvicInitStruct;

	// Enable the USART Interrupt
	nvicInitStruct.NVIC_IRQChannel = props.irqn;
	nvicInitStruct.NVIC_IRQChannelPreemptionPriority = irqPreemptionPriority;
	nvicInitStruct.NVIC_IRQChannelSubPriority = irqSubPriority;
	nvicInitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvicInitStruct);

	USART_Cmd(props.usart, ENABLE);
}

