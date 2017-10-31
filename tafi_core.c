/**
 *  tafi.c -- The Amazing Fan Idea driver
 *  Main module code with thread and ioctl code.
 * 
 *      (C) 2017 Harindu Perera
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

// Basic kernel module headers
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>

// GPIO header
#include <linux/gpio.h>

// SPI header
#include <linux/spi/spi.h>

// kThread/timer headers
#include <linux/kthread.h>
#include <linux/delay.h>

// For kmalloc
#include <linux/slab.h>

#include "tafi_common.h"
#include "tafi_ioctl.h"
#include "tafi_bus.h"

// Thread settings
#define TAFI_KTHREAD_NAME "tafi_main"
#define TAFI_KTHREAD_SCHEDULER_PRIORITY MAX_RT_PRIO - 50
#define TAFI_KTHREAD_PRIORITY 45

static unsigned char BUF[5][TAFI_SECTOR_LED_COUNT][TAFI_LED_COLOR_FIELD_COUNT] = {
    {
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128},
        {255, 128, 128}
    },
    {
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128},
        {128, 255, 128}
    },
    {
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255},
        {128, 128, 255}
    }
};

// Thread and timer

////////// DO NOT MANIPULATE THE VARIABLES BELOW DIRECTLY ////////////////
// Global array for color data to be sent to LEDs.
static unsigned char tafi_color_data_buf[TAFI_SECTOR_COUNT][TAFI_SECTOR_LED_COUNT][TAFI_LED_COLOR_FIELD_COUNT];

// Dirty flag to see if we really need to write a new frame.
static bool tafi_color_data_dirty;

// Mutex to control access to color data buffer and the dirty flag.
static struct mutex tafi_color_data_mutex;

////////// DO NOT MANIPULATE THE VARIABLES ABOVE DIRECTLY ////////////////

// The global task.
struct task_struct *tafi_task;

/**
 * Set the internal color data buffer contents.
 * Unsafe to call without bounds checking.
 */
static void tafi_set_color_data(void *buf) {
    mutex_lock(&tafi_color_data_mutex);
    memcpy(tafi_color_data_buf, buf, TAFI_DATA_BUF_LEN);
    tafi_color_data_dirty = true;
    mutex_unlock(&tafi_color_data_mutex);
}

/**
 * Copy the color data buffer if dirty flag is set,
 * and clear the dirty flag.
 */
static bool tafi_cpy_data_and_reset_if_dirty(void *buf) {
    bool ret = false;
    //mutex_lock(&tafi_color_data_mutex);
    //if (tafi_color_data_dirty) {
        memcpy(buf, tafi_color_data_buf, TAFI_DATA_BUF_LEN);
        //tafi_color_data_dirty = false;
        ret = true;
    //}
    //mutex_unlock(&tafi_color_data_mutex);
    return ret;
}

static int tafi_thread(void *data) {
    
    // Internal buffer used to write the data to SPI.
    // Stops blocking the actual buffer.
    void *buf;
    unsigned char reset = 0;
    unsigned char term = 0xff;
    int i = 0;

    buf = kmalloc(TAFI_DATA_BUF_LEN, GFP_KERNEL);
    if (buf == NULL) {
        printk(KERN_ERR TAFI_LOG_PREFIX"failed to allocate memory for internal display buffer.");
        return -ENOMEM;
    }

    printk(KERN_INFO TAFI_LOG_PREFIX"thread running.");

    // check if the thread should stop
    while (!kthread_should_stop()) {
        if (tafi_cpy_data_and_reset_if_dirty(buf)) {
            tafi_frame_begin();
            //tafi_data_write(&reset, 1);
            //tafi_data_write(buf+(i*TAFI_SECTOR_BUF_LEN), TAFI_SECTOR_BUF_LEN);
            tafi_data_write(buf, TAFI_DATA_BUF_LEN);
            //tafi_data_write(&term, 1);
            tafi_frame_end();
        }
        //i++;
        //i = i%150;
        usleep_range(92600, 92600);
    }

    kfree(buf);

    printk(KERN_INFO TAFI_LOG_PREFIX"thread returning.");
    return 0;
}

static int tafi_thread_init(void) {
    printk(KERN_INFO TAFI_LOG_PREFIX"thread starting...");
    tafi_task = kthread_run(tafi_thread, NULL, TAFI_KTHREAD_NAME);
    if (IS_ERR(tafi_task)) {
        printk(KERN_INFO TAFI_LOG_PREFIX"thread starting failed.");
        return PTR_ERR(tafi_task);
    }
    printk(KERN_INFO TAFI_LOG_PREFIX"thread started.");
    return 0;
}

static void tafi_thread_exit(void) {
    int ret;
    printk(KERN_INFO TAFI_LOG_PREFIX"thread stopping...");
    ret = kthread_stop(tafi_task);
    if (ret != -EINTR) {
        printk(KERN_INFO TAFI_LOG_PREFIX"thread stopped.");
    }
}


// MODULE INIT/EXIT HANDLERS

/**
 * Module init handler.
 */
static int __init tafi_init(void) {

    int ret;
    int i = 0;

    printk(KERN_INFO TAFI_LOG_PREFIX"staring...");
    // stuff to do

    // init mutex
    mutex_init(&tafi_color_data_mutex);

    // init GPIO
    tafi_gpio_init();

    // init SPI
    ret = tafi_spi_init();
    if (ret < 0) {
        tafi_gpio_exit();
        return ret;
    }

    // init the initial data buffer
    // this serves as a diagnostic screen as well as a security measure
    // to prevent kernel space memory leaking into user space via an 
    // initial read of the buffer.
    while (i < TAFI_SECTOR_COUNT) {
        memcpy(tafi_color_data_buf[i], BUF[i%3], TAFI_SECTOR_BUF_LEN);
        i++;
    }

    // start thread
    ret = tafi_thread_init();
    if (ret < 0) {
        tafi_gpio_exit();
        tafi_spi_exit();
        return ret;
    }

    // create sysfs ctl device
    // init FB (maybe)
    printk(KERN_INFO TAFI_LOG_PREFIX"staring done.");
    return 0;
}

/**
 * Module exit handler.
 */
static void __exit tafi_exit(void) {
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping...");
    // stuff to do
    // stop thread
    tafi_thread_exit();

    // de-init GPIO and SPI
    tafi_gpio_exit();

    // de-init SPI device
    tafi_spi_exit();

    // remove sysfs ctl device
    // de-init FB (maybe)
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping done.");
}

// Register init/exit handlers
module_init(tafi_init);
module_exit(tafi_exit);

MODULE_DESCRIPTION(TAFI_DESCRIPTION);
MODULE_AUTHOR(TAFI_AUTHOR);
MODULE_LICENSE(TAFI_LICENSE);