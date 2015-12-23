#include "main2.h"
#include "FreeRTOS.h"
#include "task.h"

#include "LED.h"

#include <cstdio>

LED::Properties redLedProps {
	GPIOD, GPIO_Pin_14, RCC_AHB1Periph_GPIOD
};

LED redLed(redLedProps);

int main(void) {
	GPIO_InitTypeDef  gpioInitStruct;

RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
gpioInitStruct.GPIO_Pin = GPIO_Pin_12;
gpioInitStruct.GPIO_Mode = GPIO_Mode_OUT;
gpioInitStruct.GPIO_OType = GPIO_OType_PP;
gpioInitStruct.GPIO_PuPd = GPIO_PuPd_UP;
gpioInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOD, &gpioInitStruct);
GPIOD->BSRRL = GPIO_Pin_12;

redLed.init();
	redLed.on();


	int counter = 0;
  for (;;) {
	  if (counter % 4000000 == 0) {
		  GPIOD->BSRRH = GPIO_Pin_12;
	  }

	  if (counter % 4000000 == 2000000) {
		  GPIOD->BSRRL = GPIO_Pin_12;
	  }

	  counter++;
  }
}

