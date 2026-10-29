/* SPDX-License-Identifier: GPL-2.0-only */
/* Included in private psb_drv.c after the original unlocked ioctl.
 * drm_dev_unplug joins these CPU entries before unload starts. GPU quarantine,
 * late GEM destruction, other KMS and PM need their own lifetime protocol.
 * No endpoint/MMU mutex may be held by remove across the SRCU join. */
#include "remesa_private_file_lost.h"
static void remesa_private_postclose(struct drm_device *dev,struct drm_file *file)
{
 int index;
 if(!drm_dev_enter(dev,&index)){
  remesa_private_stream_file_lost(dev,file);return;
 }
 remesa_stream_postclose(dev,file);
 drm_dev_exit(index);
}
static void remesa_private_lastclose(struct drm_device *dev)
{
 int index;
 if(!drm_dev_enter(dev,&index))return;
 drm_fb_helper_lastclose(dev);
 drm_dev_exit(index);
}
static bool remesa_private_file_enter(struct file *filp,int *index)
{
 struct drm_file *file=filp?filp->private_data:NULL;
 return file&&file->minor&&file->minor->dev&&drm_dev_enter(file->minor->dev,index);
}
static long remesa_private_unlocked_ioctl(struct file *filp,unsigned int cmd,
 unsigned long arg)
{
 int index;long result;
 if(!remesa_private_file_enter(filp,&index))return -ENODEV;
 result=psb_unlocked_ioctl(filp,cmd,arg);
 drm_dev_exit(index);return result;
}
#ifdef CONFIG_COMPAT
static long remesa_private_compat_ioctl(struct file *filp,unsigned int cmd,
 unsigned long arg)
{
 int index;long result;
 if(!remesa_private_file_enter(filp,&index))return -ENODEV;
 result=drm_compat_ioctl(filp,cmd,arg);
 drm_dev_exit(index);return result;
}
#else
#define remesa_private_compat_ioctl NULL
#endif
static int remesa_private_mmap(struct file *filp,struct vm_area_struct *vma)
{
 int index,result;
 if(!remesa_private_file_enter(filp,&index))return -ENODEV;
 result=remesa_gem_color_mmap(filp,vma);
 drm_dev_exit(index);return result;
}
static vm_fault_t remesa_private_gem_fault(struct vm_fault *vmf)
{
 struct drm_gem_object *object=vmf->vma->vm_private_data;
 int index;vm_fault_t result;
 if(!object||!object->dev||!drm_dev_enter(object->dev,&index))return VM_FAULT_SIGBUS;
 result=psb_gem_fault(vmf);
 drm_dev_exit(index);return result;
}
