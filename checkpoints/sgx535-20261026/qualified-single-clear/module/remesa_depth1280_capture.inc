/* SPDX-License-Identifier: GPL-2.0-only */
/* Immutable complete capture BEFORE ACK, within the same native syscall. */
#include <crypto/hash.h>
#include <linux/vmalloc.h>
#include "remesa_depth1280_capture.h"
#include "remesa/visible_readback_uapi.h"
#define DEPTH1280_BYTES (1280U*800U*4U)
struct depth1280_capture {
 struct drm_device *dev;struct drm_file *file;u64 context;
 struct remesa_memory_ticket ticket;u32 ids[2];u8 *data[2],digest[2][32];
 struct crypto_shash *tfm;struct shash_desc *desc;bool allocated,complete,ready;
};
static struct depth1280_capture depth_capture;
static void depth1280_capture_clear(void)
{
 kfree(depth_capture.desc);
 if(depth_capture.tfm&&!IS_ERR(depth_capture.tfm))crypto_free_shash(depth_capture.tfm);
 vfree(depth_capture.data[0]);vfree(depth_capture.data[1]);
 memset(&depth_capture,0,sizeof(depth_capture));
}
static int depth1280_capture_prepare(struct drm_device *dev,struct drm_file *file,u64 ctx,u32 color,u32 z)
{
 if(depth_capture.allocated)return -EBUSY;
 if(!dev||!file||!ctx||!color||!z||color==z)return -EINVAL;
 depth_capture.dev=dev;depth_capture.file=file;depth_capture.context=ctx;
 depth_capture.ids[0]=color;depth_capture.ids[1]=z;
 depth_capture.data[0]=vzalloc(DEPTH1280_BYTES);depth_capture.data[1]=vzalloc(DEPTH1280_BYTES);
 depth_capture.tfm=crypto_alloc_shash("sha256",0,0);
 if(!depth_capture.data[0]||!depth_capture.data[1]||IS_ERR(depth_capture.tfm))goto fail;
 depth_capture.desc=kmalloc(sizeof(*depth_capture.desc)+crypto_shash_descsize(depth_capture.tfm),GFP_KERNEL);
 if(!depth_capture.desc)goto fail;
 depth_capture.desc->tfm=depth_capture.tfm;depth_capture.allocated=true;return 0;
fail:depth1280_capture_clear();return -ENOMEM;
}
static bool depth1280_ticket_equal(struct remesa_memory_ticket a,struct remesa_memory_ticket b)
{return a.context==b.context&&a.operation==b.operation&&a.slot==b.slot&&a.generation==b.generation;}
int remesa_depth1280_capture_visible(struct remesa_gem_memory *s,struct remesa_memory_ticket t)
{
 struct retained_job *j;struct remesa_memory_binding uses[16];
 const unsigned roles[2]={SGX535_BO_COLOR,SGX535_BO_Z};unsigned i;int e=-ENOENT;
 if(!s||!depth_capture.allocated||depth_capture.complete||s->dev!=depth_capture.dev||
    s->file!=depth_capture.file||t.context!=depth_capture.context)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->quarantined){e=-EIO;goto done;}
 if(j->state!=REMESA_JOB_VISIBLE||!j->attached||j->count!=11){e=-EBUSY;goto done;}
 e=verify_job_bindings(s,j,uses);if(e){e=job_fault(s,j,e);goto done;}
 for(i=0;i<2;i++)if(roles[i]>=j->count||j->ids[roles[i]]!=depth_capture.ids[i]||
    uses[roles[i]].bytes!=DEPTH1280_BYTES||!j->cpu_views[roles[i]]){e=-EACCES;goto done;}
 /* Complete PTE validation once under the same mutex before BOTH copies.
  * Holds exclude mutation/teardown; this is not public arbitrary read access. */
 for(i=0;i<2;i++){
  struct backing *b=&s->bo[j->ids[roles[i]]&31];
  drm_clflush_pages(b->pages,b->inserted);
 }
 dma_rmb();
 for(i=0;i<2;i++)memcpy(depth_capture.data[i],j->cpu_views[roles[i]],DEPTH1280_BYTES);
 depth_capture.ticket=t;e=0;
done:mutex_unlock(&s->lock);if(e)return e;
 for(i=0;i<2;i++){
  e=crypto_shash_digest(depth_capture.desc,depth_capture.data[i],DEPTH1280_BYTES,depth_capture.digest[i]);if(e)return e;
 }
 depth_capture.complete=true;return 0;
}
int remesa_depth1280_capture_hashes(struct remesa_memory_ticket t,u8 c[32],u8 z[32])
{
 if(!c||!z||!depth_capture.complete||!depth1280_ticket_equal(t,depth_capture.ticket))return -EAGAIN;
 memcpy(c,depth_capture.digest[0],32);memcpy(z,depth_capture.digest[1],32);return 0;
}
int remesa_depth1280_snapshot_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
 struct remesa_visible_readback_request *r=data;unsigned i,index;int e=0;
 if(!r||!dev||!file||!capable(CAP_SYS_ADMIN))return -EPERM;
 if(!memory_enabled)return -EOPNOTSUPP;
 if(r->version!=1||!r->id||!r->context||!r->operation||!r->generation||r->slot>=8||
    !r->length||r->length>4096||r->reserved)return -EINVAL;
 for(i=0;i<4096;i++)if(r->data[i])return -EINVAL;
 mutex_lock(&endpoint_lock);
 if(!endpoint||endpoint->dev!=dev||endpoint->file!=file||endpoint->context!=r->context||
    depth_capture.dev!=dev||depth_capture.file!=file||depth_capture.context!=r->context){e=-EACCES;goto done;}
 if(!depth_capture.ready){e=-EAGAIN;goto done;}
 if(r->operation!=depth_capture.ticket.operation||r->slot!=depth_capture.ticket.slot||
    r->generation!=depth_capture.ticket.generation){e=-EACCES;goto done;}
 index=r->id==depth_capture.ids[0]?0:r->id==depth_capture.ids[1]?1:2;
 if(index==2){e=-ENOENT;goto done;}
 if(r->offset>DEPTH1280_BYTES||r->length>DEPTH1280_BYTES-r->offset){e=-EINVAL;goto done;}
 memcpy(r->data,depth_capture.data[index]+(size_t)r->offset,r->length);
done:mutex_unlock(&endpoint_lock);return e;
}
