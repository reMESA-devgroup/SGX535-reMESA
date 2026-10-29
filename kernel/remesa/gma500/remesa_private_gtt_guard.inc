/* SPDX-License-Identifier: GPL-2.0-only */
/* Included after the original GTT operations, renamed _live. The caller owns
 * the range/GEM reference; this interval protects driver state and MMIO, not
 * arbitrary caller pointers or the outer KMS callback. */
int psb_gtt_pin(struct gtt_range *range)
{
 struct drm_device *dev;int index,result;
 if(!range||!range->gem.dev)return -EINVAL;
 dev=range->gem.dev;
 if(!drm_dev_enter(dev,&index))return -ENODEV;
 result=remesa_private_gtt_pin_live(range);
 drm_dev_exit(index);return result;
}
void psb_gtt_unpin(struct gtt_range *range)
{
 struct drm_device *dev;int index;
 if(!range||!range->gem.dev)return;
 dev=range->gem.dev;
 if(!drm_dev_enter(dev,&index))return;
 remesa_private_gtt_unpin_live(range);
 drm_dev_exit(index);
}
void psb_gtt_roll(struct drm_device *dev,struct gtt_range *range,int roll)
{
 int index;
 if(!dev||!range||range->gem.dev!=dev)return;
 if(!drm_dev_enter(dev,&index))return;
 /* Native roll rejects the upper bound but accepts negative indices. */
 if(roll>=0&&roll<range->npage)
  remesa_private_gtt_roll_live(dev,range,roll);
 drm_dev_exit(index);
}
