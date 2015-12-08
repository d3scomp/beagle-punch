# Comment/uncomment the following line to disable/enable debugging
DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS) -I$(LDDINCDIR)

MODULE_NAME = pruss
TESTER_NAME = pruss_tester
COMM_TESTER_NAME = pruss_comm_tester

PRU_PRG = pru_test
PRU_BIN = $(PRU_PRG)_bin.h

COMM_PRU_PRG = pru_comm_test
COMM_PRU_BIN = $(COMM_PRU_PRG)_bin.h

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= $(MODULE_NAME).o \
	$(TESTER_NAME).o \
	$(COMM_TESTER_NAME).o

else

.PHONY: pru

default: pru
	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(TOOLCHAIN_PREFIX) M=$(PWD) LDDINCDIR=$(PWD)/../include modules

pru: $(PRU_BIN) $(COMM_PRU_BIN)

$(PRU_BIN): $(PRU_PRG).p
	$(PASM) $(PASM_V) $(PRU_PRG).p

$(COMM_PRU_BIN): $(COMM_PRU_PRG).p
	$(PASM) $(PASM_V) $(COMM_PRU_PRG).p

disasm:
	$(TOOLCHAIN_PREFIX)objdump -dS ./$(MODULE_NAME).ko > ./$(MODULE_NAME).disasm
	$(TOOLCHAIN_PREFIX)objdump -dS ./$(TESTER_NAME).ko > ./$(TESTER_NAME).disasm

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.disasm $(PRU_BIN) $(COMM_PRU_BIN)

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
