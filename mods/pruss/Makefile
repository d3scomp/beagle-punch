ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m := pruss.o

else

KERNEL=~/kralovec/punch_press/plant/kernel/kernel-3.8_clean/

all:
	make -C $(KERNEL) ARCH=arm CROSS_COMPILE=arm-linux-gnu- M=`pwd` modules

clean:
	rm -rf *.o modules.order .*.o.cmd *.ko *.mod.c .*.ko.cmd Module.symvers .tmp_versions

endif

