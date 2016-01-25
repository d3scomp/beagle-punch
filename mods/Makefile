KERNEL=~/kralovec/punch_press/plant/kernel/kernel-3.8_clean/

all:
	make -C sim all
	make -C $(KERNEL) ARCH=arm CROSS_COMPILE=arm-linux-gnu- M=`pwd` modules

native:
	make -C sim all
	make -C /lib/modules/3.8.13-00770-gfb3e41d-dirty/source M=`pwd` modules


clean:
	make -C pruss clean
	make -C sim clean
	rm -rf .tmp_versions modules.order Module.symvers
