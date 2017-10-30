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
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128}
    },
    {
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
    },{
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
    },
    {
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255}
    },
    {
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
        {128, 128, 128},
        {255, 128, 128},
        {128, 255, 128},
        {128, 128, 255},
        {128, 128, 128},
    }
};

// Thread and timer

// Global array for color data to be sent to LEDs.
static unsigned char tafi_color_data_buf[TAFI_SECTOR_COUNT][TAFI_SECTOR_LED_COUNT][TAFI_LED_COLOR_FIELD_COUNT];

// Dirty flag to see if we really need to write a new frame.
static bool tafi_color_data_dirty;

// Mutex to control access to color data buffer and the dirty flag.
static struct mutex tafi_color_data_mutex;

// The global task.
struct task_struct *tafi_task;

static int tafi_thread(void *data) {
    // unsigned char B = 0;
    int i = 0;
    int j = 0;
    int k = 0;
    unsigned char buf[TAFI_SECTOR_COUNT][TAFI_SECTOR_LED_COUNT][TAFI_LED_COLOR_FIELD_COUNT];
    bool dirty = true;

    while (i < TAFI_SECTOR_COUNT) {
        j = 0;
        while (j < TAFI_SECTOR_LED_COUNT) {
            k = 0;
            while (k < TAFI_LED_COLOR_FIELD_COUNT) {
                buf[i][j][k] = BUF[i][j][k];
            }
            j++;
        }
        i++;
    }

    printk(KERN_INFO TAFI_LOG_PREFIX"thread running.");
    while (!kthread_should_stop()) {
        // B = 0;
        // spi_write(tafi_spi_device, &B, 1);
        // spi_write(tafi_spi_device, BUF[i], 20*3);
        // B = 128;
        // spi_write(tafi_spi_device, &B, 1);
        // i++;
        // i = i%5;
        // msleep(1);

        //mutex_acquire(&tafi_color_data_mutex);
        tafi_frame_begin();
        i = 0;
        // while (i < TAFI_SECTOR_COUNT) {
        //     spi_write(tafi_spi_device, &BUF[i%5], TAFI_SECTOR_BUF_LEN);
        //     i++;
        // }
        tafi_data_write(&buf, TAFI_DATA_BUF_LEN);
        tafi_frame_end();
        msleep(20);
    }
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

    // start thread
    tafi_thread_init();

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