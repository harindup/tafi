/**
 *  tafi_ioctl.h -- The Amazing Fan Idea driver
 *  Device communication methods.
 * 
 *      (C) 2017 Harindu Perera
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/types.h>

#ifndef TAFI_IOCTL
#define TAFI_IOCTL

#define TAFI_SECTOR_COUNT 150
#define TAFI_SECTOR_LED_COUNT 20
#define TAFI_LED_COLOR_FIELD_COUNT 3
#define TAFI_SECTOR_BUF_LEN TAFI_SECTOR_LED_COUNT * TAFI_LED_COLOR_FIELD_COUNT
#define TAFI_DATA_BUF_LEN TAFI_SECTOR_COUNT * TAFI_SECTOR_BUF_LEN

// static int tafi_write_frame(void * buf);

// static int tafi_read_frame(void *buf);
#endif