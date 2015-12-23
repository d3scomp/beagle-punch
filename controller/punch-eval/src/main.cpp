#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>

void blinkTask(void* p) {
	const TickType_t period = 1000 / portTICK_PERIOD_MS;

	TickType_t lastWakeTime = xTaskGetTickCount();
	for(;;) {
		greenPulseLed.pulse();

		// Wait for the next cycle.
		vTaskDelayUntil(&lastWakeTime, period);
	}

	vTaskDelete(NULL);
}

uint32_t mainCycles = 0;

/*
LED::Properties test1LedProps {
	GPIOA, GPIO_Pin_1, RCC_AHB1Periph_GPIOA
};
LED test1Led(test1LedProps);

LED::Properties test2LedProps {
	GPIOA, GPIO_Pin_5, RCC_AHB1Periph_GPIOA
};
LED test2Led(test2LedProps);
*/

LED::Properties greenLedProps {
	GPIOD, GPIO_Pin_12, RCC_AHB1Periph_GPIOD
};
LED::Properties redLedProps {
	GPIOD, GPIO_Pin_14, RCC_AHB1Periph_GPIOD
};
LED::Properties orangeLedProps {
	GPIOD, GPIO_Pin_13, RCC_AHB1Periph_GPIOD
};
LED::Properties blueLedProps {
	GPIOD, GPIO_Pin_15, RCC_AHB1Periph_GPIOD
};
LED greenLed(greenLedProps);
LED redLed(redLedProps);
LED blueLed(blueLedProps);
LED orangeLed(orangeLedProps);

PulseLED greenPulseLed(greenLed, 10);

Button::Properties userButtonProps {
	GPIOA, GPIO_Pin_0, RCC_AHB1Periph_GPIOA, EXTI_Line0, EXTI_PortSourceGPIOA, EXTI_PinSource0, EXTI0_IRQn
};
Button infoButton(userButtonProps);

UART::Properties uart2Props {
	GPIOA, USART2,
	GPIO_Pin_2, GPIO_Pin_3, GPIO_PinSource2, GPIO_PinSource3,
	RCC_APB1PeriphClockCmd, RCC_AHB1Periph_GPIOA, RCC_APB1Periph_USART2, GPIO_AF_USART2, USART2_IRQn,
	115200
};
UART pcUART(uart2Props);

SPI::Properties spi2Props {
	GPIOB, GPIOB,
	SPI2,
	GPIO_Pin_0, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15,
	GPIO_PinSource13, GPIO_PinSource14, GPIO_PinSource15,
	RCC_AHB1Periph_GPIOB,
	RCC_APB1PeriphClockCmd,
	RCC_APB1Periph_SPI2,
	GPIO_AF_SPI2,
	SPI2_IRQn
};
SPI punchSPI(spi2Props);

void handleInfoButtonInterrupt(void*) {
	printf(
		"\r\nInfo:"
		"\r\n  mainCycles = %lu"
		"\r\n",
		mainCycles);

	punchSPI.writeBytes(4, 30);
	punchSPI.writeBytes(5, 50);
	punchSPI.readByte(1);
	punchSPI.readByte(2);
	punchSPI.readByte(0);
}

int main(void)
{

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);	// 4 bits for pre-emption priority, 0 bits for non-preemptive subpriority
	pcUART.setPriority(1,0);
	infoButton.setPriority(2,0);
	punchSPI.setPriority(0,0);

	greenLed.init();
	redLed.init();
	blueLed.init();
	orangeLed.init();

	greenPulseLed.init();

	pcUART.init();

	punchSPI.init();

	infoButton.setPressedListener(handleInfoButtonInterrupt, nullptr);
	infoButton.init();

	// Create a task
	BaseType_t ret = xTaskCreate(blinkTask, "blink", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	if (ret == pdTRUE) {
		printf("System Started!\n");
		vTaskStartScheduler();  // should never return
	} else {
		printf("System Error!\n");
	}

	NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, ENABLE); // This ..
	while (1) {
		__WFI(); // ... and this has to be commented out when debugging.
		mainCycles++; // This is to measure how many times we wake up from WFI. In fact, we should never wake up.
	}
}


