.PHONY: all clean

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m := pruss.o

else

all:

clean:
	rm -rf *.o modules.order .*.o.cmd *.ko *.mod.c .*.ko.cmd Module.symvers .tmp_versions

endif

