/*
 * UART.h
 *
 *  Created on: 14. 9. 2013
 *      Author: Tomas Bures <bures@d3s.mff.cuni.cz>
 */

#ifndef UART_H_
#define UART_H_

#include "stm32f4xx.h"

class UART {
public:
	struct Properties {
		GPIO_TypeDef* gpio;
		USART_TypeDef* usart;
		uint32_t pinTX, pinRX;
		uint16_t pinSourceTX, pinSourceRX;
		void (*clkUSARTCmdFun)(uint32_t periph, FunctionalState newState);
		uint32_t clkGPIO, clkUSART;
		uint8_t afConfig;
		uint8_t irqn;
		uint32_t baudRate;
	};

	UART(Properties& initProps);
	~UART();

	typedef void (*Listener)(void *);

	void setPriority(uint8_t irqPreemptionPriority, uint8_t irqSubPriority);
	void init();

	inline bool isBreakOrError() {
		return props.usart->SR & (USART_FLAG_FE | USART_FLAG_ORE | USART_FLAG_PE);
	}

	void clearBreakOrError();

	void enableSendEvents();
	void disableSendEvents();
	void setSendListener(Listener sendListener, void *obj);

	void enableRecvEvents();
	void disableRecvEvents();
	void setRecvListener(Listener recvListener, void *obj);

	void txrxInterruptHandler();

	inline uint8_t recv() {
		return (uint8_t)props.usart->DR;
	}

	inline void send(uint8_t ch) {
		props.usart->DR = ch;
	}

	inline bool canSend() {
		return props.usart->SR & USART_FLAG_TXE;
	}

	inline bool canRecv() {
		return props.usart->SR & USART_FLAG_RXNE;
	}

	inline bool isSendComplete() {
		return props.usart->SR & USART_FLAG_TC;
	}

private:
	uint8_t irqPreemptionPriority;
	uint8_t irqSubPriority;

	bool rxIntEnabled;
	bool txIntEnabled;

	Listener sendListener;
	void *sendListenerObj;

	Listener recvListener;
	void *recvListenerObj;

	Properties props;
};

#endif /* UART_H_ */
