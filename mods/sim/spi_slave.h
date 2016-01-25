
#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include <linux/types.h>

struct mcspi_slave_device;

struct mcspi_slave_device * mcspi_slave_get_one(void);

int mcspi_slave_free(struct mcspi_slave_device * slave);

// IRQ register masks - common for IRQSTATUS and IRQENABLE
#define IRQ_EOW_MASK    0x20000 // end of word count event
#define IRQ_TX0_EMPTY_MASK      0x1 // channel 0 empty event
#define IRQ_TX0_UNDERFLOW_MASK  0x2 // channel 0 underflow event
#define IRQ_RX0_FULL_MASK       0x4 // channel 0 full event
#define IRQ_RX0_OVERFLOW_MASK   0x8 // channel 0 overflow event

int mcspi_slave_enable(struct mcspi_slave_device * slave, u8 word_length);

void mcspi_slave_disable(struct mcspi_slave_device * slave);

#endif

