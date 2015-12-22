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
	GPIO_AF_SPI3,
	SPI2_IRQn
};
SPI punchSPI(spi2Props);

void handleInfoButtonInterrupt(void*) {
	printf(
		"\nInfo:"
		"\n  mainCycles = %d"
		"\n",
		mainCycles);
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
	ret = xTaskCreate(blinkTask, "blink", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

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

void USART2_IRQHandler(void) {
//	GPIOA->BSRRL = GPIO_Pin_1;  // Requires test1Led to be initialized in main.cpp
	pcUART.txrxInterruptHandler();
//	GPIOA->BSRRH = GPIO_Pin_1;
}

void EXTI0_IRQHandler(void) {
	infoButton.pressedInterruptHandler();
}

void SPI3_IRQHandler(void) {
//	GPIOA->BSRRL = GPIO_Pin_5;    // Requires test2Led to be initialized in main.cpp
	punchSPI.interruptHandler();
//	GPIOA->BSRRH = GPIO_Pin_5;
}

void vApplicationTickHook(void) {
	PulseLED::tickInterruptHandler();
}

/* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created.  It is also called by various parts of the
   demo application.  If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
void vApplicationMallocFailedHook(void) {
	taskDISABLE_INTERRUPTS();
	for(;;);
}

/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
   task.  It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()).  If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
void vApplicationIdleHook(void) {
}

void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName) {
	(void) pcTaskName;
	(void) pxTask;
	/* Run time stack overflow checking is performed if
     configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
     function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for(;;);
}

#ifdef  USE_FULL_ASSERT

/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
	printf("Wrong parameters value: file %s on line %d\r\n", file, line);
	taskDISABLE_INTERRUPTS();
	for(;;);
}
#endif

