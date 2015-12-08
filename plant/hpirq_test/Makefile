# Comment/uncomment the following line to disable/enable debugging
DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS) -I$(LDDINCDIR)

MODULE_NAME = hpirq_test

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= $(MODULE_NAME).o

else

include ../kmodule_setup.mk

default:
	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(TOOLCHAIN_PREFIX) M=$(PWD) LDDINCDIR=$(PWD)/../include modules

endif


disasm:
	$(TOOLCHAIN_PREFIX)objdump -dS ./$(MODULE_NAME).ko > ./$(MODULE_NAME).disasm

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.disasm

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif
