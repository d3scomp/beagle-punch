#include "SPI.h"
#include "main.h"

SPI::SPI(Properties& initProps) : props(initProps) {
}

SPI::~SPI() {
}

void SPI::setPriority(uint8_t irqPreemptionPriority, uint8_t irqSubPriority) {
	this->irqPreemptionPriority = irqPreemptionPriority;
	this->irqSubPriority = irqSubPriority;
}

void SPI::init() {
	// Enable the SPI clock
	props.clkSPICmdFun(props.clkSPI, ENABLE);

	// Enable GPIO clocks
	RCC_AHB1PeriphClockCmd(props.clkGPIOs, ENABLE);

	// Connect SPI pins to AF5
	GPIO_PinAFConfig(props.gpioTXRX, props.pinSourceSCK, props.afConfig); // alternative function SPIx_SCK
	GPIO_PinAFConfig(props.gpioTXRX, props.pinSourceMISO, props.afConfig); // alternative function SPIx_MISO
	GPIO_PinAFConfig(props.gpioTXRX, props.pinSourceMOSI, props.afConfig); // alternative function SPIx_MOSI

	// GPIO Deinitialisation
	GPIO_InitTypeDef gpioInitStruct;

	gpioInitStruct.GPIO_Pin = props.pinSCK | props.pinMISO | props.pinMOSI;
	gpioInitStruct.GPIO_Mode = GPIO_Mode_AF;
	gpioInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	gpioInitStruct.GPIO_OType = GPIO_OType_PP;
	gpioInitStruct.GPIO_PuPd  = GPIO_PuPd_DOWN;
	GPIO_Init(props.gpioTXRX, &gpioInitStruct);

	gpioInitStruct.GPIO_Pin = props.pinCS;
	gpioInitStruct.GPIO_Mode = GPIO_Mode_OUT;
	gpioInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	gpioInitStruct.GPIO_OType = GPIO_OType_PP;
	gpioInitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_Init(props.gpioCS, &gpioInitStruct);

	// SPI configuration
	SPI_InitTypeDef spiInitStruct;
	SPI_StructInit(&spiInitStruct);
	spiInitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	spiInitStruct.SPI_Mode = SPI_Mode_Master;
	spiInitStruct.SPI_DataSize = SPI_DataSize_8b;
	spiInitStruct.SPI_CPOL = SPI_CPOL_Low;
	spiInitStruct.SPI_CPHA = SPI_CPHA_1Edge;
	spiInitStruct.SPI_NSS = SPI_NSS_Soft;
	spiInitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
	spiInitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_Init(props.spi, &spiInitStruct);

	// Default (idle) CS state
	GPIO_SetBits(props.gpioCS, props.pinCS);

	// Configure the SPI interrupt priority
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = props.irqn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = irqPreemptionPriority;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = irqSubPriority;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	SPI_I2S_ITConfig(props.spi, SPI_I2S_IT_RXNE, ENABLE);

	SPI_Cmd(props.spi, ENABLE);
}
