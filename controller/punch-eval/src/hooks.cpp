#include "hooks.h"
#include "main.h"

void SysTick_Handler(void) {

}
/*

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

*/

void vApplicationTickHook(void) {
//	PulseLED::tickInterruptHandler();
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

