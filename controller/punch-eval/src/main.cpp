#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>

void task_blink(void *p);

uint32_t mainCycles;

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

Button::Properties userButtonProps {
	GPIOA, GPIO_Pin_0, RCC_AHB1Periph_GPIOA, EXTI_Line0, EXTI_PortSourceGPIOA, EXTI_PinSource0, EXTI0_IRQn
};
Button infoButton(userButtonProps);

UART::Properties uart2Props {
	GPIOA, USART2,
	GPIO_Pin_2, GPIO_Pin_3, GPIO_PinSource2, GPIO_PinSource3,
	RCC_APB1PeriphClockCmd, RCC_AHB1Periph_GPIOA, RCC_APB1Periph_USART2, GPIO_AF_USART2, USART2_IRQn,
	921600
};
UART pcUART(uart2Props);

void handleInfoButtonInterrupt(void*) {
	printf("Test");
}

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);	// 4 bits for pre-emption priority, 0 bits for non-preemptive subpriority
	pcUART.setPriority(1,0);
	infoButton.setPriority(2,0);

	greenLed.init();
	redLed.init();
	blueLed.init();
	orangeLed.init();

	pcUART.init();

	infoButton.setPressedListener(handleInfoButtonInterrupt, nullptr);
	infoButton.init();

	// Create a task
	ret = xTaskCreate(task_blink, "blink", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	if (ret == pdTRUE) {
		printf("System Started!\n");
		vTaskStartScheduler();  // should never return
	} else {
		printf("System Error!\n");
		// --TODO blink some LEDs to indicate fatal system error
	}

	NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, ENABLE); // This ..
	while (1) {
		__WFI(); // ... and this has to be commented out when debugging.
		mainCycles++; // This is to measure how many times we wake up from WFI. In fact, we should never wake up.
	}
}

void vApplicationTickHook(void) {
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

void task_blink(void* p) {
	vTaskDelete(NULL);
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

