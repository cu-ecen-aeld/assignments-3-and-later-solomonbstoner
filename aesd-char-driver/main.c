/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Solomon T");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device; // TODO: Qn: why is this global?

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
	filp->private_data = &aesd_device; // save device struct in private_data of filp
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
	filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	// TODO : lock aesd_dev
	struct aesd_dev *ad = (struct aesd_dev *)filp->private_data;
	struct aesd_circular_buffer *cbuf = ad->cbuf;
	// TODO : kmalloc count bytes
	// TODO : find starting entry based on f_pos. Iterate through and copy all strings to the kmalloced buffer
	// TODO : return data from circular buffer using copy_to_user
	// TODO : set retval based on the number of chars read
	// TODO : unlock aesd_dev
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	// We wil ignore f_pos
	// TODO : lock aesd_dev
	struct aesd_dev *ad = (struct aesd_dev *)filp->private_data;
	struct aesd_circular_buffer *cbuf = ad->cbuf;
	// TODO : kmalloc count bytes (or count + prev_count if a temp buf is occupied). kbuf = kzalloc()
	// TODO : copy data from user using copy_from_user
	bool is_term = (kbuf[count-1] == '\n');
	// TODO : if buf ends with '\n' add entry using aesd_circular_buffer_add_entry
	// TODO : else hold it in a temp buf
	// TODO : unlock aesd_dev
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1); // After this call, the aesd dev should be ready to handle all ops from the kernel
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));
	// TODO: init aesddev lock
	aesd_device.cbuf.entry = NULL;
	aesd_device.cbuf.full = false;
	aesd_device.cbuf.in_offs = 0;
	aesd_device.cbuf.out_offs = 0;
	aesd_device.cdev = dev;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

	// TODO: lock aesddev
	// TODO: iterate through the circular buffer and free all kmalloced  memory
	// TODO: unlock and destroy aesddev lock
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
