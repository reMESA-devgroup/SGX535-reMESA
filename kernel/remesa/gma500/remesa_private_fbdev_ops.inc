/* SPDX-License-Identifier: GPL-2.0-only */
/* fb_info/helper survive unregister until the final core put_fb_info. All
 * callbacks that reach helper locks or display state must join CPU unplug. */
static void remesa_private_fb_destroy_info(struct fb_info *info);
#define REMESA_FB_INT(name,parameters,arguments,live) \
static int name parameters {struct drm_device *dev;int index,result; \
 if(!remesa_private_fb_enter(info,&dev,&index))return -ENODEV; \
 result=live arguments;drm_dev_exit(index);return result;}
#define REMESA_FB_VOID(name,type,live) \
static void name(struct fb_info *info,const struct type *data) {struct drm_device *dev;int index; \
 if(!remesa_private_fb_enter(info,&dev,&index))return; \
 live(info,data);drm_dev_exit(index);}
REMESA_FB_INT(remesa_fb_check_var,(struct fb_var_screeninfo *var,struct fb_info *info),(var,info),drm_fb_helper_check_var)
REMESA_FB_INT(remesa_fb_set_par,(struct fb_info *info),(info),drm_fb_helper_set_par)
REMESA_FB_INT(remesa_fb_setcmap,(struct fb_cmap *map,struct fb_info *info),(map,info),drm_fb_helper_setcmap)
REMESA_FB_INT(remesa_fb_blank,(int blank,struct fb_info *info),(blank,info),drm_fb_helper_blank)
REMESA_FB_INT(remesa_fb_pan,(struct fb_var_screeninfo *var,struct fb_info *info),(var,info),drm_fb_helper_pan_display)
REMESA_FB_INT(remesa_fb_roll,(struct fb_var_screeninfo *var,struct fb_info *info),(var,info),psbfb_pan)
REMESA_FB_INT(remesa_fb_debug_enter,(struct fb_info *info),(info),drm_fb_helper_debug_enter)
REMESA_FB_INT(remesa_fb_debug_leave,(struct fb_info *info),(info),drm_fb_helper_debug_leave)
REMESA_FB_INT(remesa_fb_ioctl,(struct fb_info *info,unsigned int cmd,unsigned long arg),(info,cmd,arg),drm_fb_helper_ioctl)
REMESA_FB_INT(remesa_fb_color,(unsigned reg,unsigned red,unsigned green,unsigned blue,unsigned transp,struct fb_info *info),(reg,red,green,blue,transp,info),psbfb_setcolreg)
REMESA_FB_INT(remesa_fb_sync,(struct fb_info *info),(info),psbfb_sync)
REMESA_FB_VOID(remesa_fb_fill,fb_fillrect,drm_fb_helper_cfb_fillrect)
REMESA_FB_VOID(remesa_fb_copy,fb_copyarea,drm_fb_helper_cfb_copyarea)
REMESA_FB_VOID(remesa_fb_native_copy,fb_copyarea,psbfb_copyarea)
REMESA_FB_VOID(remesa_fb_image,fb_image,drm_fb_helper_cfb_imageblit)
#undef REMESA_FB_INT
#undef REMESA_FB_VOID
#define REMESA_PRIVATE_FB_DEFAULT_OPS \
 .fb_check_var=remesa_fb_check_var,.fb_set_par=remesa_fb_set_par, \
 .fb_setcmap=remesa_fb_setcmap,.fb_blank=remesa_fb_blank,.fb_pan_display=remesa_fb_pan, \
 .fb_debug_enter=remesa_fb_debug_enter,.fb_debug_leave=remesa_fb_debug_leave,.fb_ioctl=remesa_fb_ioctl
