/**
 *  tafi_chardev.c -- The Amazing Fan Idea driver
 *  Character device implementation.
 * 
 *      (C) 2017 Harindu Perera
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */


#include "tafi_chardev.h"
#include "tafi_ioctl.h"

static struct mutex tafi_chardev_mutex;
static int    tafi_chardev_major_number;                  ///< Stores the device number -- determined automatically
static struct class*  tafi_chardev_class  = NULL; ///< The device-driver class struct pointer
static struct device* tafi_chardev = NULL;

static int     tafi_chardev_open(struct inode *, struct file *);
static int     tafi_chardev_release(struct inode *, struct file *);
static ssize_t tafi_chardev_read(struct file *, char *, size_t, loff_t *);
static ssize_t tafi_chardev_write(struct file *, const char *, size_t, loff_t *);

static int tafi_chardev_uevent(struct device *dev, struct kobj_uevent_env *env);

static struct file_operations fops = {
    .open = tafi_chardev_open,
    .read = tafi_chardev_read,
    .write = tafi_chardev_write,
    .release = tafi_chardev_release,
};

/**
 * Initialize the character device.
 */
int tafi_chardev_init(void) {
    printk(KERN_INFO TAFI_LOG_PREFIX"initializing character device...");

    // Try to dynamically allocate a major number for the device -- more difficult but worth it
    tafi_chardev_major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (tafi_chardev_major_number < 0){
        printk(KERN_INFO TAFI_LOG_PREFIX"EBBChar failed to register a major number\n");
        return tafi_chardev_major_number;
    }
    printk(KERN_INFO TAFI_LOG_PREFIX"EBBChar: registered correctly with major number %d\n", tafi_chardev_major_number
);

    // Register the device class
    tafi_chardev_class = class_create(THIS_MODULE, CLASS_NAME);

    tafi_chardev_class->dev_uevent = tafi_chardev_uevent;

    if (IS_ERR(tafi_chardev_class)){                // Check for error and clean up if there is
        unregister_chrdev(tafi_chardev_major_number, DEVICE_NAME);
        printk(KERN_ALERT TAFI_LOG_PREFIX"failed to register device class\n");
        return PTR_ERR(tafi_chardev_class);          // Correct way to return an error on a pointer
    }
    printk(KERN_INFO TAFI_LOG_PREFIX"device class registered correctly\n");

    // Register the device driver
    tafi_chardev = device_create(tafi_chardev_class, NULL, MKDEV(tafi_chardev_major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(tafi_chardev)){               // Clean up if there is an error
        class_destroy(tafi_chardev_class);           // Repeated code but the alternative is goto statements
        unregister_chrdev(tafi_chardev_major_number, DEVICE_NAME);
        printk(KERN_ALERT TAFI_LOG_PREFIX"failed to create the device\n");
        return PTR_ERR(tafi_chardev);
    }

    mutex_init(&tafi_chardev_mutex);

    printk(KERN_INFO TAFI_LOG_PREFIX"device class created correctly\n"); // Made it! device was initialized
    return 0;
}
 
/**
 * De-initialize chardev and clean up before exit.
 */
void tafi_chardev_exit(void) {

    printk(KERN_INFO TAFI_LOG_PREFIX"remvoving character device...");

    // remove the device
    device_destroy(tafi_chardev_class, MKDEV(tafi_chardev_major_number, 0));
    // unregister the device class
    class_unregister(tafi_chardev_class);
    // remove the device class
    class_destroy(tafi_chardev_class);
    // unregister the major number
    unregister_chrdev(tafi_chardev_major_number, DEVICE_NAME);

    printk(KERN_INFO TAFI_LOG_PREFIX"character device removed.");
}
 
/**
 * Handler called whenever the device is opened.
 */
static int tafi_chardev_open(struct inode *inodep, struct file *filep){
    if (!mutex_trylock(&tafi_chardev_mutex)) {
        printk(KERN_INFO TAFI_LOG_PREFIX"character device already open!");
        return -EBUSY;
    }
    printk(KERN_INFO TAFI_LOG_PREFIX"character device has been opened");
    return 0;
}
 
/**
 * Device read implementation.
 */
static ssize_t tafi_chardev_read(struct file *filep, char *buf, size_t len, loff_t *offset) {

    void *tmp_buf;
    int error_count = 0;

    if (tafi_check_bounds(len, *offset)) {
        return -EFAULT;
    }

    tmp_buf = kmalloc(len, GFP_KERNEL);
    if (tmp_buf == NULL) {
        printk(KERN_ERR TAFI_LOG_PREFIX"failed to allocate memory for character device copy buffer.");
        return -ENOMEM;
    }

    tafi_get_color_data(tmp_buf, len, *offset);

    // copy_to_user has the format ( * to, *from, size) and returns 0 on success
    error_count = copy_to_user(buf, tmp_buf, len);

    if (error_count == 0) {
        // if true then have success
        printk(KERN_INFO TAFI_LOG_PREFIX"read data");
    }
    else {
        printk(KERN_INFO TAFI_LOG_PREFIX"failed to send %d characters to users pace", error_count);
        return -EFAULT;
    }
}
 
/**
 * Write data to device.
 */
static ssize_t tafi_chardev_write(struct file *filep, const char *buf, size_t len, loff_t *offset) {
   
    if (tafi_check_bounds(len, *offset)) {
        return -EFAULT;
    }

    tafi_set_color_data(buf, len, *offset);

    printk(KERN_INFO TAFI_LOG_PREFIX"received %d from user space", len);
    return len;
}
 
/**
 * Release chardev after closure
 */
static int tafi_chardev_release(struct inode *inodep, struct file *filep) {
   mutex_unlock(&tafi_chardev_mutex);
   printk(KERN_INFO TAFI_LOG_PREFIX"Device successfully closed\n");
   return 0;
}

/**
 * Configure character device file permissions.
 */
static int tafi_chardev_uevent(struct device *dev, struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}