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

#ifndef TAFI_FB
#define TAFI_FB

int tafi_fb_init(void);
void tafi_fb_exit(void);

#endif