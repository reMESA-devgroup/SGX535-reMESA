/* SPDX-License-Identifier: GPL-2.0-only */
/* Covers the fb helper/SysRq legacy modeset route independently of a DRM fd. */
int gma_crtc_set_config(struct drm_mode_set *set,struct drm_modeset_acquire_ctx *ctx)
{
 struct drm_device *dev;int index,result;
 if(!set||!set->crtc||!set->crtc->dev)return -EINVAL;
 dev=set->crtc->dev;
 if(!drm_dev_enter(dev,&index))return -ENODEV;
 result=remesa_private_fb_config_live(set,ctx);
 drm_dev_exit(index);return result;
}
int gma_crtc_cursor_set(struct drm_crtc *crtc,struct drm_file *file_priv,
                       uint32_t handle,uint32_t width,uint32_t height)
{
 int index,result;
 if(!crtc||!crtc->dev)return -EINVAL;
 if(!drm_dev_enter(crtc->dev,&index))return -ENODEV;
 result=remesa_private_fb_cursor_set_live(crtc,file_priv,handle,width,height);
 drm_dev_exit(index);return result;
}
int gma_crtc_cursor_move(struct drm_crtc *crtc,int x,int y)
{
 int index,result;
 if(!crtc||!crtc->dev)return -EINVAL;
 if(!drm_dev_enter(crtc->dev,&index))return -ENODEV;
 result=remesa_private_fb_cursor_move_live(crtc,x,y);
 drm_dev_exit(index);return result;
}
