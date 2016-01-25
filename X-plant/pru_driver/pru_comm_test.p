// pru_test.p

.origin 0

.entrypoint start


#include "pru_test.hp"


#define GPIO1 0x4804c000

#define GPIO_CLEARDATAOUT 0x190

#define GPIO_SETDATAOUT 0x194

#define LED_ON_VAR      0x0

start:


        // Enable OCP master port

        LBCO      r0, CONST_PRUCFG, 4, 4

        CLR     r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port

        SBCO      r0, CONST_PRUCFG, 4, 4


LOOP0:

        mov r10, LED_ON_VAR
        lbbo r11, r10, 0, 4
        qbeq led_off, r11, 0

led_on:
        MOV r3, GPIO1 | GPIO_SETDATAOUT
        jmp set_led

led_off:
        MOV r3, GPIO1 | GPIO_CLEARDATAOUT

set_led:
        MOV r2, 1<<21
        SBBO r2, r3, 0, 4

        jmp LOOP0



        // Halt the processor

        HALT
