ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m := spi_slave.o simulator.o 
main-objs = json.o simulation.o userspace_interface.o xorshift.o

else

KERNEL=~/kralovec/punch_press/plant/kernel/kernel-3.8_clean/

all: spi_controller_bin.h
	make -C $(KERNEL) ARCH=arm CROSS_COMPILE=arm-linux-gnu- M=`pwd` modules

spi_controller_bin.h: spi_controller.p
	../../pasm/pasm spi_controller.p

clean:
	rm -rf spi_controller_bin.h *.o modules.order .*.o.cmd *.ko *.mod.c .*.ko.cmd Module.symvers .tmp_versions

endif


