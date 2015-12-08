
#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include <linux/types.h>

struct mcspi_slave_device;

struct mcspi_slave_device * mcspi_slave_get_by_name(const char * name);

struct mcspi_slave_device * mcspi_slave_get_one(void);

int mcspi_slave_get_irq(const struct mcspi_slave_device * slave);

int mcspi_slave_free(struct mcspi_slave_device * slave);

// IRQ register masks - common for IRQSTATUS and IRQENABLE
#define IRQ_EOW_MASK    0x20000 // end of word count event
#define IRQ_TX0_EMPTY_MASK      0x1 // channel 0 empty event
#define IRQ_TX0_UNDERFLOW_MASK  0x2 // channel 0 underflow event
#define IRQ_RX0_FULL_MASK       0x4 // channel 0 full event
#define IRQ_RX0_OVERFLOW_MASK   0x8 // channel 0 overflow event

void mcspi_slave_reset_communication(struct mcspi_slave_device * slave);

int mcspi_slave_enable(struct mcspi_slave_device * slave, u8 word_length);

void mcspi_slave_disable(struct mcspi_slave_device * slave);

void mcspi_slave_irq_disable(struct mcspi_slave_device * slave, u32 irq_mask);

void mcspi_slave_irq_enable(struct mcspi_slave_device * slave, u32 irq_mask);

u32 mcspi_slave_irq_status(struct mcspi_slave_device * slave);

void mcspi_slave_irq_status_clear(struct mcspi_slave_device * slave, u32 mask);

bool mcspi_slave_tx_empty(struct mcspi_slave_device * slave);

bool mcspi_slave_rx_full(struct mcspi_slave_device * slave);

u32 mcspi_slave_read_rx(struct mcspi_slave_device * slave);

void mcspi_slave_write_tx(struct mcspi_slave_device * slave, u32 value);


int mcspi_slave_test_start(struct mcspi_slave_device * slave);

void mcspi_slave_test_stop(struct mcspi_slave_device * slave);

#endif

