/* SPDX-License-Identifier: MIT */
/* Same provider TU, private owned state shared solely for lifecycle exclusion. */
_Static_assert(sizeof(struct remesa_general_session_request)==176,"general session ABI");
_Static_assert(offsetof(struct remesa_general_session_request,operation)==120,"general prefix");
static int general_host_valid(const struct remesa_linux_memory *d,const void *p,size_t bytes)
{
 if(!p||scene_host_overlap(d,sizeof(*d),p,bytes))return 0;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu&&
    scene_host_overlap(d->objects[i].cpu,d->objects[i].bytes,p,bytes))return 0;
 return 1;
}
static int general_output_valid(const struct remesa_linux_memory *d,const struct remesa_general_session_request *r)
{return d&&r&&!((uintptr_t)r%_Alignof(struct remesa_general_session_request))&&general_host_valid(d,r,sizeof(*r));}
static int general_exchange(struct remesa_linux_memory *d,struct remesa_general_session_request *r,
 struct remesa_general_session_request *out)
{
 struct remesa_general_session_request before=*r,check;
 int rc=ioctl(d->fd,DRM_IOCTL_REMESA_GENERAL_SESSION,r);
 *out=*r;check=*r;if(before.action==1)check.session=before.session;
 if(rc||memcmp(&check,&before,offsetof(struct remesa_general_session_request,operation))||
    r->error||r->reserved[0]||r->reserved[1])return private_lab_uncertain(d);
 if(r->action==3){
  if(r->state!=3||r->stages!=32||r->operation||r->cookie||r->core||r->revision||r->pds_base||r->phase||r->ledger)
   return private_lab_uncertain(d);
  d->private_active=false;d->general_adapter_active=false;return 0;
 }
 if(r->state!=1||r->core!=0x01130000||r->revision!=0x10201||r->pds_base!=0x20000000)
  return private_lab_uncertain(d);
 if(r->action==1){
  if(!r->session||r->stages!=16||r->operation||r->cookie||r->phase||r->ledger)return private_lab_uncertain(d);
  d->private_session=r->session;d->private_maximum=r->max_jobs;d->private_protocol=2;
  d->private_active=true;d->private_pds_id=0;return 0;
 }
 if(r->stages!=15||r->phase!=9||r->ledger!=7||!r->operation||r->operation!=r->cookie||
    r->operation<=d->private_operation)return private_lab_uncertain(d);
 d->private_operation=r->operation;d->private_sequence=r->sequence;return 0;
}
int remesa_linux_memory_general_open(struct remesa_linux_memory *d,const uint32_t ids[18],
 uint32_t profile,uint32_t maximum,struct remesa_general_session_request *out)
{
 struct remesa_general_session_request r={.version=1,.action=1,.profile=profile,.max_jobs=maximum};
 int error;
 if(!d||!ids||(uintptr_t)ids%_Alignof(uint32_t)||!general_output_valid(d,out)||
    !general_host_valid(d,ids,sizeof(r.ids))||scene_host_overlap(ids,sizeof(r.ids),out,sizeof(*out)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_attempted||d->scene_attempted)return EALREADY;
 if(d->recorder.record)return EOPNOTSUPP;
 r.context=d->context;memcpy(r.ids,ids,sizeof(r.ids));
 if(!remesa_general_session_request_valid(&r))return EINVAL;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 for(unsigned i=0;i<18;i++){
  struct held *b=find(d,ids[i]);if(!b||!b->gpu_va)return EINVAL;
  if(b->poison)return EOWNERDEAD;
 }
 for(unsigned i=0;i<18;i++){
  struct held *b=find(d,ids[i]);error=remesa_linux_memory_validate_backing(d,b->id,b->gpu_va,b->bytes);if(error)return error;
 }
 d->private_attempted=true;d->scene_attempted=true;
 return general_exchange(d,&r,out);
}
static int general_next(struct remesa_linux_memory *d,uint32_t budget,
 struct remesa_general_session_request *out,bool close)
{
 struct remesa_general_session_request r={.version=1,.action=close?3u:2u};
 if(!d||!general_output_valid(d,out)||(!close&&(!budget||budget>10000)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(!d->private_active)return ENOENT;
 if(d->private_protocol!=2)return EOPNOTSUPP;
 if(!close&&d->private_sequence>=d->private_maximum)return ENOSPC;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 r.context=d->context;r.session=d->private_session;
 r.sequence=d->private_sequence+(close?0:1);r.budget_ms=budget;
 return general_exchange(d,&r,out);
}
int remesa_linux_memory_general_submit(struct remesa_linux_memory *d,uint32_t budget,
 struct remesa_general_session_request *out)
{return general_next(d,budget,out,false);}
int remesa_linux_memory_general_close(struct remesa_linux_memory *d,struct remesa_general_session_request *out)
{return general_next(d,0,out,true);}

int remesa_linux_memory_general_scene_open(struct remesa_linux_memory *d,
 const struct remesa_scene_packet *packet,uint32_t profile,uint32_t maximum,uint32_t budget,
 struct remesa_general_session_request *out)
{
 if(!d||!packet||(uintptr_t)packet%_Alignof(struct remesa_scene_packet)||
    !general_output_valid(d,out)||!general_host_valid(d,packet,sizeof(*packet))||
    scene_host_overlap(packet,sizeof(*packet),out,sizeof(*out))||!budget||budget>10000)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(!remesa_scene_packet_valid(packet)||packet->owner!=d->context||
    packet->binding_count!=18||packet->use_count!=11||packet->window_count!=2||
    profile<3||profile>6||packet->controls.width!=((profile&1)?1280u:640u)||
    packet->controls.height!=((profile&1)?800u:640u)||packet->controls.schema!=2||
    packet->controls.depth_finalize!=1||packet->controls.ta_bytes!=104)return EINVAL;
 uint32_t ids[18];for(unsigned i=0;i<18;i++)ids[i]=packet->bindings[i].id;
 int error=remesa_linux_memory_general_open(d,ids,profile,maximum,out);
 if(error)return error;
 d->general_packet=*packet;d->general_budget=budget;d->general_host_operation=0;
 d->general_adapter_active=true;return 0;
}

enum remesa_dispatch remesa_linux_memory_general_scene_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{
 struct remesa_linux_memory *d=opaque;
 if(!d||!submit||!out||(uintptr_t)submit%_Alignof(struct remesa_gpu_scene_submit)||
    (uintptr_t)out%_Alignof(struct remesa_gpu_scene_receipt)||
    !general_host_valid(d,submit,sizeof(*submit))||!general_host_valid(d,out,sizeof(*out))||
    scene_host_overlap(submit,sizeof(*submit),out,sizeof(*out)))return REMESA_NOT_ISSUED;
 if(!submit->packet||(uintptr_t)submit->packet%_Alignof(struct remesa_scene_packet)||
    !general_host_valid(d,submit->packet,sizeof(*submit->packet))||
    scene_host_overlap(out,sizeof(*out),submit->packet,sizeof(*submit->packet)))return REMESA_NOT_ISSUED;
 if(d->private_poison)return REMESA_ISSUANCE_AMBIGUOUS;
 if(!d->general_adapter_active||!d->private_active||d->private_protocol!=2||
    submit->context!=d->context||!submit->operation||submit->operation<=d->general_host_operation||
    memcmp(submit->packet,&d->general_packet,sizeof(d->general_packet)))return REMESA_NOT_ISSUED;
 struct remesa_general_session_request response;
 int error=remesa_linux_memory_general_submit(d,d->general_budget,&response);
 if(error)return d->private_poison?REMESA_ISSUANCE_AMBIGUOUS:REMESA_NOT_ISSUED;
 d->general_host_operation=submit->operation;
 *out=(struct remesa_gpu_scene_receipt){.context=submit->context,.operation=submit->operation,
  .retired=true,.published=true,.acknowledged=true};
 return REMESA_ISSUED;
}
