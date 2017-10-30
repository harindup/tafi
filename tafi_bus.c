/**
 *  tafi_bus.c -- The Amazing Fan Idea driver
 *  Hardware bus implementation (SPI/GPIO).
 * 
 *      (C) 2017 Harindu Perera
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

// Linux header info
#include <linux/device.h>

// GPIO header
#include <linux/gpio.h>

// SPI header
#include <linux/spi/spi.h>

#include "tafi_common.h"
#include "tafi_bus.h"

// GPIO

/**
 * Initialize GPIO pins for use.
 */
void tafi_gpio_init(void) {
  
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
void tafi_gpio_exit(void) {
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping GPIO...");
    gpio_free(TAFI_GPIO_FRAME_START_PIN);
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping GPIO.");
}

/**
 * Set the frame pin to high, and signal the start of a frame.
 */
inline void tafi_frame_begin(void) {
    gpio_set_value(TAFI_GPIO_FRAME_START_PIN, 1);
}

/**
 * Set the frame pin to low, and signal the end of a frame.
 */
inline void tafi_frame_end(void) {
    gpio_set_value(TAFI_GPIO_FRAME_START_PIN, 0);
} 


// SPI

// The global SPI device
static struct spi_device *tafi_spi_device;

/**
 * Initializes SPI device.
 * TODO: fix the hijacking voodoo mess, and ensure removal
 * of the default spidev beforehand.
 */
int tafi_spi_init(void) {
    
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

    ret = spi_add_device(tafi_spi_device);
    if (ret < 0) {
        printk(KERN_INFO TAFI_LOG_PREFIX"SPI device setup failed.");
        spi_dev_put(tafi_spi_device);
        put_device(&master->dev);
        return -ENODEV;
    }

    printk(KERN_INFO TAFI_LOG_PREFIX"SPI started.");
    return 0;
}

/**
 * De-initialize SPI device.
 */
void tafi_spi_exit(void) {
    printk(KERN_INFO TAFI_LOG_PREFIX"stopping SPI...");
    if (tafi_spi_device) {
        spi_dev_put(tafi_spi_device);
    } else {
        printk(KERN_INFO TAFI_LOG_PREFIX"SPI device not found.");
    }
    printk(KERN_INFO TAFI_LOG_PREFIX"stopped SPI.");
}

/**
 * Write data to device.
 * Note that it is useless to call this unless a frame begin has been
 * via the GPIO command.
 */
inline int tafi_data_write(const void *buf, size_t len) {
    return spi_write(tafi_spi_device, buf, len);
}