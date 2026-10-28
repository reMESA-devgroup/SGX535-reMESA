/* SPDX-License-Identifier: MIT */
#include "remesa_linux_memory.h"
#include "remesa/memory_uapi.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
_Static_assert(sizeof(struct remesa_memory_request)==4152,"fixed native/i386 memory ABI");
struct held {uint32_t id;size_t bytes;uint64_t gpu_va;void *cpu;bool write,poison;};
struct remesa_linux_memory {int fd;uint64_t context,copy_operation;bool owned_session;struct remesa_memory_recorder recorder;struct held objects[32];};
static struct held *find(struct remesa_linux_memory *d,uint32_t id)
{for(unsigned i=0;i<32;i++)if(d->objects[i].id==id && id)return &d->objects[i];return NULL;}
static int call(struct remesa_linux_memory *d,struct remesa_memory_request *r)
{
 struct remesa_memory_request before;int e;r->version=REMESA_MEMORY_VERSION;r->context=d->context;before=*r;
 if(d->recorder.record && d->recorder.record(d->recorder.priv,&before,NULL,0,0,true)) return EOWNERDEAD;
 e=ioctl(d->fd,DRM_IOCTL_REMESA_MEMORY,r);int saved_error=e<0?errno:0;
 if(d->recorder.record && d->recorder.record(d->recorder.priv,&before,r,e,saved_error,false)) return EOWNERDEAD;
 errno=saved_error;
 /* No retry, including EINTR. Any possible partial mutation poisons ownership. */
 if(e<0) return errno==EINVAL || errno==ENOTTY || errno==EOPNOTSUPP || errno==ENOENT || errno==ENOMEM || errno==ENOSPC || errno==EBUSY ? errno:EOWNERDEAD;
 if(e || r->version!=before.version || (r->action==REMESA_MEM_OPEN ? (!r->context || before.context) : r->context!=before.context) || r->action!=before.action ||
    r->flags!=before.flags || r->bytes!=before.bytes || r->va!=before.va || r->offset!=before.offset ||
    r->length!=before.length || r->reserved ||
    (r->action!=REMESA_MEM_CREATE && r->id!=before.id) ||
    (r->action==REMESA_MEM_CREATE && (!r->id || !(r->id>>5))) ||
    (r->action!=REMESA_MEM_READ && r->action!=REMESA_MEM_INFO && memcmp(r->data,before.data,sizeof(r->data)))) return EOWNERDEAD;
 return 0;
}
static int simple(void *priv,uint32_t id,unsigned action)
{struct remesa_linux_memory *d=priv;struct held *b=find(d,id);struct remesa_memory_request r={0};int e;if(!b)return EINVAL;
 if(b->poison)return EOWNERDEAD;
 r.action=action;r.id=id;e=call(d,&r);if(e==EOWNERDEAD)b->poison=true;return e;}
static int create(void *priv,size_t bytes,uint32_t *id)
{
 struct remesa_linux_memory *d=priv;struct held *b=NULL;struct remesa_memory_request r={0};int e;
 if(!id)return EINVAL;
 *id=0;
 /* Match the qualified Linux service, before issuing any mutation. */
 if(!bytes || bytes%4096 || bytes>128u*1024u*1024u)return EINVAL;
 for(unsigned i=0;i<32;i++)if(!d->objects[i].id && !d->objects[i].poison){b=&d->objects[i];break;}
 if(!b)return ENOSPC;
 r.action=REMESA_MEM_CREATE;r.bytes=bytes;e=call(d,&r);
 if(e){if(e==EOWNERDEAD)b->poison=true;return e;}
 if(find(d,r.id)){b->poison=true;return EOWNERDEAD;}
 b->id=r.id;b->bytes=bytes;*id=r.id;return 0;
}
static int pin(void *d,uint32_t id){return simple(d,id,REMESA_MEM_PIN);}
static int unpin(void *d,uint32_t id){return simple(d,id,REMESA_MEM_UNPIN);}
static int sync_device(void *d,uint32_t id){return simple(d,id,REMESA_MEM_SYNC);}
static int destroy(void *priv,uint32_t id)
{struct remesa_linux_memory *d=priv;struct held *b=find(d,id);int e;if(!b)return EINVAL;if(b->cpu)return EBUSY;e=simple(d,id,REMESA_MEM_DESTROY);if(!e)memset(b,0,sizeof(*b));return e;}
static int gpu_map(void *priv,uint32_t id,uint64_t va,size_t bytes)
{struct remesa_linux_memory *d=priv;struct held *b=find(d,id);struct remesa_memory_request r={0};int e;if(!b || bytes!=b->bytes)return EINVAL;
 if(b->poison)return EOWNERDEAD;
 /* Qualified SGX MMU virtual addresses are 32-bit, despite the wide UAPI. */
 if(!va || va%4096 || va>=UINT64_C(0x100000000) ||
    bytes>UINT64_C(0x100000000)-va)return EINVAL;
 if(b->gpu_va || b->cpu)return EBUSY;
 r.action=REMESA_MEM_MAP;r.id=id;r.va=va;e=call(d,&r);if(e==EOWNERDEAD)b->poison=true;
 if(!e)b->gpu_va=va;
 return e;}
static int gpu_unmap(void *d,uint32_t id,uint64_t va,size_t bytes)
{struct held *b=find(d,id);int e;if(!b || bytes!=b->bytes || !va || va!=b->gpu_va)return EINVAL;
 if(b->cpu)return EBUSY;
 e=simple(d,id,REMESA_MEM_UNMAP);if(!e)b->gpu_va=0;return e;}
static int transfer(struct remesa_linux_memory *d,struct held *b,bool write)
{
 for(size_t off=0;off<b->bytes;off+=REMESA_MEMORY_CHUNK){struct remesa_memory_request r={0};int e;
  r.action=write?REMESA_MEM_WRITE:REMESA_MEM_READ;r.id=b->id;r.offset=off;
  r.length=b->bytes-off>REMESA_MEMORY_CHUNK?REMESA_MEMORY_CHUNK:(uint32_t)(b->bytes-off);
  if(write)memcpy(r.data,(char *)b->cpu+off,r.length);
  e=call(d,&r);if(e){b->poison=true;return EOWNERDEAD;}
  if(!write)memcpy((char *)b->cpu+off,r.data,r.length);
 }return 0;
}
static int cpu_map(void *priv,uint32_t id,bool write,void **out)
{
 struct remesa_linux_memory *d=priv;struct held *b=find(d,id);struct remesa_memory_request r={0};int e;
 if(!out || !b)return EINVAL;
 *out=NULL;if(b->poison)return EOWNERDEAD;if(b->cpu)return EBUSY;
 b->cpu=malloc(b->bytes);if(!b->cpu)return ENOMEM;
 r.action=REMESA_MEM_LEASE;r.id=id;r.flags=write;e=call(d,&r);
 if(e){free(b->cpu);b->cpu=NULL;if(e==EOWNERDEAD)b->poison=true;return e;}
 b->write=write;e=transfer(d,b,false);if(e)return e;*out=b->cpu;return 0;
}
static int cpu_unmap(void *priv,uint32_t id,bool write)
{
 struct remesa_linux_memory *d=priv;struct held *b=find(d,id);int e;
 if(!b || !b->cpu || write!=b->write)return EINVAL;
 if(b->poison)return EOWNERDEAD;
 if(write){e=transfer(d,b,true);if(e)return e;}
 e=simple(d,id,REMESA_MEM_UNLEASE);if(e){b->poison=true;return EOWNERDEAD;}
 free(b->cpu);b->cpu=NULL;return 0;
}
static enum remesa_dispatch dispatch(void *d,const struct remesa_gpu_submit *s)
{(void)d;(void)s;return REMESA_NOT_ISSUED;}
static int poll_receipt(void *d,uint64_t c,uint64_t op,uint64_t ns,struct remesa_gpu_receipt *r)
{(void)d;(void)c;(void)op;(void)ns;(void)r;return EOPNOTSUPP;}
static int release(void *d,uint64_t c,uint64_t op){(void)d;(void)c;(void)op;return EOPNOTSUPP;}
static const struct remesa_persistent_ops ops={create,pin,gpu_map,gpu_unmap,unpin,destroy,cpu_map,cpu_unmap,sync_device,dispatch,poll_receipt,release};
const struct remesa_persistent_ops *remesa_linux_memory_ops(void){return &ops;}
int remesa_linux_memory_attach(int fd,uint64_t context,struct remesa_linux_memory **out)
{struct remesa_linux_memory *d;if(!out || fd<0 || !context)return EINVAL;*out=NULL;d=calloc(1,sizeof(*d));if(!d)return ENOMEM;d->fd=fd;d->context=context;*out=d;return 0;}
int remesa_linux_memory_begin_recorded(int fd,struct remesa_linux_memory **out,const struct remesa_memory_recorder *recorder)
{
 struct remesa_linux_memory *d;struct remesa_memory_request r={0};int e;
 if(!out || fd<0)return EINVAL;
 *out=NULL;d=calloc(1,sizeof(*d));if(!d)return ENOMEM;d->fd=fd;if(recorder)d->recorder=*recorder;
 r.action=REMESA_MEM_OPEN;e=call(d,&r);
 if(e){/* A corrupted/uncertain OPEN may own mappings: retain fd externally. */
  free(d);return e;
 }
 d->context=r.context;d->owned_session=true;*out=d;return 0;
}
int remesa_linux_memory_detach(struct remesa_linux_memory *d)
{if(!d)return EINVAL;for(unsigned i=0;i<32;i++)if(d->objects[i].id || d->objects[i].poison)return EBUSY;
 if(d->owned_session){struct remesa_memory_request r={0};r.action=REMESA_MEM_CLOSE;int e=call(d,&r);if(e)return e;}
 free(d);return 0;}

uint64_t remesa_linux_memory_context(const struct remesa_linux_memory *d)
{return d?d->context:0;}

int remesa_linux_memory_submit_retained(struct remesa_linux_memory *d,
 struct remesa_retained_request *r,bool *attempted,remesa_retained_recorder record,void *priv)
{
 if(!d||!r||!attempted||!record||r->context!=d->context)return EINVAL;
 if(*attempted)return EALREADY;
 int validation=remesa_retained_request_validate(r);if(validation)return validation;
 for(unsigned i=0;i<REMESA_RETAINED_ROLES;i++) {
  int error=remesa_linux_memory_validate_backing(d,r->bindings[i].id,
    r->bindings[i].va,(size_t)r->bindings[i].bytes);
  if(error)return error;
 }
 return remesa_retained_submit_once(d->fd,r,attempted,record,priv);
}

int remesa_linux_memory_info(struct remesa_linux_memory *d,uint32_t id,struct remesa_memory_info *out)
{
 struct remesa_memory_request r={0};int e;
 if(!d || !out)return EINVAL;
 r.action=REMESA_MEM_INFO;r.id=id;e=call(d,&r);if(e)return e;
 memcpy(out,r.data,sizeof(*out));
 for(size_t i=sizeof(*out);i<sizeof(r.data);i++)if(r.data[i])return EPROTO;
 if(out->objects>32 || out->pinned_objects>out->objects || out->leases>out->objects ||
    out->mapped_pages>32768 || out->verified_pages>out->object_mapped_pages ||
    out->object_mapped_pages>32768 || out->object_pinned>1 || out->object_lease>1 ||
    out->allocated_bytes>128u*1024u*1024u || out->object_bytes>out->allocated_bytes) return EPROTO;
 return 0;
}

int remesa_linux_memory_validate_backing(struct remesa_linux_memory *d,uint32_t id,
                                        uint64_t va,size_t bytes)
{
 struct held *b;struct remesa_memory_info info;int e;
 if(!d || !id || !va || va%4096 || !bytes || bytes%4096 ||
    bytes>128u*1024u*1024u || va>=UINT64_C(0x100000000) ||
    bytes>UINT64_C(0x100000000)-va)return EINVAL;
 b=find(d,id);if(!b || bytes!=b->bytes)return EINVAL;
 if(b->poison)return EOWNERDEAD;
 if(b->cpu)return EBUSY;
 e=remesa_linux_memory_info(d,id,&info);
 if(e){if(e==EOWNERDEAD || e==EPROTO)b->poison=true;return e;}
 if(info.quarantined || !info.objects || info.object_bytes!=bytes ||
    info.gpu_va!=va || info.object_pinned!=1 || info.object_lease ||
    info.object_mapped_pages!=bytes/4096 || info.verified_pages!=bytes/4096 ||
    info.pinned_objects<1 || info.mapped_pages<info.object_mapped_pages) {
  b->poison=true;return EOWNERDEAD;
 }
 return 0;
}

int remesa_linux_memory_begin(int fd,struct remesa_linux_memory **out)
{return remesa_linux_memory_begin_recorded(fd,out,NULL);}

void remesa_linux_memory_forget(struct remesa_linux_memory *d)
{if(d){for(unsigned i=0;i<32;i++)free(d->objects[i].cpu);free(d);}}

_Static_assert(sizeof(struct remesa_twod_copy_request)==120,"fixed native copy ABI");
int remesa_linux_memory_copy_2d(struct remesa_linux_memory *d,struct remesa_twod_copy_request *out)
{
 struct remesa_twod_copy_request r,before;struct held *a,*b;int e;
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_twod_copy_request)||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out)return EINVAL;
 r=*out;
 if(r.version||r.context||r.flags||r.reserved||r.response_reserved||r.operation||r.cookie||
    r.stages||r.retained||r.core||r.revision||r.error||!r.width||!r.height||
    !r.budget_ms||r.budget_ms>10000)return EINVAL;
 a=find(d,r.source.id);b=find(d,r.destination.id);
 if(!a||!b||!a->gpu_va||!b->gpu_va)return EINVAL;
 if(a->poison||b->poison)return EOWNERDEAD;
 if(a->cpu||b->cpu)return EBUSY;
 if(d->recorder.record)return EOPNOTSUPP;
 r.version=REMESA_TWOD_COPY_VERSION;r.context=d->context;before=r;
 e=ioctl(d->fd,DRM_IOCTL_REMESA_TWOD_COPY,&r);
 if(e<0){
  int error=errno;
  if(!memcmp(&r,&before,sizeof r)&&(error==EINVAL||error==ENOTTY||error==EOPNOTSUPP||error==EPERM||error==EACCES||error==EBUSY||error==ENOMEM))return error;
  goto uncertain;
 }
 if(e||memcmp(&r,&before,offsetof(struct remesa_twod_copy_request,operation))||r.response_reserved||
    !r.operation||r.operation<=d->copy_operation)goto uncertain;
 if(r.error<0&&r.error>=-4095&&r.stages==REMESA_TWOD_COPY_PREPARED&&!r.cookie&&!r.retained&&!r.core&&!r.revision){d->copy_operation=r.operation;*out=r;return -r.error;}
 if(r.error||r.retained||r.stages!=REMESA_TWOD_COPY_ALL||!r.operation||!r.cookie||
    r.core!=0x01130000u||r.revision!=0x00010201u)goto uncertain;
 d->copy_operation=r.operation;*out=r;return 0;
uncertain:
 a->poison=b->poison=true;return EOWNERDEAD;
}
