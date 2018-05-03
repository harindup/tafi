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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/page.h>

#include "tafi_ioctl.h"
#include "tafi_fb.h"
#include "tafi_fb_lut.h"
#include "tafi_common.h"
    /*
     *  RAM we reserve for the frame buffer. This defines the maximum screen
     *  size
     *
     *  The default can be overridden if the driver is compiled as a module
     */

#define VIDEOMEMSIZE	(20*1024)	/* 12 KB */

static void *videomemory;
static u_long videomemorysize = VIDEOMEMSIZE;

static unsigned char shadow_mem[VIDEOMEMSIZE];

static unsigned char square_buf[TAFI_FB_XRES][TAFI_FB_YRES][3];

static struct platform_device *tafi_fb_device;

static const struct fb_videomode tafi_fb_default = {
	.xres =		TAFI_FB_XRES,
	.yres =		TAFI_FB_YRES,
	.left_margin =	0,
	.right_margin =	0,
	.upper_margin =	0,
	.lower_margin =	0,
	.vmode =	FB_VMODE_NONINTERLACED,
};

static struct fb_var_screeninfo tafi_fb_var = {
	.xres =		TAFI_FB_XRES,
	.yres =		TAFI_FB_YRES,
	.xres_virtual = TAFI_FB_XRES,
	.yres_virtual = TAFI_FB_YRES,
	.bits_per_pixel = TAFI_FB_BPP,
	.nonstd = 1,
	.red =  {
		.offset = 0,
		.length = 8,
		.msb_right = 0,
	},
	.green = {
		.offset = 8,
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

static int tafi_fb_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos);
static int tafi_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
static int tafi_fb_set_par(struct fb_info *info);
static int tafi_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue, u_int transp, struct fb_info *info);
static int tafi_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
static int tafi_fb_mmap(struct fb_info *info, struct vm_area_struct *vma);

static void tafi_fb_copy_to_device(void);

static struct fb_ops tafi_fb_ops = {
	.fb_read        = fb_sys_read,
	.fb_write       = tafi_fb_write,
//	.fb_check_var	= tafi_fb_check_var,
	.fb_set_par	= tafi_fb_set_par,
	.fb_setcolreg	= tafi_fb_setcolreg,
	.fb_pan_display	= tafi_fb_pan_display,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
	.fb_mmap	= tafi_fb_mmap,
};

static void tafi_fb_deferred_io(struct fb_info *info, struct list_head *pagelist);

static struct fb_deferred_io tafi_fb_defio = {
	.delay		= HZ,
	.deferred_io	= tafi_fb_deferred_io,
};

/*
 *  Internal routines
 */

static u_long get_line_length(int xres_virtual, int bpp) {
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

// static int tafi_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info) {
// 	u_long line_length;

// 	/*
// 	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
// 	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
// 	 */

// 	if (var->vmode & FB_VMODE_CONUPDATE) {
// 		var->vmode |= FB_VMODE_YWRAP;
// 		var->xoffset = info->var.xoffset;
// 		var->yoffset = info->var.yoffset;
// 	}

// 	/*
// 	 *  Some very basic checks
// 	 */
// 	if (var->xres != 80)
// 		var->xres = 80;
// 	if (var->yres != 80)
// 		var->yres = 80;
// 	if (var->xres > var->xres_virtual)
// 		var->xres_virtual = var->xres;
// 	if (var->yres > var->yres_virtual)
// 		var->yres_virtual = var->yres;
	
// 	if (var->bits_per_pixel != 24) 
// 		return -EINVAL;

// 	if (var->xres_virtual < var->xoffset + var->xres)
// 		var->xres_virtual = var->xoffset + var->xres;
// 	if (var->yres_virtual < var->yoffset + var->yres)
// 		var->yres_virtual = var->yoffset + var->yres;

// 	/*
// 	 *  Memory limit
// 	 */
// 	line_length =
// 	    get_line_length(var->xres_virtual, var->bits_per_pixel);
// 	if (line_length * var->yres_virtual > videomemorysize)
// 		return -ENOMEM;

// 	// RGB 888
// 	var->red.offset = 8;
// 	var->red.length = 8;
// 	var->green.offset = 0;
// 	var->green.length = 8;
// 	var->blue.offset = 16;
// 	var->blue.length = 8;
// 	// No transparency
// 	var->transp.offset = 0;
// 	var->transp.length = 0;

// 	var->red.msb_right = 0;
// 	var->green.msb_right = 0;
// 	var->blue.msb_right = 0;
// 	var->transp.msb_right = 0;

// 	return 0;
// }

/* This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the 
 * change in par. For this driver it doesn't do much. 
 */
static int tafi_fb_set_par(struct fb_info *info) {
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

	red = (red >> 1);
	green = (green >> 1);
	blue = (blue >> 1);

	v = (red << info->var.red.offset) |
		(green << info->var.green.offset) |
		(blue << info->var.blue.offset) |
		(transp << info->var.transp.offset);

	((u32 *) (info->pseudo_palette))[regno] = v;

	return 0;
}

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

static void tafi_fb_copy_to_device(void) {
	void *data;
	unsigned int p;
	unsigned int x;
	unsigned int y;
	unsigned int s;
	unsigned int l;
	unsigned int i;

	printk(KERN_INFO TAFI_LOG_PREFIX"copy data.");

	data = kmalloc(TAFI_DATA_BUF_LEN, GFP_KERNEL);
	if (data == NULL) {
		printk(KERN_INFO TAFI_LOG_PREFIX"cannot reserve memory for FB ioctl");
		return;
	}

	memcpy(square_buf, shadow_mem, TAFI_FB_XRES * TAFI_FB_YRES * 3);

	for (s = 0; s < TAFI_SECTOR_COUNT; s++) {
		for (l = 0; l < TAFI_SECTOR_LED_COUNT; l++) {
			x = TAFI_FB_DEV_LUT[s][l][0];
			y = TAFI_FB_DEV_LUT[s][l][1];
			for (i = 0; i < TAFI_LED_COLOR_FIELD_COUNT; i++) {
				p = s * TAFI_SECTOR_LED_COUNT * TAFI_LED_COLOR_FIELD_COUNT + l * TAFI_LED_COLOR_FIELD_COUNT + i;
				*((unsigned char *)(data+p)) = (TAFI_FB_LED_BRIGHTNESS_LUT[square_buf[x][y][(i+2)%3]][l] >> 1) | 0x80;
			}
		}
	}

	printk(KERN_INFO TAFI_LOG_PREFIX"copy data 2.");
	tafi_set_color_data(data, TAFI_DATA_BUF_LEN, (loff_t *) 0);
	printk(KERN_INFO TAFI_LOG_PREFIX"copy data 3.");
	kfree(data);
}

static void tafi_fb_copy_to_shadow(const unsigned char *vfb_mem, uint32_t byte_offset, uint32_t byte_width) {
	unsigned char *addr0 = (unsigned char *)(vfb_mem + byte_offset);
	unsigned char *addr1 = (unsigned char *)(shadow_mem + byte_offset);
	int j = byte_width;
	uint32_t k; 
	while (j--) {
		*(addr1 + j) = *(addr0 + j);
	}
}

static void tafi_fb_deferred_io(struct fb_info *info, struct list_head *pagelist) {
	printk(KERN_INFO TAFI_LOG_PREFIX"defio triggered");
	struct page *cur;
	struct fb_deferred_io *fbdefio = info->fbdefio;
	// list_for_each_entry(cur, &fbdefio->pagelist, lru) {
	// 	tafi_fb_copy_to_shadow((unsigned char *) info->fix.smem_start, cur->index << PAGE_SHIFT, PAGE_SIZE);
	// }
	tafi_fb_copy_to_shadow((unsigned char *) info->fix.smem_start, 0, TAFI_FB_XRES*TAFI_FB_YRES*3);
	tafi_fb_copy_to_device();
}

static ssize_t tafi_fb_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos) {
	ssize_t ret;
	ret = fb_sys_write(info, buf, count, ppos);
	tafi_fb_copy_to_shadow((unsigned char *) info->fix.smem_start, 0, TAFI_FB_XRES*TAFI_FB_YRES*3);
	tafi_fb_copy_to_device();
	return ret;
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

	info->fbdefio = &tafi_fb_defio;
	fb_deferred_io_init(info);

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
	fb_deferred_io_cleanup(info);
err1:
	framebuffer_release(info);
err:
	vfree(videomemory);
	return retval;
}

static int tafi_fb_remove(struct platform_device *dev) {
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		fb_deferred_io_cleanup(info);
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