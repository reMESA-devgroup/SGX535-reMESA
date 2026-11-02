/* SPDX-License-Identifier: MIT */
#include "remesa_linux_memory.h"
#include "remesa/memory_uapi.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
_Static_assert(sizeof(struct remesa_memory_request)==4152,"fixed native/i386 memory ABI");
struct held {uint32_t id;size_t bytes;uint64_t gpu_va;void *cpu;bool write,poison;};
struct remesa_linux_memory {int fd;uint64_t context,copy_operation;bool owned_session;struct remesa_memory_recorder recorder;struct held objects[32];
 uint64_t private_session,private_sequence,private_operation;
 uint32_t private_maximum,private_pds_id;bool private_attempted,private_active,private_poison,scene_attempted;
 struct remesa_scene_probe_request scene_response;
 unsigned scene_protocol;uint32_t gears_access[18];
 struct remesa_gears_scene_probe_request gears_response;};
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
{if(!d)return EINVAL;if(d->private_poison)return EOWNERDEAD;if(d->private_active)return EBUSY;
 for(unsigned i=0;i<32;i++)if(d->objects[i].id || d->objects[i].poison)return EBUSY;
 if(d->owned_session){struct remesa_memory_request r={0};r.action=REMESA_MEM_CLOSE;int e=call(d,&r);if(e)return e;}
 free(d);return 0;}

uint64_t remesa_linux_memory_context(const struct remesa_linux_memory *d)
{return d?d->context:0;}

int remesa_linux_memory_inventory_snapshot(const struct remesa_linux_memory *d,
 struct remesa_linux_memory_inventory *out)
{
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_linux_memory_inventory)||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out)return EINVAL;
 uintptr_t at=(uintptr_t)out,owner=(uintptr_t)d;
 if(sizeof(*d)>UINTPTR_MAX-owner||
    (at<owner+sizeof(*d)&&owner<at+sizeof(*out)))return EINVAL;
 struct remesa_linux_memory_inventory snapshot={.context=d->context,.version=1};
 for(unsigned i=0;i<32;i++){
  const struct held *b=&d->objects[i];
  if(b->cpu){
   uintptr_t base=(uintptr_t)b->cpu;
   if(b->bytes>UINTPTR_MAX-base||(at<base+b->bytes&&base<at+sizeof(*out)))return EINVAL;
  }
  if(!b->id&&!b->poison)continue;
  unsigned n=snapshot.count++;
  snapshot.entries[n].id=b->id;snapshot.entries[n].gpu_va=b->gpu_va;
  snapshot.entries[n].bytes=b->bytes;
  snapshot.entries[n].flags=(b->id?REMESA_MEMORY_INVENTORY_LIVE:0)|
   (b->cpu?REMESA_MEMORY_INVENTORY_LEASE:0)|
   (b->cpu&&b->write?REMESA_MEMORY_INVENTORY_WRITE:0)|
   (b->poison?REMESA_MEMORY_INVENTORY_POISON:0);
 }
 *out=snapshot;return 0;
}

int remesa_linux_memory_submit_retained(struct remesa_linux_memory *d,
 struct remesa_retained_request *r,bool *attempted,remesa_retained_recorder record,void *priv)
{
 if(!d||!r||!attempted||!record||r->context!=d->context)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active)return EBUSY;
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

_Static_assert(sizeof(struct remesa_private_session_lab_request)==136,"fixed private session ABI");
_Static_assert(offsetof(struct remesa_private_session_lab_request,operation)==88,"private request prefix");
static int private_lab_uncertain(struct remesa_linux_memory *d)
{
 d->private_poison=true;
 /* Even unused slots are poisoned: no new allocation after uncertain OPEN,
  * SUBMIT or CLOSE. Only explicit INFO and host-only forget remain useful. */
 for(unsigned i=0;i<32;i++)d->objects[i].poison=true;
 return EOWNERDEAD;
}
_Static_assert(sizeof(struct remesa_scene_probe_request)==144,"fixed scene LAB ABI");
_Static_assert(offsetof(struct remesa_scene_probe_request,operation)==88,"scene request prefix");
static bool scene_host_overlap(const void *a,size_t an,const void *b,size_t bn)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return true;
 return x<=y?y-x<an:x-y<bn;
}
int remesa_linux_memory_scene_lab_submit(struct remesa_linux_memory *d,
 const struct remesa_scene_packet *packet,uint32_t profile,uint32_t budget,
 struct remesa_scene_probe_request *out)
{
 struct remesa_scene_probe_request r={0},before;
 if(!d||!packet||!out||(uintptr_t)packet%_Alignof(struct remesa_scene_packet)||
    (uintptr_t)out%_Alignof(struct remesa_scene_probe_request)||
    scene_host_overlap(packet,sizeof(*packet),out,sizeof(*out))||
    scene_host_overlap(d,sizeof(*d),out,sizeof(*out))||
    scene_host_overlap(d,sizeof(*d),packet,sizeof(*packet)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active)return EBUSY;
 if(d->scene_attempted)return EALREADY;
 if(d->recorder.record)return EOPNOTSUPP;
 if(!profile||profile>REMESA_SCENE_PROBE_PROFILE_COUNT||!budget||budget>10000||
    !remesa_scene_packet_valid(packet)||packet->owner!=d->context||
    packet->binding_count!=17||packet->use_count!=10||packet->window_count!=2)return EINVAL;
 const unsigned width=(profile&1)?1280:640,height=(profile&1)?800:640;
 if(packet->controls.width!=width||packet->controls.height!=height||
    packet->controls.schema!=(profile>=5?2u:1u)||
    packet->controls.depth_finalize!=(profile>=5?1u:0u)||
    packet->controls.ta_bytes!=(profile>=3?96u:68u))return EINVAL;
 for(unsigned i=0;i<17;i++){
  struct held *b=find(d,packet->bindings[i].id);
  if(!b||!b->gpu_va)return EINVAL;
  if(b->poison)return EOWNERDEAD;
  if(b->cpu)return EBUSY;
  r.ids[i]=b->id;
 }
 r.version=1;r.profile=profile;r.context=d->context;r.budget_ms=budget;before=r;
 d->scene_attempted=true;d->scene_protocol=1;
 int e=ioctl(d->fd,DRM_IOCTL_REMESA_SCENE_PROBE,&r);
 d->scene_response=r;
 *out=r; /* Preserve raw evidence, including unsuccessful/malformed responses. */
 if(e||memcmp(&r,&before,offsetof(struct remesa_scene_probe_request,operation))||
    r.reserved[0]||r.reserved[1]||!r.operation||r.cookie!=r.operation||
    r.core!=0x01130000u||r.revision!=0x00010201u||r.pds_base!=0x20000000u||
    r.phase!=9||r.ledger!=7||r.error||r.retained||r.stages!=15)
  return private_lab_uncertain(d);
 return 0;
}
static enum remesa_dispatch scene_lab_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out,unsigned profile_offset)
{
 struct remesa_linux_memory *d=opaque;
 if(!d||!submit||!out||(uintptr_t)submit%_Alignof(struct remesa_gpu_scene_submit)||
    (uintptr_t)out%_Alignof(struct remesa_gpu_scene_receipt)||
    !submit->packet||!submit->operation||submit->context!=d->context||
    scene_host_overlap(out,sizeof(*out),d,sizeof(*d))||
    scene_host_overlap(out,sizeof(*out),submit,sizeof(*submit))||
    scene_host_overlap(out,sizeof(*out),submit->packet,sizeof(*submit->packet)))return REMESA_NOT_ISSUED;
 if(d->private_poison)return REMESA_ISSUANCE_AMBIGUOUS;
 if(d->scene_attempted)return REMESA_NOT_ISSUED;
 unsigned profile=(submit->packet->controls.width==1280?5:6)+profile_offset;
 struct remesa_scene_probe_request response;
 int error=remesa_linux_memory_scene_lab_submit(d,submit->packet,profile,1000,&response);
 if(error)return d->private_poison?REMESA_ISSUANCE_AMBIGUOUS:REMESA_NOT_ISSUED;
 const struct remesa_gpu_scene_receipt r={.context=submit->context,.operation=submit->operation,
  .retired=true,.published=true,.acknowledged=true};
 *out=r;return REMESA_ISSUED;
}
enum remesa_dispatch remesa_linux_memory_scene_lab_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{return scene_lab_dispatch(opaque,submit,out,0);}
enum remesa_dispatch remesa_linux_memory_gallium_scene_lab_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{return scene_lab_dispatch(opaque,submit,out,2);}
enum remesa_dispatch remesa_linux_memory_gallium_matrix_scene_lab_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{return scene_lab_dispatch(opaque,submit,out,4);}
int remesa_linux_memory_scene_lab_response(const struct remesa_linux_memory *d,
 struct remesa_scene_probe_request *out)
{
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_scene_probe_request)||
    scene_host_overlap(out,sizeof(*out),d,sizeof(*d)))return EINVAL;
 if(!d->scene_attempted||d->scene_protocol!=1)return ENODATA;
 *out=d->scene_response;return 0;
}
_Static_assert(sizeof(struct remesa_gears_scene_probe_request)==152,"separate18-binding LAB ABI");
_Static_assert(offsetof(struct remesa_gears_scene_probe_request,operation)==96,"explicit request padding");
_Static_assert(sizeof(struct remesa_gears_scene_trace)==4280,"complete18-binding trace");
_Static_assert(sizeof(struct remesa_gears_scene_trace_request)==4296,"complete trace query ABI");
int remesa_linux_memory_gears_scene_lab_submit(struct remesa_linux_memory *d,
 const struct remesa_scene_packet *packet,uint32_t profile,uint32_t budget,
 struct remesa_gears_scene_probe_request *out)
{
 struct remesa_gears_scene_probe_request r={0},before;
 if(!d||!packet||!out||(uintptr_t)packet%_Alignof(struct remesa_scene_packet)||
    (uintptr_t)out%_Alignof(struct remesa_gears_scene_probe_request)||
    scene_host_overlap(packet,sizeof(*packet),out,sizeof(*out))||
    scene_host_overlap(d,sizeof(*d),out,sizeof(*out))||
    scene_host_overlap(d,sizeof(*d),packet,sizeof(*packet)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active)return EBUSY;
 if(d->scene_attempted)return EALREADY;
 if(d->recorder.record)return EOPNOTSUPP;
 if(!profile||profile>REMESA_GEARS_SCENE_PROFILE_COUNT||!budget||budget>10000||
    !remesa_scene_packet_valid(packet)||packet->owner!=d->context||
    packet->binding_count!=18||packet->use_count!=11||packet->window_count!=2)return EINVAL;
 const unsigned width=(profile&1)?1280:640,height=(profile&1)?800:640;
 if(packet->controls.width!=width||packet->controls.height!=height||
    packet->controls.schema!=2||packet->controls.depth_finalize!=1||packet->controls.ta_bytes!=104)return EINVAL;
 for(unsigned i=0;i<18;i++){
  struct held *b=find(d,packet->bindings[i].id);
  if(!b||!b->gpu_va)return EINVAL;
  if(b->poison)return EOWNERDEAD;
  if(b->cpu)return EBUSY;
  r.ids[i]=b->id;
 }
 r.version=2;r.profile=profile;r.context=d->context;r.budget_ms=budget;before=r;
 for(unsigned i=0;i<18;i++)d->gears_access[i]=packet->bindings[i].access;
 d->scene_attempted=true;d->scene_protocol=2;
 int e=ioctl(d->fd,DRM_IOCTL_REMESA_GEARS_SCENE_PROBE,&r);
 d->gears_response=r;*out=r; /* Preserve raw evidence after every entered ioctl. */
 if(e||memcmp(&r,&before,offsetof(struct remesa_gears_scene_probe_request,operation))||
    r.reserved[0]||r.reserved[1]||!r.operation||r.cookie!=r.operation||
    r.core!=0x01130000u||r.revision!=0x00010201u||r.pds_base!=0x20000000u||
    r.phase!=9||r.ledger!=7||r.error||r.retained||r.stages!=15)return private_lab_uncertain(d);
 return 0;
}
static enum remesa_dispatch gears_scene_lab_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out,unsigned profile_offset)
{
 struct remesa_linux_memory *d=opaque;
 if(!d||!submit||!out||(uintptr_t)submit%_Alignof(struct remesa_gpu_scene_submit)||
    (uintptr_t)out%_Alignof(struct remesa_gpu_scene_receipt)||!submit->packet||!submit->operation||submit->context!=d->context||
    scene_host_overlap(out,sizeof(*out),d,sizeof(*d))||scene_host_overlap(out,sizeof(*out),submit,sizeof(*submit))||
    scene_host_overlap(out,sizeof(*out),submit->packet,sizeof(*submit->packet)))return REMESA_NOT_ISSUED;
 if(d->private_poison)return REMESA_ISSUANCE_AMBIGUOUS;
 if(d->scene_attempted)return REMESA_NOT_ISSUED;
 struct remesa_gears_scene_probe_request response;
 unsigned profile=(submit->packet->controls.width==1280?1:2)+profile_offset;
 int error=remesa_linux_memory_gears_scene_lab_submit(d,submit->packet,profile,1000,&response);
 if(error)return d->private_poison?REMESA_ISSUANCE_AMBIGUOUS:REMESA_NOT_ISSUED;
 *out=(struct remesa_gpu_scene_receipt){.context=submit->context,.operation=submit->operation,
  .retired=true,.published=true,.acknowledged=true};return REMESA_ISSUED;
}
enum remesa_dispatch remesa_linux_memory_gears_scene_lab_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{return gears_scene_lab_dispatch(opaque,submit,out,0);}
enum remesa_dispatch remesa_linux_memory_gears_homogeneous_lab_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{return gears_scene_lab_dispatch(opaque,submit,out,2);}
int remesa_linux_memory_gears_scene_lab_response(const struct remesa_linux_memory *d,
 struct remesa_gears_scene_probe_request *out)
{
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_gears_scene_probe_request)||
    scene_host_overlap(out,sizeof(*out),d,sizeof(*d)))return EINVAL;
 if(!d->scene_attempted||d->scene_protocol!=2)return ENODATA;
 *out=d->gears_response;return 0;
}
int remesa_linux_memory_gears_scene_lab_trace(struct remesa_linux_memory *d,
 struct remesa_gears_scene_trace *out)
{
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_gears_scene_trace)||
    scene_host_overlap(out,sizeof(*out),d,sizeof(*d)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(!d->scene_attempted||d->scene_protocol!=2)return ENODATA;
 struct remesa_gears_scene_trace_request q={.version=2,.context=d->context};
 int error=ioctl(d->fd,DRM_IOCTL_REMESA_GEARS_SCENE_TRACE,&q),saved=errno;
 *out=q.trace; /* A diagnostic query cannot undo proven execution retirement. */
 if(error)return error<0?saved:EIO;
 const struct remesa_scene_probe_trace *t=&q.trace.prefix;
 const struct remesa_gears_scene_probe_request *r=&d->gears_response;
 if(q.version!=2||q.reserved||q.context!=d->context||t->version!=2||t->bytes!=sizeof(q.trace)||
    t->context!=d->context||t->operation!=r->operation||t->cookie!=r->cookie||t->profile!=r->profile||
    t->stages!=15||t->phase!=9||t->ledger!=7||t->error||t->retained||t->reserved||
    t->binding_count!=18||t->event_count>64||t->load_count!=29||t->ta_count!=31||t->raster_count!=20)return EPROTO;
 for(unsigned i=0;i<18;i++){
  const struct remesa_scene_trace_binding *b=i<17?&t->bindings[i]:&q.trace.last_binding;
  struct held *h=find(d,r->ids[i]);
  if(!h||b->id!=h->id||b->address!=h->gpu_va||b->bytes!=h->bytes||b->access!=d->gears_access[i])return EPROTO;
 }
 return 0;
}
static int private_lab_output_valid(const struct remesa_private_session_lab_request *out)
{
 return out && !((uintptr_t)out%_Alignof(struct remesa_private_session_lab_request)) &&
        sizeof(*out)<=UINTPTR_MAX-(uintptr_t)out;
}
static int private_lab_exchange(struct remesa_linux_memory *d,
 struct remesa_private_session_lab_request *r,struct remesa_private_session_lab_request *out)
{
 struct remesa_private_session_lab_request before=*r,check;
 /* An errno cannot establish whether ownership was acquired or work issued.
  * Never retry, including EINTR and supposedly pre-admission errors. */
 int rc=ioctl(d->fd,DRM_IOCTL_REMESA_PRIVATE_SESSION_LAB,r);
 *out=*r;check=*r;
 if(before.action==REMESA_PRIVATE_SESSION_LAB_OPEN)check.session=before.session;
 if(rc || memcmp(&check,&before,offsetof(struct remesa_private_session_lab_request,operation)) ||
    r->reserved || r->error)return private_lab_uncertain(d);
 if(r->action==REMESA_PRIVATE_SESSION_LAB_CLOSE){
  if(r->state!=REMESA_PRIVATE_SESSION_LAB_CLOSED || r->stages!=REMESA_PRIVATE_SESSION_LAB_CLOSED_STAGE ||
     r->operation || r->cookie || r->core || r->revision || r->phase || r->ledger)
   return private_lab_uncertain(d);
  d->private_active=false;return 0;
 }
 if(r->state!=REMESA_PRIVATE_SESSION_LAB_ACTIVE || r->core!=0x01130000u || r->revision!=0x00010201u)
  return private_lab_uncertain(d);
 if(r->action==REMESA_PRIVATE_SESSION_LAB_OPEN){
  if(!r->session || r->stages!=REMESA_PRIVATE_SESSION_LAB_OPENED ||
     r->operation || r->cookie || r->phase || r->ledger)return private_lab_uncertain(d);
  d->private_session=r->session;d->private_maximum=r->max_jobs;d->private_pds_id=r->ids[0];d->private_active=true;
 }else{
  if(r->stages!=15 || r->phase!=9 || r->ledger!=7 || !r->operation ||
     r->operation!=r->cookie || r->operation<=d->private_operation)return private_lab_uncertain(d);
  d->private_sequence=r->sequence;d->private_operation=r->operation;
 }
 return 0;
}
int remesa_linux_memory_private_lab_open(struct remesa_linux_memory *d,
 const uint32_t ids[11],uint32_t maximum,struct remesa_private_session_lab_request *out)
{
 struct remesa_private_session_lab_request r={0};int e;
 if(!d || !ids || !private_lab_output_valid(out) || !maximum || maximum>64)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_attempted)return EALREADY;
 if(d->recorder.record)return EOPNOTSUPP;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 r.version=REMESA_PRIVATE_SESSION_LAB_VERSION;r.action=REMESA_PRIVATE_SESSION_LAB_OPEN;
 r.context=d->context;r.max_jobs=maximum;memcpy(r.ids,ids,sizeof(r.ids));
 /* Validate the full local set before the first provider query. Canonical
  * contents, exact profile bindings, permissions and sealing belong to kernel
  * admission; local INFO checks cannot authorize execution. */
 for(unsigned i=0;i<11;i++){
  struct held *b=find(d,r.ids[i]);
  if(!b || !b->gpu_va)return EINVAL;
  if(b->poison)return EOWNERDEAD;
  if(b->cpu)return EBUSY;
  for(unsigned j=0;j<i;j++)if(r.ids[i]==r.ids[j])return EINVAL;
 }
 for(unsigned i=0;i<11;i++){
  struct held *b=find(d,r.ids[i]);
  e=remesa_linux_memory_validate_backing(d,b->id,b->gpu_va,b->bytes);if(e)return e;
 }
 d->private_attempted=true;
 return private_lab_exchange(d,&r,out);
}
int remesa_linux_memory_private_lab_submit(struct remesa_linux_memory *d,
 uint32_t budget_ms,struct remesa_private_session_lab_request *out)
{
 struct remesa_private_session_lab_request r={0};
 if(!d || !private_lab_output_valid(out) || !budget_ms || budget_ms>10000)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(!d->private_active)return ENOENT;
 if(d->private_sequence>=d->private_maximum)return ENOSPC;
 /* Outstanding host leases must be released before execution. No per-frame
  * provider queries or full input comparisons are needed for sealed backing. */
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 r.version=REMESA_PRIVATE_SESSION_LAB_VERSION;r.action=REMESA_PRIVATE_SESSION_LAB_SUBMIT;
 r.context=d->context;r.session=d->private_session;r.sequence=d->private_sequence+1;r.budget_ms=budget_ms;
 return private_lab_exchange(d,&r,out);
}
int remesa_linux_memory_private_lab_close(struct remesa_linux_memory *d,
 struct remesa_private_session_lab_request *out)
{
 struct remesa_private_session_lab_request r={0};
 if(!d || !private_lab_output_valid(out))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(!d->private_active)return ENOENT;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 r.version=REMESA_PRIVATE_SESSION_LAB_VERSION;r.action=REMESA_PRIVATE_SESSION_LAB_CLOSE;
 r.context=d->context;r.session=d->private_session;r.sequence=d->private_sequence;
 return private_lab_exchange(d,&r,out);
}

int remesa_linux_memory_upload_pages(struct remesa_linux_memory *d,uint32_t id,
 uint64_t offset,const void *data,size_t bytes)
{
 struct held *b;struct remesa_memory_request r={0};int e;
 if(!d || !data || !bytes || offset%4096 || bytes%4096)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 b=find(d,id);if(!b || offset>b->bytes || bytes>b->bytes-offset)return EINVAL;
 if(b->poison)return EOWNERDEAD;
 if(b->cpu)return EBUSY;
 if(d->private_active && id!=d->private_pds_id)return EPERM;
 if(bytes>UINTPTR_MAX-(uintptr_t)data)return EINVAL;
 r.action=REMESA_MEM_LEASE;r.id=id;r.flags=1;e=call(d,&r);
 if(e)return e==EOWNERDEAD?private_lab_uncertain(d):e;
 for(size_t at=0;at<bytes;at+=REMESA_MEMORY_CHUNK){
  memset(&r,0,sizeof(r));r.action=REMESA_MEM_WRITE;r.id=id;
  r.offset=offset+at;r.length=REMESA_MEMORY_CHUNK;
  memcpy(r.data,(const unsigned char *)data+at,REMESA_MEMORY_CHUNK);
  e=call(d,&r);if(e)return private_lab_uncertain(d);
 }
 memset(&r,0,sizeof(r));r.action=REMESA_MEM_UNLEASE;r.id=id;e=call(d,&r);
 return e?private_lab_uncertain(d):0;
}

_Static_assert(sizeof(struct remesa_twod_copy_request)==120,"fixed native copy ABI");
int remesa_linux_memory_copy_2d(struct remesa_linux_memory *d,struct remesa_twod_copy_request *out)
{
 struct remesa_twod_copy_request r,before;struct held *a,*b;int e;
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_twod_copy_request)||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active)return EBUSY;
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

int remesa_linux_memory_native_copy(void *opaque,const struct remesa_gpu_copy *c)
{
 if(!c)return EINVAL;
 struct remesa_twod_copy_request r={.source={c->source.id,c->source.width,c->source.height,c->source.stride},
  .destination={c->destination.id,c->destination.width,c->destination.height,c->destination.stride},
  .source_x=c->source_x,.source_y=c->source_y,.destination_x=c->destination_x,
  .destination_y=c->destination_y,.width=c->width,.height=c->height,.budget_ms=1000};
 return remesa_linux_memory_copy_2d(opaque,&r);
}

_Static_assert(sizeof(struct remesa_twod_fill_request)==104,"fixed native fill ABI");
int remesa_linux_memory_fill_2d(struct remesa_linux_memory *d,struct remesa_twod_fill_request *out)
{
 struct remesa_twod_fill_request r,before;struct held *b;int e;
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_twod_fill_request)||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active)return EBUSY;
 r=*out;
 if(r.version||r.context||r.flags||r.reserved[0]||r.reserved[1]||r.response_reserved||r.operation||r.cookie||
    r.stages||r.retained||r.core||r.revision||r.error||!r.width||!r.height||
    !r.budget_ms||r.budget_ms>10000)return EINVAL;
 b=find(d,r.destination.id);
 if(!b||!b->gpu_va)return EINVAL;
 if(b->poison)return EOWNERDEAD;
 if(b->cpu)return EBUSY;
 if(d->recorder.record)return EOPNOTSUPP;
 r.version=REMESA_TWOD_FILL_VERSION;r.context=d->context;before=r;
 e=ioctl(d->fd,DRM_IOCTL_REMESA_TWOD_FILL,&r);
 if(e<0){
  int error=errno;
  if(!memcmp(&r,&before,sizeof r)&&(error==EINVAL||error==ENOTTY||error==EOPNOTSUPP||error==EPERM||error==EACCES||error==EBUSY||error==ENOMEM))return error;
  goto uncertain;
 }
 if(e||memcmp(&r,&before,offsetof(struct remesa_twod_fill_request,operation))||r.response_reserved||
    !r.operation||r.operation<=d->copy_operation)goto uncertain;
 if(r.error<0&&r.error>=-4095&&r.stages==REMESA_TWOD_FILL_PREPARED&&!r.cookie&&!r.retained&&!r.core&&!r.revision){d->copy_operation=r.operation;*out=r;return -r.error;}
 if(r.error||r.retained||r.stages!=REMESA_TWOD_FILL_ALL||!r.operation||!r.cookie||
    r.core!=0x01130000u||r.revision!=0x00010201u)goto uncertain;
 d->copy_operation=r.operation;*out=r;return 0;
uncertain:
 b->poison=true;return EOWNERDEAD;
}

int remesa_linux_memory_native_fill(void *opaque,const struct remesa_gpu_fill *f)
{
 if(!f)return EINVAL;
 struct remesa_twod_fill_request r={.destination={f->destination.id,f->destination.width,f->destination.height,f->destination.stride},
  .destination_x=f->destination_x,.destination_y=f->destination_y,.width=f->width,.height=f->height,.color=f->color,.budget_ms=1000};
 return remesa_linux_memory_fill_2d(opaque,&r);
}
