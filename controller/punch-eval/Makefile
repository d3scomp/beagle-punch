TARGET:=punch-eval
TOOLCHAIN_PREFIX:=arm-none-eabi-
TOOLCHAIN_ROOT:=/root/gcc-arm

# Optimization level, can be [0, 1, 2, 3, s].
OPTLVL:=0
DBG:=-g3

FREERTOS:=$(CURDIR)/FreeRTOS
STARTUP:=$(CURDIR)/hardware
LINKER_SCRIPT:=$(CURDIR)/stm32_flash.ld

INCLUDE=-I$(CURDIR)/src
INCLUDE=-I$(CURDIR)/hardware
INCLUDE+=-I$(FREERTOS)/include
INCLUDE+=-I$(FREERTOS)/portable/GCC/ARM_CM4F
INCLUDE+=-I$(CURDIR)/libraries/CMSIS/Device/ST/STM32F4xx/Include
INCLUDE+=-I$(CURDIR)/libraries/CMSIS/Include
INCLUDE+=-I$(CURDIR)/libraries/STM32F4xx_StdPeriph_Driver/inc
INCLUDE+=-I$(CURDIR)/config

BUILD_DIR = $(CURDIR)/build
BIN_DIR = $(CURDIR)/binary

# vpath is used so object files are written to the current directory instead
# of the same directory as their source files
vpath %.c $(CURDIR)/src $(CURDIR)/libraries/STM32F4xx_StdPeriph_Driver/src \
	  $(CURDIR)/libraries/syscall $(CURDIR)/hardware $(FREERTOS) \
	  $(FREERTOS)/portable/MemMang $(FREERTOS)/portable/GCC/ARM_CM4F 

vpath %.s $(STARTUP)

vpath %.cpp $(CURDIR)/src

# Project Source Files
SRC+=startup_stm32f4xx.s
SRC+=stm32f4xx_it.c
SRC+=system_stm32f4xx.c
SRC+=syscalls.c
#SRC+=main2.cpp
SRC+=main.cpp
SRC+=hooks.cpp
SRC+=Button.cpp
SRC+=LED.cpp
SRC+=SPI.cpp
SRC+=UART.cpp

# FreeRTOS Source Files
SRC+=port.c
SRC+=list.c
SRC+=queue.c
SRC+=tasks.c
SRC+=event_groups.c
SRC+=timers.c
SRC+=heap_4.c

# Standard Peripheral Source Files
SRC+=stm32f4xx_syscfg.c
SRC+=misc.c
SRC+=stm32f4xx_adc.c
SRC+=stm32f4xx_dac.c
SRC+=stm32f4xx_dma.c
SRC+=stm32f4xx_exti.c
SRC+=stm32f4xx_flash.c
SRC+=stm32f4xx_gpio.c
SRC+=stm32f4xx_i2c.c
SRC+=stm32f4xx_rcc.c
SRC+=stm32f4xx_spi.c
SRC+=stm32f4xx_tim.c
SRC+=stm32f4xx_usart.c
SRC+=stm32f4xx_rng.c

CDEFS=-DUSE_STDPERIPH_DRIVER
CDEFS+=-DSTM32F4XX
CDEFS+=-DHSE_VALUE=8000000
CDEFS+=-D__FPU_PRESENT=1
CDEFS+=-D__FPU_USED=1
CDEFS+=-DARM_MATH_CM4

MCUFLAGS=-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb-interwork -MMD -MP -mlittle-endian
COMMONFLAGS=-O$(OPTLVL) $(DBG) -Wall
CFLAGS=$(COMMONFLAGS) $(MCUFLAGS) $(INCLUDE) $(CDEFS)
CPPFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -std=c++11 -fno-use-cxa-atexit 
#LDFLAGS=$(COMMONFLAGS) $(MCUFLAGS) -fno-exceptions -ffunction-sections -fdata-sections -nostartfiles -Wl,--gc-sections,-T$(LINKER_SCRIPT)
LDFLAGS=$(COMMONFLAGS) -T$(LINKER_SCRIPT) -Wl,-Map,$(BIN_DIR)/$(TARGET).map $(CPPFLAGS)


CC=$(TOOLCHAIN_PREFIX)gcc
CPP=$(TOOLCHAIN_PREFIX)g++
LD=$(TOOLCHAIN_PREFIX)g++
OBJCOPY=$(TOOLCHAIN_PREFIX)objcopy
OSIZE=$(TOOLCHAIN_PREFIX)size
AS=$(TOOLCHAIN_PREFIX)as

OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))
OBJ := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(OBJ))
OBJ := $(patsubst %.s,$(BUILD_DIR)/%.o,$(OBJ))

DEP = $(patsubst %.c,$(BUILD_DIR)/%.d,$(SRC))
DEP := $(patsubst %.cpp,$(BUILD_DIR)/%.d,$(DEP))
DEP := $(patsubst %.s,,$(DEP))


$(BUILD_DIR)/%.o: %.c
	@echo [CC] $(notdir $<)
	$(CC) $(CFLAGS) $< -c -o $@

$(BUILD_DIR)/%.o: %.cpp
	@echo [C++] $(notdir $<)
	$(CPP) $(CPPFLAGS) $< -c -o $@

$(BUILD_DIR)/%.o: %.s
	@echo [AS] $(notdir $<)
	$(AS) $< -o $@

$(BUILD_DIR)/%.dep: %.c
	$(CC) -M $(CFLAGS) "$<" > "$@"

$(BUILD_DIR)/%.dep: %.cpp
	$(CPP) -M $(CPPFLAGS) "$<" > "$@"

all: $(BIN_DIR)/$(TARGET).elf

$(BIN_DIR)/$(TARGET).elf: $(OBJ)
	@echo [LD] $(TARGET).elf
	$(LD) -o $(BIN_DIR)/$(TARGET).elf $(LDFLAGS) $(OBJ) $(LDLIBS)
	@echo [OBJCOPY] $(TARGET).hex
	@$(OBJCOPY) -O ihex $(BIN_DIR)/$(TARGET).elf $(BIN_DIR)/$(TARGET).hex
	@$(OSIZE) --format=berkeley $(BIN_DIR)/$(TARGET).elf
	
#	@echo [OBJCOPY] $(TARGET).bin
#	@$(OBJCOPY) -O binary $(BIN_DIR)/$(TARGET).elf $(BIN_DIR)/$(TARGET).bin

.PHONY: clean

clean:
	@echo [RM] OBJ
	@rm -f $(OBJ)
	@echo [RM] BIN
	@rm -f $(BIN_DIR)/$(TARGET).elf
	@rm -f $(BIN_DIR)/$(TARGET).hex
	@rm -f $(BIN_DIR)/$(TARGET).bin

openocd:
	#openocd -f interface/stlink-v2.cfg -f target/stm32f4x_stlink.cfg
	openocd -f board/stm32f4discovery.cfg

flash: all
	openocd -f board/stm32f4discovery.cfg -c "program $(BIN_DIR)/$(TARGET).elf verify reset"



-include $(DEP)