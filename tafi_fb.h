/*
 *  tafi_fb.h -- The Amazing Fan Idea driver
 *  Framebuffer device definitions
 * 
 * 		Copyright (C) 2017 R A Harindu Perera
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/fb.h>
// Required for the copy from user function
#include <asm/uaccess.h>

#ifndef TAFI_FB
#define TAFI_FB

#define TAFI_FB_XRES 80
#define TAFI_FB_YRES 80

// bits per pixel
#define TAFI_FB_BPP 24

int tafi_fb_init(void);
void tafi_fb_exit(void);

#endif