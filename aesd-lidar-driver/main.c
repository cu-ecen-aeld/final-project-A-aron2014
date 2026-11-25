/**
 * @file aesdlidar.c
 * @brief Functions and data related to the AESD lidar driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 * As well as Dan Walkes' aesdchar.c implementation for assignment 9.
 * @author Aaron Pineda
 * @date 2026-11-24
 * 
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h> //for kmalloc and krealloc and kfree
#include <linux/uaccess.h> //for copy_to_user and copy_from_user
#include <linux/string.h> //memchr access

#include "aesdchar.h"
#include "lidar_ioctl.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Aaron Pineda"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
static struct class *lidar_class;
static struct device *lidar_dev;

static char *lidar_devnode(const struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0444; //make this read only since we are reading from a sensor.
    return NULL;
}

//Helper function to keep track of the number of entries in teh circular buffer. 
static uint8_t aesd_entry_count(struct aesd_circular_buffer *buffer){
    if(buffer->full)
        return AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    return(buffer->in_offs+AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-buffer->out_offs)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
}

//Helper function to calculate the total size of the data in the circular buffer.
static loff_t aesd_total_size(struct aesd_circular_buffer *buffer){
    loff_t total=0;
    uint8_t i,n = aesd_entry_count(buffer);
    for (i=0;i<n;i++){
        total+=buffer->entry[(buffer->out_offs+i)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
    }
    return total;
}

static loff_t aesd_llseek(struct file *filp, loff_t offset, int where){
    struct aesd_dev *dev = filp->private_data;
    loff_t ret;
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    ret=fixed_size_llseek(filp, offset, where, aesd_total_size(&dev->buffer));
    mutex_unlock(&dev->lock);
    return ret;
}

static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct lidar_stats sensor_stats;

    if( _IOC_TYPE(cmd)!=LIDAR_IOC_MAGIC || _IOC_NR(cmd)>LIDAR_IOC_MAXNR)
        return -ENOTTY;
    switch(cmd){
        case LIDAR_IOC_GET_STATS:
            memset(&sensor_stats,0, sizeof(sensor_stats)); //want to zero out the struct by default
            if(copy_to_user((void __user *)arg, &sensor_stats, sizeof(sensor_stats)))
                return -EFAULT;
            return 0;
        case LIDAR_IOC_FLUSH:
            //Flush the circular buffer and reset the partial write buffer
            {
                struct aesd_dev *dev = filp->private_data;
                if (mutex_lock_interruptible(&dev->lock))
                    return -ERESTARTSYS;
                aesd_circular_buffer_init(&dev->buffer);
                kfree(dev->partial.buffptr);
                dev->partial.buffptr=NULL;
                dev->partial.size=0;
                mutex_unlock(&dev->lock);
                return 0;
            }
        
        default:
            return -ENOTTY; 
    }
}

static int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;

    dev = container_of(inode->i_cdev,struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

static int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    //nothing to release here 
    return 0;
}

static ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	    ssize_t retval = 0;
    	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    	/**
     	* TODO: handle read
    	*/
	struct aesd_dev *dev=filp->private_data;
	struct aesd_buffer_entry *entry;
	size_t entry_offset = 0;
	size_t bytes;
	
	if(mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
	entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, (size_t)*f_pos, &entry_offset);

	if(!entry)
		goto out; //return 0 for EOF
	bytes = entry->size-entry_offset;
	if(bytes>count)
		bytes=count;
	
	if(copy_to_user(buf, entry->buffptr+entry_offset, bytes)){
		retval=-EFAULT;
		goto out;
	}

	*f_pos+=bytes;
	retval=bytes;

out:
	mutex_unlock(&dev->lock);
   	return retval;
}

static ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    //copy file pointer data
    struct aesd_dev *dev =filp->private_data;
    char *newbuf;

    if(count==0)
	    return 0;
    if(mutex_lock_interruptible(&dev->lock))
	    return -ERESTARTSYS;
    //grow the buffer to allow for new bytes in the command
    newbuf=krealloc(dev->partial.buffptr, dev->partial.size+count, GFP_KERNEL);
    if(!newbuf)
	    goto out; //old partial buffer is still valid so no need to discard
    dev->partial.buffptr=newbuf;//reallocate buffer data
    if(copy_from_user(newbuf+dev->partial.size, buf, count))
    {
	    retval=-EFAULT;
	    goto out;
    }
    dev->partial.size+=count; //increase buffer size to match allocated data
    

    //if the command is completed then add it to the circular buffer
    if(memchr(dev->partial.buffptr, '\n', dev->partial.size)){
	    const char *old=NULL;
	    if(dev->buffer.full)
		    old=dev->buffer.entry[dev->buffer.in_offs].buffptr;
	    aesd_circular_buffer_add_entry(&dev->buffer, &dev->partial);
	    kfree(old);//free up the overwritten entry
	    dev->partial.buffptr=NULL;
	    dev->partial.size=0;
    }
    retval=count;
out:
    mutex_unlock(&dev->lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,//adding ioctl to the file operations
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }

    return err;
}

static int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdlidar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.buffer);
    mutex_init(&aesd_device.lock);
    result=aesd_setup_cdev(&aesd_device);
    if( result ) {
        goto fail;
    }

    lidar_class = class_create("aesdlidar");
    if(IS_ERR(lidar_class)){
        result=PTR_ERR(lidar_class);
        goto fail_cdev;
    }
    lidar_class->devnode=lidar_devnode;

    lidar_dev = device_create(lidar_class, NULL, dev, NULL, "lidar0");
    if(IS_ERR(lidar_dev)){
        result=PTR_ERR(lidar_dev);
        goto fail_class;
    }
    return 0;

fail_class:
    class_destroy(lidar_class);
fail_cdev:
    cdev_del(&aesd_device.cdev);
fail:
    unregister_chrdev_region(dev, 1);
    return result;
}

static void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    
    uint8_t index;
    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    struct aesd_buffer_entry *entry;

    device_destroy(lidar_class, devno);
    class_destroy(lidar_class);
    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index)
    {
		    kfree(entry->buffptr);
    }

    kfree(aesd_device.partial.buffptr);
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
