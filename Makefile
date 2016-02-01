.PHONY: all mods dist clean

all: dist

pasm/pasm:
	cd pasm/pasm_source && sh linuxbuild

mods:	pasm/pasm
	make -C mods all

dist:	mods
	mkdir -p dist/root/punchpress/mods
	cp -f mods/modules.order mods/Module.symvers mods/pruss/pruss.ko mods/sim/spi_slave.ko mods/sim/sim.ko dist/root/punchpress/mods

clean:
	make -C mods clean


#
#.PHONY: all pc bbb clean_pc clean_bbb run_bbb quit_bbb insert_modules modules clean
#
#PC_FOLDERS= \
#	./simulator \
#	./spi_driver \
#	./pru_driver
#
#BBB_FOLDERS= \
#	./visualizer_interface
#
#
#all: pc bbb
#
#include ./kmodule_setup.mk
#
#PASM=./pasm/pasm
#
#$(PASM):
#	cd ./pasm/pasm_source && ./linuxbuild
#
#modules: $(PASM)
#	for i in $(PC_FOLDERS); do \
#		make -C "$$i" pru; \
#		echo; \
#	done
#	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(TOOLCHAIN_PREFIX) M=$(PWD) LDDINCDIR=$(PWD)/include modules
#
#pc: modules
#	make -C ./visualizer_interface
#
#bbb:
#	make -C ./visualizer_interface
#
#insert_modules:
#	insmod ./spi_driver/spi_slave.ko
#	insmod ./pru_driver/pruss.ko
#	cd ./simulator; sh ./insert_sim.sh
#
#run_bbb: bbb insert_modules
#	cd ./visualizer_interface; ./visualizer_interface &
#
#quit_bbb:
#	-killall visualizer_interface
#	-rmmod simulator
#	-rmmod pruss
#	-rmmod spi_slave
#
#clean_pc:
#	for i in $(PC_FOLDERS); do \
#		make -C "$$i" clean; \
#		echo; \
#	done
#
#clean_bbb:
#	for i in $(BBB_FOLDERS); do \
#		make -C "$$i" clean; \
#		echo; \
#	done
#
#clean: clean_pc clean_bbb
