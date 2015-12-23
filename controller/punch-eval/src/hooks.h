#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

void USART2_IRQHandler(void);
void EXTI0_IRQHandler(void);
void SPI2_IRQHandler(void);
void vApplicationTickHook(void);
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName);

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line);
#endif


#ifdef __cplusplus
}
#endif
