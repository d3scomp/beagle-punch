#include <linux/init.h>
#include <linux/module.h>

#include <asm/current.h>
#include <linux/sched.h>

#include <linux/gpio.h>
#include <plat/dmtimer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/kthread.h>

#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>

//#define DEBUG_SPI	1
#define PRU_SPI		1
#include "consumer.h"

#include "simulation.h"
#include "json.h"
#include "userspace_interface.h"

MODULE_LICENSE("Dual BSD/GPL");

static struct cdev cdev;/* character device representing the simulator in userspace */
static dev_t cdev_num;/* device number */

extern const char * module_name;
extern struct pp_t pp_sim;

#ifdef DEBUG_SPI
extern u8 * spi_buf;
extern unsigned int * sb_counter;
#endif

#define FILE_BUF_SIZE	2048

static ssize_t cdev_read(struct file * filp, char __user * buf, size_t count, loff_t * f_pos)
{
	char * msg_buf = filp->private_data;
	int msg_size = json_print_status(msg_buf, FILE_BUF_SIZE, &pp_sim);
	if (count < msg_size)
	{
		return -EPIPE;
	}

	if (copy_to_user(buf, msg_buf, msg_size))
	{
		return -EFAULT;
	}

	return msg_size;
}

static ssize_t cdev_write(struct file * filp, const char __user * buf, size_t count, loff_t * f_pos)
{
	char local_buf[101];
	int err;

	if (count > 100)
	{
		return -EPIPE;
	}

	if (copy_from_user(local_buf, buf, count))
	{
		return -EFAULT;
	}

	local_buf[count] = 0;
	err = json_apply_command(&pp_sim, local_buf);
	if (err)
	{
		return err;
	}

	return count;
}

static int cdev_open(struct inode * in, struct file * filp)
{
	char * buf = (char *)kmalloc(FILE_BUF_SIZE * sizeof(char), GFP_KERNEL);
	filp->private_data = buf;
	if (buf == 0)
	{
		return -ENOMEM;
	}

	return 0;
}

static int cdev_release(struct inode * in, struct file * filp)
{
	kfree(filp->private_data);
	return 0;
}

#ifdef DEBUG_SPI
static long spi_debug_ioctl(unsigned int command, unsigned long param)
{
	size_t counter = sb_counter;
	if (copy_to_user((__user char *)param, spi_buf, counter))
	{
		return -EFAULT;
	}

	return counter;
}
#endif

/* ioctl without the Big Kernel Lock, used for debugging only */
static long cdev_ioctl(struct file * f, unsigned int command, unsigned long param)
{
	switch (command)
	{
		#ifdef DEBUG_SPI
		case IOCTL_SPI_LOG:
			return spi_debug_ioctl(command, param);
		#endif
	}

	return -EFAULT;
}

static struct file_operations cdev_fops = {
	.owner = THIS_MODULE,
	.read = cdev_read,
	.write = cdev_write,
	.open = cdev_open,
	.release = cdev_release,
	.unlocked_ioctl = cdev_ioctl,
};

int userinterface_setup(void)
{
	int err;
	err = alloc_chrdev_region(&cdev_num, 0, 1, module_name);
	if (err)
	{
		printk(KERN_ALERT "Unable to allocate char device region.");
		return err;
	}

	cdev_init(&cdev, &cdev_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &cdev_fops;
	err = cdev_add(&cdev, cdev_num, 1);
	if (err)
	{
		printk(KERN_ALERT "Unable to add char device.");
		unregister_chrdev_region(cdev_num, 1);
		return err;
	}

	return 0;
}

void userinterface_release(void)
{
	cdev_del(&cdev);
	unregister_chrdev_region(cdev_num, 1);
}

