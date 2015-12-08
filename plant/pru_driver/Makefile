MODULE_NAME = pruss
TESTER_NAME = pruss_tester
COMM_TESTER_NAME = pruss_comm_tester

PRU_PRG = pru_test
PRU_BIN = $(PRU_PRG)_bin.h

COMM_PRU_PRG = pru_comm_test
COMM_PRU_BIN = $(COMM_PRU_PRG)_bin.h

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= $(MODULE_NAME).o \
	$(TESTER_NAME).o \
	$(COMM_TESTER_NAME).o

else

.PHONY: pru

pru: $(PRU_BIN) $(COMM_PRU_BIN)

$(PRU_BIN): $(PRU_PRG).p
	$(PASM) $(PASM_V) $(PRU_PRG).p

$(COMM_PRU_BIN): $(COMM_PRU_PRG).p
	$(PASM) $(PASM_V) $(COMM_PRU_PRG).p

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.disasm $(PRU_BIN) $(COMM_PRU_BIN)

