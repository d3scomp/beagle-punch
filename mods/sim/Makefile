.PHONY: all clean

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m := spi_slave.o sim.o 
sim-objs = json.o simulation.o simulator.o userspace_interface.o xorshift.o

else

all: spi_controller_bin.h

spi_controller_bin.h: spi_controller.p
	../../pasm/pasm spi_controller.p

clean:
	#rm -rf spi_controller_bin.h *.o modules.order .*.o.cmd *.ko *.mod.c .*.ko.cmd Module.symvers .tmp_versions
	rm -rf *.o modules.order .*.o.cmd *.ko *.mod.c .*.ko.cmd Module.symvers .tmp_versions

endif


