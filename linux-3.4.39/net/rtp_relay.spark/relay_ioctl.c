/******************************************************************************
        (c) COPYRIGHT 2016- by Dinstar technologies Co.,Ltd
                          All rights reserved.
File:relay_ioctl.c
Desc: 本文件用于与用户交互，完成用户的请求，目前用户的请求主要有
1:
Modification history(no, author, date, desc)
spark 16-11-25create file
******************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include "relay.h"
#include <asm/uaccess.h>


static struct class *relay_class;
static struct device *relay_device;
static dev_t dev;
static struct cdev *cdev_p;


static int relay_ioctl_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int relay_ioctl_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long relay_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int data;
	static unsigned int addr = 0;

	if (cmd & 0x01) {	/* write */
		if (copy_from_user(&data, (int *)arg, sizeof(int))) {
			return -EFAULT;
		}
	}

	switch (cmd) {
	default:
		return -EFAULT;
		break;
	}

	if ((cmd & 0x01) == 0) {	/* read */
		if (copy_to_user((int *)arg, &data, sizeof(int))) {
			return -EFAULT;
		}
	}
	return 0;
}

static const struct file_operations relay_ioctl_fops = {
	.owner = THIS_MODULE,
	.open = relay_ioctl_open,	/* open */
	.release = relay_ioctl_close,	/* release */
	.unlocked_ioctl = relay_ioctl,
};


static char *relay_ioctl_devnode(struct device *dev, umode_t *mode)
{
	struct miscdevice *c = dev_get_drvdata(dev);

	if (mode && c->mode)
		*mode = c->mode;
	if (c->nodename)
		return kstrdup(c->nodename, GFP_KERNEL);
	return NULL;
}


int __init relay_ioctl_init(void)
{
	int err;


	printk(KERN_INFO "gio: driver initialized\n");


	if ((err = alloc_chrdev_region(&dev, 0, 1, "dinstar_relay")) < 0) 
	{
		printk(KERN_ERR
		       "gio: Couldn't alloc_chrdev_region, error=%d\n",
		       err);
		return 1;
	}

	cdev_p = cdev_alloc();
	cdev_p->ops = &relay_ioctl_fops;
	err = cdev_add(cdev_p, dev, 1);
	if (err) 
	{
		printk(KERN_ERR
		       "dinstar_relay: Couldn't cdev_add, error=%d\n", err);
		return 1;
	}


	
	relay_class = class_create(THIS_MODULE, "dinstar_relay");
	err = PTR_ERR(relay_class);
	if (IS_ERR(relay_class))
	{
		return -1;
	}

	relay_device = device_create(relay_class, NULL,dev, NULL, "dinstar_relay");
    if(IS_ERR(relay_device))
    {
        printk(KERN_WARNING "device_create error\n");
        return -1;
    }

	return 0;
}

void __exit relay_ioctl_exit(void)
{
	//cdev_del(cdev_p);
	//unregister_chrdev_region(dev, 1);
}


