/**
 *  tafi.c -- The Amazing Fan Idea driver
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
#include <linux/kernel.h>
#include <linux/device.h>

// GPIO header
#include <linux/gpio.h>

// SPI header
#include <linux/spi/spi.h>

// kThread/timer headers
#include <linux/kthread.h>
#include <linux/delay.h>

#define TAFI_LOG_PREFIX "Desperate Housewife: "

#define TAFI_DRIVER_NAME "tafi"
#define TAFI_DESCRIPTION "The Amazing Fan Idea PoV Display Driver"
#define TAFI_AUTHOR "Harindu Perera <r.a.h.perera@student.utwente.nl>"
#define TAFI_LICENSE "GPL v2"

// GPIO pin for sending the frame start/end signal
#define TAFI_GPIO_FRAME_START_PIN 17

// SPI settings
#define TAFI_SPI_BUS_NUM 0
#define TAFI_SPI_BUS_SPEED_HZ 10000000 // 10 MHz
#define TAFI_SPI_CHIP_SELECT 0
#define TAFI_SPI_MODE SPI_MODE_0
#define TAFI_SPI_BITS_PER_WORD 8

// Thread settings
#define TAFI_KTHREAD_NAME "tafi_main"
#define TAFI_KTHREAD_SCHEDULER_PRIORITY MAX_RT_PRIO - 50
#define TAFI_KTHREAD_PRIORITY 45

/**
 * Initialize GPIO pins for use.
 */
static void tafi_gpio_init(void) {
  
  printk(KERN_INFO TAFI_LOG_PREFIX"starting GPIO...");
  
  // init GPIO pin for frame signal
  gpio_request(TAFI_GPIO_FRAME_START_PIN, "TAFI_GPIO_FRAME_START_PIN");
  gpio_direction_output(TAFI_GPIO_FRAME_START_PIN, 0);
  gpio_set_value(TAFI_GPIO_FRAME_START_PIN, 0);

  printk(KERN_INFO TAFI_LOG_PREFIX"started GPIO.");
}

/**
 * De-initialize the GPIO pins before exit.
 */
static void tafi_gpio_exit(void) {
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping GPIO...");
    gpio_free(TAFI_GPIO_FRAME_START_PIN);
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping GPIO.");
}

static void tafi_begin_frame(void) {
    gpio_set_value(TAFI_GPIO_FRAME_START_PIN, 1);
}

static void tafi_end_frame(void) {
    gpio_set_value(TAFI_GPIO_FRAME_START_PIN, 0);
} 


// SPI

// The global SPI device
static struct spi_device *tafi_spi_device;

/**
 * Initializes SPI device.
 */
static int tafi_spi_init(void) {
    
    int ret;
    struct spi_master *master;
    struct device *temp_device;
    char temp_device_buf[20];

    printk(KERN_INFO TAFI_LOG_PREFIX"starting SPI...");

    master = spi_busnum_to_master(TAFI_SPI_BUS_NUM);
    if (!master) {
        printk(KERN_INFO TAFI_LOG_PREFIX"SPI master init failed.");
        return -ENODEV;
    }

    tafi_spi_device = spi_alloc_device(master);
    if (!tafi_spi_device) {
        put_device(&master->dev);
        printk(KERN_INFO TAFI_LOG_PREFIX"SPI device alloc failed.");
        return -ENODEV;
    }
  
    tafi_spi_device->chip_select = TAFI_SPI_CHIP_SELECT;
    snprintf(temp_device_buf, sizeof(temp_device_buf), "%s.%u", dev_name(&tafi_spi_device->master->dev), tafi_spi_device->chip_select);
  
    // Attempt to find the device, and if found hijack it.
    temp_device = bus_find_device_by_name(tafi_spi_device->dev.bus, NULL, temp_device_buf);
    if (temp_device) {
        printk(KERN_INFO TAFI_LOG_PREFIX"found other SPI device (probably spidev), forcing removal.");
        spi_unregister_device(to_spi_device(temp_device));
        spi_dev_put(to_spi_device(temp_device));
    }

    tafi_spi_device->max_speed_hz = TAFI_SPI_BUS_SPEED_HZ;
    tafi_spi_device->mode = TAFI_SPI_MODE;
    tafi_spi_device->bits_per_word = TAFI_SPI_BITS_PER_WORD;
    tafi_spi_device->irq = -1;
    tafi_spi_device->controller_state = NULL;
    tafi_spi_device->controller_data = NULL;
    strcpy(tafi_spi_device->modalias, TAFI_DRIVER_NAME);

    tafi_spi_device->bits_per_word = TAFI_SPI_BITS_PER_WORD;

    ret = spi_setup(tafi_spi_device);  
    if (ret){
        printk(KERN_INFO TAFI_LOG_PREFIX"SPI device setup failed.");
        spi_unregister_device(tafi_spi_device);
        return -ENODEV;
    }

    printk(KERN_INFO TAFI_LOG_PREFIX"SPI started.");
    return 0;
}

/**
 * De-initialize SPI device.
 */
static void tafi_spi_exit(void) {
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping SPI...");
    if (tafi_spi_device) {
        spi_unregister_device(tafi_spi_device);
    } else {
        printk(KERN_INFO TAFI_LOG_PREFIX"SPI device not found.");
    }
    printk(KERN_INFO TAFI_LOG_PREFIX"stopped SPI.");
}

// Thread and timer

// The global task.
struct task_struct *tafi_task;

static int tafi_thread(void *data) {
    printk(KERN_INFO TAFI_LOG_PREFIX"thread running.");
    while (!kthread_should_stop()) {
        tafi_begin_frame();
        ssleep(1);
        tafi_end_frame();
        ssleep(1);
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
  printk(KERN_INFO TAFI_LOG_PREFIX"staring...");
  // stuff to do

  // init GPIO
  tafi_gpio_init();

  // init SPI
  tafi_spi_init();

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