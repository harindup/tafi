/*
 *  tafi_fb.c -- The Amazing Fan Idea driver
 *  Framebuffer device implementation
 *  Based on the default Linux virtual framebuffer device at
 *  drivers/video/vfb.c
 * 
 * 		Copyright (C) 2017 R A Harindu Perera
 * 
 * 	Original vfb.c:
 *
 *      Copyright (C) 2002 James Simmons
 *
 *	    Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include "tafi_ioctl.h"
#include "tafi_fb.h"
    /*
     *  RAM we reserve for the frame buffer. This defines the maximum screen
     *  size
     *
     *  The default can be overridden if the driver is compiled as a module
     */

#define VIDEOMEMSIZE	(1024*1024)	/* 1 MB */

static void *videomemory;
static u_long videomemorysize = VIDEOMEMSIZE;

static struct platform_device *tafi_fb_device;

static const struct fb_videomode tafi_fb_default = {
	.xres =		40,
	.yres =		40,
	.left_margin =	0,
	.right_margin =	0,
	.upper_margin =	0,
	.lower_margin =	0,
	.vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_var_screeninfo tafi_fb_var = {
	.xres =		40,
	.yres =		40,
	.xres_virtual = 40,
	.yres_virtual = 40,
	.bits_per_pixel = 24,
	.nonstd = 1,
	.red =  {
		.offset = 8,
		.length = 8,
		.msb_right = 0,
	},
	.green = {
		.offset = 1,
		.length = 8,
		.msb_right = 0,
	},
	.blue = {
		.offset = 16,
		.length = 8,
		.msb_right = 0,
	},
	.transp = {
		.offset = 0,
		.length = 0,
		.msb_right = 0,
	},
};

static struct fb_fix_screeninfo tafi_fb_fix = {
	.id =		"TAFIFBDEVICE",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep = 0,
	.accel =	FB_ACCEL_NONE,
};

static int tafi_fb_check_var(struct fb_var_screeninfo *var,
			 struct fb_info *info);
static int tafi_fb_set_par(struct fb_info *info);
static int tafi_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			 u_int transp, struct fb_info *info);
static int tafi_fb_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *info);
static int tafi_fb_mmap(struct fb_info *info,
		    struct vm_area_struct *vma);

static struct fb_ops tafi_fb_ops = {
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
//	.fb_check_var	= tafi_fb_check_var,
	.fb_set_par	= tafi_fb_set_par,
	.fb_setcolreg	= tafi_fb_setcolreg,
	.fb_pan_display	= tafi_fb_pan_display,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
	.fb_mmap	= tafi_fb_mmap,
};

/*
 *  Internal routines
 */

static u_long get_line_length(int xres_virtual, int bpp)
{
	u_long length;

	length = xres_virtual * bpp;
	length = (length + 31) & ~31;
	length >>= 3;
	return (length);
}

    /*
     *  Setting the video mode has been split into two parts.
     *  First part, xxxfb_check_var, must not write anything
     *  to hardware, it should only verify and adjust var.
     *  This means it doesn't alter par but it does use hardware
     *  data from it to check this var. 
     */

static int tafi_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info) {
	u_long line_length;

	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	/*
	 *  Some very basic checks
	 */
	if (var->xres != 40)
		var->xres = 40;
	if (var->yres != 40)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	
	if (var->bits_per_pixel != 24) 
		return -EINVAL;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	/*
	 *  Memory limit
	 */
	line_length =
	    get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (line_length * var->yres_virtual > videomemorysize)
		return -ENOMEM;

	// RGB 888
	var->red.offset = 8;
	var->red.length = 8;
	var->green.offset = 0;
	var->green.length = 8;
	var->blue.offset = 16;
	var->blue.length = 8;
	// No transparency
	var->transp.offset = 0;
	var->transp.length = 0;

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the 
 * change in par. For this driver it doesn't do much. 
 */
static int tafi_fb_set_par(struct fb_info *info)
{
	info->fix.line_length = get_line_length(info->var.xres_virtual,
						info->var.bits_per_pixel);
	return 0;
}

/*
 *  Set a single color register. The values supplied are already
 *  rounded down to the hardware's capabilities (according to the
 *  entries in the var structure). Return != 0 for invalid regno.
*/
static int tafi_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue, u_int transp, struct fb_info *info) {
	
	if (regno >= 16)	/* no. of hw registers */
		return 1;

	if (info->fix.visual != FB_VISUAL_TRUECOLOR)
		return 1;

	u32 v;

	red = (red >> 1) & 0x80;
	green = (green >> 1) & 0x80;
	blue = (blue >> 1) & 0x80;

	v = (red << info->var.red.offset) |
		(green << info->var.green.offset) |
		(blue << info->var.blue.offset) |
		(transp << info->var.transp.offset);

	((u32 *) (info->pseudo_palette))[regno] = v;

	return 0;
}

/*
 *  Pan or Wrap the Display
 *
 *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 */
static int tafi_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info) {
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset >= info->var.yres_virtual ||
		    var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + info->var.xres > info->var.xres_virtual ||
		    var->yoffset + info->var.yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

/*
 *  Most drivers don't need their own mmap function 
 */
static int tafi_fb_mmap(struct fb_info *info, struct vm_area_struct *vma) {
	return remap_vmalloc_range(vma, (void *)info->fix.smem_start, vma->vm_pgoff);
}

/*
 *  Initialisation
 */

static int tafi_fb_probe(struct platform_device *dev) {
	struct fb_info *info;
	unsigned int size = PAGE_ALIGN(videomemorysize);
	int retval = -ENOMEM;

	/*
	 * For real video cards we use ioremap.
	 */
	if (!(videomemory = vmalloc_32_user(size)))
		return retval;

	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info)
		goto err;

	info->screen_base = (char __iomem *)videomemory;
	info->fbops = &tafi_fb_ops;
	info->mode = &tafi_fb_default;

	info->var = tafi_fb_var;

	tafi_fb_fix.smem_start = (unsigned long) videomemory;
	tafi_fb_fix.smem_len = videomemorysize;
	info->fix = tafi_fb_fix;
	info->pseudo_palette = info->par;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;

	retval = fb_alloc_cmap(&info->cmap, 256, 0);
	if (retval < 0)
		goto err1;

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err2;
	platform_set_drvdata(dev, info);

	fb_info(info, "Desperate Housewife frame buffer device, using %ldK of video memory\n",
		videomemorysize >> 10);
	return 0;
err2:
	fb_dealloc_cmap(&info->cmap);
err1:
	framebuffer_release(info);
err:
	vfree(videomemory);
	return retval;
}

static int tafi_fb_remove(struct platform_device *dev) {
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		unregister_framebuffer(info);
		vfree(videomemory);
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver tafi_fb_driver = {
	.probe	= tafi_fb_probe,
	.remove = tafi_fb_remove,
	.driver = {
		.name	= "tafi_fb",
	},
};

int tafi_fb_init(void) {
	int ret = 0;

	ret = platform_driver_register(&tafi_fb_driver);

	if (!ret) {
		tafi_fb_device = platform_device_alloc("tafi_fb", 0);

		if (tafi_fb_device)
			ret = platform_device_add(tafi_fb_device);
		else
			ret = -ENOMEM;

		if (ret) {
			platform_device_put(tafi_fb_device);
			platform_driver_unregister(&tafi_fb_driver);
		}
	}

	return ret;
}

void tafi_fb_exit(void) {
	platform_device_unregister(tafi_fb_device);
	platform_driver_unregister(&tafi_fb_driver);
}