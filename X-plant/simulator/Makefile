MODULE_NAME=simulator
PRU_PRG=spi_controller
PRU_BIN=$(PRU_PRG)_bin.h

ifneq ($(KERNELRELEASE),)
# call from kernel build system

$(MODULE_NAME)-objs := xorshift.o simulation.o main.o userspace_interface.o json.o

obj-m	:= $(MODULE_NAME).o

else

.PHONY: pru

pru: $(PRU_BIN)

$(PRU_BIN): $(PRU_PRG).p
	$(PASM) $(PASM_V) $(PRU_PRG).p

tester: tester.c simulation.c json.c xorshift.c
	gcc -o tester -Wall tester.c simulation.c json.c xorshift.c

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.disasm tester $(PRU_BIN)

