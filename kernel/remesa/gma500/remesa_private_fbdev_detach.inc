/* SPDX-License-Identifier: GPL-2.0-only */
/* put_fb_info invokes fb_destroy only at the final open-file reference. The
 * target GEM tracker keeps the device/module alive through this callback. */
static void remesa_private_fb_destroy_info(struct fb_info *info)
{
 struct drm_fb_helper *helper=info->par;
 struct drm_device *dev=helper->dev;
 struct drm_psb_private *p=dev->dev_private;
 if(p&&p->fb_helper==helper)p->fb_helper=NULL;
 psb_fbdev_destroy(dev,helper);kfree(helper);
}
static void psb_fbdev_fini(struct drm_device *dev)
{
 struct drm_psb_private *p=dev->dev_private;
 struct drm_fb_helper *helper=p->fb_helper;
 if(!helper||p->remesa_private_fbdev_unregistered)return;
 p->remesa_private_fbdev_unregistered=true;
 cancel_work_sync(&helper->resume_work);cancel_work_sync(&helper->dirty_work);
 if(helper->fbdev){
  /* May synchronously destroy helper/info. Never access helper afterwards. */
  drm_fb_helper_unregister_fbi(helper);return;
 }
 p->fb_helper=NULL;drm_fb_helper_fini(helper);kfree(helper);
}
void remesa_private_fbdev_detach(struct drm_device *dev)
{if(dev&&dev->dev_private)psb_fbdev_fini(dev);}
