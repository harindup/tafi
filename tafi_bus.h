/**
 *  tafi_bus.h -- The Amazing Fan Idea driver
 *  Hardware bus implementation declarations (SPI/GPIO).
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

#ifndef TAFI_BUS
#define TAFI_BUS

// GPIO pin for sending the frame start/end signal
#define TAFI_GPIO_FRAME_START_PIN 17

extern void tafi_gpio_init(void);

extern void tafi_gpio_exit(void);

extern inline void tafi_frame_begin(void);

extern inline void tafi_frame_end(void);

// SPI settings
#define TAFI_SPI_BUS_NUM 0
#define TAFI_SPI_BUS_SPEED_HZ 10000000 // 10 MHz
#define TAFI_SPI_CHIP_SELECT 0
#define TAFI_SPI_MODE SPI_MODE_0
#define TAFI_SPI_BITS_PER_WORD 8

extern int tafi_spi_init(void);

extern void tafi_spi_exit(void);

extern int tafi_data_write(const void *buf, size_t len);

#endif