#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include <linux/types.h>

struct mcspi_slave_device;

struct mcspi_slave_device * mcspi_slave_get_one(void);

int mcspi_slave_free(struct mcspi_slave_device * slave);

int mcspi_slave_enable(struct mcspi_slave_device * slave, u8 word_length);

void mcspi_slave_disable(struct mcspi_slave_device * slave);

#endif

