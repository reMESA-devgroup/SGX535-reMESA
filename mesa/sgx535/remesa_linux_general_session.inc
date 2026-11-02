/* SPDX-License-Identifier: MIT */
/* Same provider TU, private owned state shared solely for lifecycle exclusion. */
_Static_assert(sizeof(struct remesa_general_session_request)==176,"general session ABI");
_Static_assert(offsetof(struct remesa_general_session_request,operation)==120,"general prefix");
static int general_output_valid(const struct remesa_general_session_request *r)
{return r&&!((uintptr_t)r%_Alignof(struct remesa_general_session_request))&&sizeof(*r)<=UINTPTR_MAX-(uintptr_t)r;}
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
  d->private_active=false;return 0;
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
 if(!d||!ids||!general_output_valid(out))return EINVAL;
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
 if(!d||!general_output_valid(out)||(!close&&(!budget||budget>10000)))return EINVAL;
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
