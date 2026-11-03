/* SPDX-License-Identifier: MIT */
/* Same provider TU, private owned state shared solely for lifecycle exclusion. */
_Static_assert(sizeof(struct remesa_general_session_request)==176,"general session ABI");
_Static_assert(offsetof(struct remesa_general_session_request,operation)==120,"general prefix");
_Static_assert(sizeof(struct remesa_general_parameter_request)==592,"parameter update ABI");
_Static_assert(offsetof(struct remesa_general_parameter_request,accepted_sequence)==568,"parameter update prefix");
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
 if(d->general_record){d->general_request=before;d->general_response=*r;d->general_recorded=true;}
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
  d->private_active=true;d->private_pds_id=0;d->general_version=r->version;return 0;
 }
 if(r->stages!=15||r->phase!=9||r->ledger!=7||!r->operation||r->operation!=r->cookie||
    r->operation<=d->private_operation)return private_lab_uncertain(d);
 d->private_operation=r->operation;d->private_sequence=r->sequence;return 0;
}
static int general_open_version(struct remesa_linux_memory *d,const uint32_t ids[18],
 uint32_t profile,uint32_t maximum,uint32_t version,struct remesa_general_session_request *out)
{
 struct remesa_general_session_request r={.version=version,.action=1,.profile=profile,.max_jobs=maximum};
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
int remesa_linux_memory_general_open(struct remesa_linux_memory *d,const uint32_t ids[18],
 uint32_t profile,uint32_t maximum,struct remesa_general_session_request *out)
{return general_open_version(d,ids,profile,maximum,1,out);}
int remesa_linux_memory_general_typed_open(struct remesa_linux_memory *d,const uint32_t ids[18],
 uint32_t profile,uint32_t maximum,struct remesa_general_session_request *out)
{return general_open_version(d,ids,profile,maximum,2,out);}
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
 r.version=d->general_version;
 r.sequence=d->private_sequence+(close?0:1);r.budget_ms=budget;
 return general_exchange(d,&r,out);
}
int remesa_linux_memory_general_submit(struct remesa_linux_memory *d,uint32_t budget,
 struct remesa_general_session_request *out)
{return general_next(d,budget,out,false);}
int remesa_linux_memory_general_close(struct remesa_linux_memory *d,struct remesa_general_session_request *out)
{return general_next(d,0,out,true);}
int remesa_linux_memory_general_record_enable(struct remesa_linux_memory *d)
{
 if(!d)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_attempted||d->scene_attempted||d->general_record)return EALREADY;
 d->general_record=true;return 0;
}
int remesa_linux_memory_general_response(const struct remesa_linux_memory *d,
 struct remesa_general_session_request *out)
{
 if(!general_output_valid(d,out))return EINVAL;
 if(!d->general_recorded)return ENODATA;
 *out=d->general_response;return 0;
}
int remesa_linux_memory_general_request(const struct remesa_linux_memory *d,
 struct remesa_general_session_request *out)
{
 if(!general_output_valid(d,out))return EINVAL;
 if(!d->general_recorded)return ENODATA;
 *out=d->general_request;return 0;
}

static int general_scene_open_version(struct remesa_linux_memory *d,
 const struct remesa_scene_packet *packet,uint32_t profile,uint32_t maximum,uint32_t budget,
 uint32_t version,struct remesa_general_session_request *out)
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
 int error=general_open_version(d,ids,profile,maximum,version,out);
 if(error)return error;
 d->general_packet=*packet;d->general_budget=budget;d->general_host_operation=0;
 d->general_adapter_active=true;return 0;
}
int remesa_linux_memory_general_scene_open(struct remesa_linux_memory *d,
 const struct remesa_scene_packet *p,uint32_t profile,uint32_t maximum,uint32_t budget,
 struct remesa_general_session_request *out)
{return general_scene_open_version(d,p,profile,maximum,budget,1,out);}
int remesa_linux_memory_general_scene_typed_open(struct remesa_linux_memory *d,
 const struct remesa_scene_packet *p,uint32_t profile,uint32_t maximum,uint32_t budget,
 struct remesa_general_session_request *out)
{return general_scene_open_version(d,p,profile,maximum,budget,2,out);}

static int general_parameter_output_valid(const struct remesa_linux_memory *d,
 const struct remesa_general_parameter_request *out)
{return d&&out&&!((uintptr_t)out%_Alignof(struct remesa_general_parameter_request))&&general_host_valid(d,out,sizeof(*out));}
int remesa_linux_memory_general_parameters(struct remesa_linux_memory *d,uint32_t draw,
 uint32_t offset,const uint32_t *bits,uint32_t words,struct remesa_general_parameter_request *out)
{
 struct remesa_general_parameter_request r={.version=1,.draw=draw,.offset=offset,.words=words},before;
 size_t bytes=(size_t)words*4;int rc;
 if(!general_parameter_output_valid(d,out)||!bits||(uintptr_t)bits%_Alignof(uint32_t)||
    !words||words>127||!general_host_valid(d,bits,bytes)||scene_host_overlap(bits,bytes,out,sizeof(*out)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(!d->private_active)return ENOENT;
 if(d->private_protocol!=2||d->general_version!=2)return EOPNOTSUPP;
 if(!d->private_operation||!d->private_sequence)return EAGAIN;
 if(d->general_update_sequence==UINT64_MAX)return EOVERFLOW;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 r.context=d->context;r.session=d->private_session;r.retired_operation=d->private_operation;
 r.sequence=d->general_update_sequence+1;memcpy(r.data,bits,bytes);
 if(!remesa_general_parameter_request_valid(&r))return EINVAL;
 before=r;rc=ioctl(d->fd,DRM_IOCTL_REMESA_GENERAL_PARAMETER,&r);
 if(d->general_record){d->general_parameter_request=before;d->general_parameter_response=r;d->general_parameter_recorded=true;}
 *out=r;
 if(rc||memcmp(&r,&before,offsetof(struct remesa_general_parameter_request,accepted_sequence))||
    r.reserved[0]||r.reserved[1]||r.state!=1)return private_lab_uncertain(d);
 if(r.error){
  if(r.accepted_sequence)return private_lab_uncertain(d);
  /* A stale epoch means host/native ownership diverged, even when this
   * particular request was refused without a write. Never guess a new epoch. */
  if(r.error==-EINVAL||r.error==-EACCES||r.error==-ERANGE||
     r.error==-EBUSY||r.error==-ENODATA||r.error==-ENOMEM||r.error==-EOPNOTSUPP)return -r.error;
  return private_lab_uncertain(d);
 }
 if(r.accepted_sequence!=before.sequence)return private_lab_uncertain(d);
 d->general_update_sequence=r.accepted_sequence;return 0;
}
int remesa_linux_memory_general_parameter_request(const struct remesa_linux_memory *d,
 struct remesa_general_parameter_request *out)
{
 if(!general_parameter_output_valid(d,out))return EINVAL;
 if(!d->general_parameter_recorded)return ENODATA;
 *out=d->general_parameter_request;return 0;
}
int remesa_linux_memory_general_parameter_response(const struct remesa_linux_memory *d,
 struct remesa_general_parameter_request *out)
{
 if(!general_parameter_output_valid(d,out))return EINVAL;
 if(!d->general_parameter_recorded)return ENODATA;
 *out=d->general_parameter_response;return 0;
}

int remesa_linux_memory_general_scene_parameters(void *opaque,
 const struct remesa_gpu_scene_parameters *update,struct remesa_gpu_scene_parameter_receipt *out)
{
 struct remesa_linux_memory *d=opaque;
 if(!d||!update||!out||(uintptr_t)update%_Alignof(struct remesa_gpu_scene_parameters)||
    (uintptr_t)out%_Alignof(struct remesa_gpu_scene_parameter_receipt)||
    !general_host_valid(d,update,sizeof(*update))||!general_host_valid(d,out,sizeof(*out))||
    scene_host_overlap(update,sizeof(*update),out,sizeof(*out)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(!d->general_adapter_active||!d->private_active||d->general_version!=2)return EOPNOTSUPP;
 const struct remesa_parameter_window *w=&update->window;
 if(update->context!=d->context||update->serial!=d->general_packet.serial||
    !update->draw||update->draw>=d->general_packet.window_count||
    !remesa_parameter_window_valid(w)||w->owner!=d->context||
    update->words!=w->coefficient_dwords||!update->bits||
    (uintptr_t)update->bits%_Alignof(uint32_t)||
    !general_host_valid(d,update->bits,(size_t)update->words*4)||
    scene_host_overlap(out,sizeof(*out),update->bits,(size_t)update->words*4))return EINVAL;
 if(!d->general_host_operation)return EAGAIN;
 if(update->published_operation!=d->general_host_operation||
    d->general_update_sequence==UINT64_MAX||update->sequence!=d->general_update_sequence+1)
  return private_lab_uncertain(d);
 struct held *b=find(d,w->id);unsigned read=0,use=0;
 if(!b||b->poison||b->gpu_va!=w->gpu_va||b->bytes!=w->allocation_bytes)return EINVAL;
 for(unsigned i=0;i<d->general_packet.binding_count;i++)
  if(d->general_packet.bindings[i].id==w->id&&d->general_packet.bindings[i].access==REMESA_ACCESS_READ)read++;
 for(unsigned i=0;i<d->general_packet.use_count;i++){
  const struct remesa_scene_packet_use *u=&d->general_packet.uses[i];
  if(u->id==w->id&&u->byte_offset==w->use_offset&&u->program_bytes==8&&u->data_master==REMESA_USE_VERTEX)use++;
 }
 if(read!=1||use!=1)return EINVAL;
 struct remesa_general_parameter_request response;
 int e=remesa_linux_memory_general_parameters(d,update->draw,w->source_offset+4,
  update->bits,update->words,&response);
 if(e)return e;
 *out=(struct remesa_gpu_scene_parameter_receipt){.context=update->context,.serial=update->serial,
  .published_operation=update->published_operation,.sequence=response.accepted_sequence};
 return 0;
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
