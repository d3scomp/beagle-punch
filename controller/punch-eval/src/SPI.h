#ifndef SPI_H_
#define SPI_H_

#include "stm32f4xx.h"

class SPI {
public:
	struct Properties {
		GPIO_TypeDef* gpioCS;
		GPIO_TypeDef* gpioTXRX;
		SPI_TypeDef* spi;
		uint32_t pinCS;
		uint32_t pinSCK;
		uint32_t pinMISO;
		uint32_t pinMOSI;
		uint8_t pinSourceSCK;
		uint8_t pinSourceMISO;
		uint8_t pinSourceMOSI;
		uint32_t clkGPIOs;
		void (*clkSPICmdFun)(uint32_t periph, FunctionalState newState);
		uint32_t clkSPI;
		uint8_t afConfig;
		IRQn irqnSPI;
	};

	SPI(Properties& initProps);
	~SPI();

	void setPriority(uint8_t irqPreemptionPriority, uint8_t irqSubPriority);
	void init();

	inline void writeByte(uint8_t val) {
		props.gpioCS->BSRRH = props.pinCS;

		props.spi->DR = val;

		while (!(props.spi->SR & SPI_I2S_FLAG_TXE)); // BSY flag is set only after some delay, so it is compulsory to first wait for TXE, which ensures that BSY flag is set meanwhile
		while (props.spi->SR & SPI_I2S_FLAG_BSY);
		props.gpioCS->BSRRL = props.pinCS;
	}

	inline void writeBytes(uint8_t val1, uint8_t val1) {
		props.gpioCS->BSRRH = props.pinCS;

		props.spi->DR = val1;

		while (!(props.spi->SR & SPI_I2S_FLAG_TXE));
		props.spi->DR = val2;

		while (!(props.spi->SR & SPI_I2S_FLAG_TXE)); // BSY flag is set only after some delay, so it is compulsory to first wait for TXE, which ensures that BSY flag is set meanwhile
		while (props.spi->SR & SPI_I2S_FLAG_BSY);
		props.gpioCS->BSRRL = props.pinCS;
	}

	inline uint8_t readByte(uint8_t addr) {
		uint8_t result;

		props.gpioCS->BSRRH = props.pinCS;

		props.spi->DR = addr;

		while (!(props.spi->SR & SPI_I2S_FLAG_TXE));
		props.spi->DR = 0;

		while (!(props.spi->SR & SPI_I2S_FLAG_TXE)); // BSY flag is set only after some delay, so it is compulsory to first wait for TXE, which ensures that BSY flag is set meanwhile
		while (props.spi->SR & SPI_I2S_FLAG_BSY);
		result = props.spi->DR;

		props.gpioCS->BSRRL = props.pinCS;

		return result;
	}

	inline void interruptHandler() {
		if (props.spi->SR & SPI_I2S_FLAG_RXNE) {
			lastReadValue = props.spi->DR;
		}
	}

private:
	Properties props;

	uint8_t lastReadValue;

	uint8_t irqPreemptionPriority;
	uint8_t irqSubPriority;
}

#endif
