/**
 *  tafi_chardev.h -- The Amazing Fan Idea driver
 *  Character device definitions.
 * 
 *      (C) 2017 Harindu Perera
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef TAFI_CHARDEV
#define TAFI_CHARDEV

#include <linux/device.h>
// Header for the Linux file system support
#include <linux/fs.h>
// Required for the copy to user function
#include <asm/uaccess.h>

// For kmalloc
#include <linux/slab.h>

// Character device mutex
#include <linux/mutex.h>

#include "tafi_common.h"
#include "tafi_ioctl.h"

#define  DEVICE_NAME "tafi"    
#define  CLASS_NAME  "tafi"

int tafi_chardev_init(void);

void tafi_chardev_exit(void);

#endif