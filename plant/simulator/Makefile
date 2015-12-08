# Comment/uncomment the following line to disable/enable debugging
DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS) -I$(LDDINCDIR)

MODULE_NAME=simulator
PRU_PRG=spi_controller
PRU_BIN=$(PRU_PRG)_bin.h

ifneq ($(KERNELRELEASE),)
# call from kernel build system

$(MODULE_NAME)-objs := xorshift.o simulation.o main.o userspace_interface.o json.o

obj-m	:= $(MODULE_NAME).o

else

.PHONY: pru

default:
	$(MAKE) -C ../ modules

pru: $(PRU_BIN)

$(PRU_BIN): $(PRU_PRG).p
	$(PASM) $(PASM_V) $(PRU_PRG).p

tester: tester.c simulation.c json.c xorshift.c
	gcc -o tester -Wall tester.c simulation.c json.c xorshift.c

disasm:
	$(TOOLCHAIN_PREFIX)objdump -dS ./$(MODULE_NAME).ko > ./$(MODULE_NAME).disasm

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.disasm tester $(PRU_BIN)

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif
