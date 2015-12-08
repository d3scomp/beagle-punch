.PHONY: all bbb vi-bbb modules pru clean run-bbb quit-bbb insmod-bbb

PWD := $(shell pwd)
KERNELDIR = $(PWD)/kernel/kernel/
ARCH :=arm
TOOLCHAIN_PREFIX :=arm-linux-gnu-
PASM_V := -V3
PASM := ../pasm/pasm

all: pru modules

bbb: vi-bbb

$(PASM):
	cd ./pasm/pasm_source && ./linuxbuild

pru: $(PASM)
	make -C simulator PASM=$(PASM) PASM_V=$(PASM_V) pru
	make -C pru_driver PASM=$(PASM) PASM_V=$(PASM_V) pru

modules:
	make -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(TOOLCHAIN_PREFIX) M=$(PWD) modules
	mkdir -p modules
	cp -v spi_driver/*.ko hpirq_test/*.ko pru_driver/*.ko simulator/*.ko simulator/insert_sim.sh modules

vi-bbb: 
	make -C visualizer_interface



insmod-bbb:
	insmod ./modules/spi_slave.ko
	insmod ./modules/pruss.ko
	cd modules; sh insert_sim.sh
	cd ..

run-bbb: bbb insmod-bbb
	cd vi-bbb; ./visualizer_interface &

quit-bbb:
	-killall visualizer_interface
	-rmmod simulator
	-rmmod pruss
	-rmmod spi_slave

clean:
	make -C simulator clean
	make -C spi_driver clean
	make -C pru_driver clean

clean-bbb:
	make -C visualizer_interface clean


