.PHONY: all native clean

all:
	make -C sim all
	make -C ../kernel/kernel ARCH=arm CROSS_COMPILE=arm-linux-gnu- M=`pwd` modules

native:
	make -C sim all
	make -C /lib/modules/3.8.13-00770-*-dirty/source M=`pwd` modules

clean:
	make -C pruss clean
	make -C sim clean
	rm -rf .tmp_versions modules.order Module.symvers
