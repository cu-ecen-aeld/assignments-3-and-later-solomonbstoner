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

#include <asm-generic/errno-base.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesd-circular-buffer.h"
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Solomon T");
MODULE_LICENSE("Dual BSD/GPL");

int aesd_open(struct inode *, struct file *);
int aesd_open(struct inode *, struct file *);
ssize_t aesd_read(struct file *, char __user *, size_t, loff_t *);
ssize_t aesd_write(struct file *, const char __user *, size_t, loff_t *);
int aesd_init_module(void);
void aesd_cleanup_module(void);
int aesd_release(struct inode *, struct file *);
loff_t aesd_llseek (struct file *filp, loff_t offset, int whence);
long aesd_unlocked_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset);

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
	if (count < 1)
	{
		return retval;
	}
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	down(&aesd_device.lock); // lock aesd_dev
	// struct aesd_dev *ad = (struct aesd_dev *)filp->private_data;
	struct aesd_circular_buffer *cbuf = &aesd_device.cbuf;
	char *ret_str = (char *) kzalloc(count+1, GFP_KERNEL); // +1 for the terminating null
	if (ret_str == NULL)
	{
		// Error allocating memory
		retval = -ENOMEM;
		goto out;
	}
	// Find starting entry based on f_pos. Iterate through and copy all strings to the kmalloced buffer
	// i = current relative char offset so far (relative from f_pos)
	// j = offset in the entry where f_pos points to
	// n = size of the current entry
	// n_b_to_cpy = number of bytes to copy over from this entry
	// Since count > 0, the first entry will always have something for us to copy over
	size_t i = 0, j = 0;
	struct aesd_buffer_entry *ent = aesd_circular_buffer_find_entry_offset_for_fpos(cbuf, *(f_pos), &j);
	if (ent == NULL)
	{
		retval = 0;
		goto out;
	}
	size_t n = ent->size;
	size_t n_b_to_cpy = count > n ? n : count; // Copy partial if count < total length of this entry's string
	memcpy(ret_str, ent->buffptr+j, n_b_to_cpy);
	i += n_b_to_cpy;
    retval = i;
	while (i < count)
   	{
		ent = aesd_circular_buffer_find_entry_offset_for_fpos(cbuf, *(f_pos)+i, &j);
		if (ent == NULL)
		{
            // Nothing else to copy. Copy what we have to userland
			break;
		}
		n = ent->size;
		n_b_to_cpy = (count - i) > n ? n : (count - i); // Copy partial if (count - i) < total length of this entry's string
		memcpy(ret_str+i, ent->buffptr, n_b_to_cpy);
		i += n_b_to_cpy;
		retval = i; // Update the number of characters read
	}
    PDEBUG("read: returning string: %s" , ret_str);
	if (copy_to_user(buf, ret_str, count) != 0) // Return data from circular buffer using copy_to_user
	{
		// Error copying
		retval = -EFAULT;
		goto out;
	}
out:
	kfree(ret_str); // Safe to kfree NULL
	up(&aesd_device.lock);// unlock aesd_dev
    *f_pos += retval; // Update the position of the file
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	// We wil ignore f_pos
	down(&aesd_device.lock); // lock aesd_dev
	// struct aesd_dev *ad = (struct aesd_dev *)filp->private_data;
	// struct aesd_circular_buffer *cbuf = ad->cbuf;
	struct aesd_buffer_entry *tmp_kbuf = &aesd_device.tmp_kbuf;
    PDEBUG("write: kzallocating  %ld bytes", count + tmp_kbuf->size + 1);
	char *kbuf = (char *) kzalloc(count + tmp_kbuf->size + 1, GFP_KERNEL); // tmp_kbuf->size != 0 if there is an existing temporary string. +1 for terminating null
	if (kbuf == NULL)
	{
		// Return -ENOMEM
		retval = -ENOMEM;
		goto out;
	}
	if (tmp_kbuf->size > 0 && tmp_kbuf->buffptr != NULL)
	{
		strcpy(kbuf, tmp_kbuf->buffptr); // Copy the existing string to the new buffer
		kfree(tmp_kbuf->buffptr); // We dont need the existing string buffer anymore
	}
	if (copy_from_user(kbuf + tmp_kbuf->size, buf, count) != 0)
	{
		// Error copying
		retval = -EFAULT;
		goto free_kbuf;
	}
    PDEBUG("write: kbuf = %s", kbuf);
	retval = count; // We successfully wrote count number of bytes
	tmp_kbuf->buffptr = kbuf;
	tmp_kbuf->size += count;
    PDEBUG("write: tmp_kbuf->size = %ld, tmp_kbuf->buffptr =  %s", tmp_kbuf->size, kbuf);
	bool is_term = (tmp_kbuf->buffptr[tmp_kbuf->size-1] == '\n');
	if (is_term)
	{
		// If kbuf ends with '\n' add entry using aesd_circular_buffer_add_entry
		aesd_circular_buffer_add_entry(&aesd_device.cbuf, tmp_kbuf);
		PDEBUG("write: ADDED TO CIRC BUF: tmp_kbuf->size = %ld, tmp_kbuf->buffptr =  %s", tmp_kbuf->size, kbuf);
		memset(tmp_kbuf, 0, sizeof(struct aesd_buffer_entry)); // Clear memory after it has been copied over
		goto out; // No need to clear kbuf
	}
	else
	{
		goto out; // No need to clear kbuf
	}
free_kbuf:
	kfree(kbuf);
out:
	up(&aesd_device.lock); // unlock aesd_dev
    return retval;
}

loff_t aesd_llseek (struct file *filp, loff_t offset, int whence)
{
    // TODO
    return 0;
}

long aesd_unlocked_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    // TODO: handle ioctl and call aesd_adjust_file_offset
    return 0;
}

/**
 * Adjust the file offset (f_pos) parameter of @param filp based on the location specified by
 * @param write_cmd (the zero referenced command to locate)
 * and @param write_cmd_offset (the zero referenced offset into the command)
 * @return 0 if successful, negative if error occurred
 *  -ERESTARTSYS if mutex could not be obtained
 *  -EINVAL if write command or write_cmd_offset was out of range
 */
static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    // TODO
    return 0;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
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
    memset(&aesd_device,0,sizeof(struct aesd_dev)); // cbuf and tmp_kbuf are all cleared too
	sema_init(&aesd_device.lock, 1); // init aesddev semaphore as a mutex
	down(&aesd_device.lock);
	aesd_device.cbuf.full = false;
	aesd_device.tmp_kbuf.buffptr = NULL;
	aesd_device.tmp_kbuf.size = 0;
	up(&aesd_device.lock);
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

	down(&aesd_device.lock); // lock aesddev
	// iterate through the circular buffer and free all kmalloced  memory
	size_t i = 0; // index
	struct aesd_buffer_entry *ptr = NULL;
	AESD_CIRCULAR_BUFFER_FOREACH(ptr, &aesd_device.cbuf, i)
	{
		kfree(ptr->buffptr); // Safe to free null ptr
	}
	kfree(aesd_device.tmp_kbuf.buffptr); // Safe to free null ptr
	up(&aesd_device.lock); // unlock aesddev lock
	// TODO Is there a need to destroy the lock?
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
