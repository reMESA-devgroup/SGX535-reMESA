/* SPDX-License-Identifier: MIT */
_Static_assert(sizeof(struct remesa_matrix_submit_request)==2344,"matrix submit ABI");
_Static_assert(offsetof(struct remesa_matrix_submit_request,operation)==2288,"matrix submit input prefix");
int remesa_linux_memory_matrix_continuous(struct remesa_linux_memory *d,bool enable)
{
 if(!d)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active||d->private_attempted||d->scene_attempted)return EBUSY;
 if(d->recorder.record)return EOPNOTSUPP;
 d->matrix_continuous=enable;return 0;
}
int remesa_linux_memory_matrix_close(struct remesa_linux_memory *d)
{
 struct remesa_matrix_submit_request r={.version=1,.flags=REMESA_MATRIX_CLOSE_SESSION},before;
 if(!d)return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active)return EBUSY;
 if(!d->matrix_continuous||!d->matrix_session_open||d->scene_protocol!=3)return ENOENT;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 r.context=d->context;before=r;
 int rc=ioctl(d->fd,DRM_IOCTL_REMESA_MATRIX_SUBMIT,&r);
 d->matrix_response=r;d->matrix_recorded=true;
 if(rc||memcmp(&r,&before,offsetof(struct remesa_matrix_submit_request,operation))||
    r.error||r.retained||r.response_reserved[0]||r.response_reserved[1]||r.stages!=8||
    r.operation||r.cookie||r.core||r.revision||r.pds_base||r.phase||r.ledger)
  return private_lab_uncertain(d);
 d->matrix_session_open=false;d->matrix_color=d->matrix_depth=0;return 0;
}
int remesa_linux_memory_matrix_submit(struct remesa_linux_memory *d,
 const struct remesa_scene_packet *packet,uint32_t budget,struct remesa_matrix_submit_request *out)
{
 struct remesa_matrix_submit_request r={.version=1,.budget_ms=budget},before;int rc,saved;
 if(!d||!packet||(uintptr_t)packet%_Alignof(struct remesa_scene_packet)||
    !general_host_valid(d,packet,sizeof(*packet))||!out||
    (uintptr_t)out%_Alignof(struct remesa_matrix_submit_request)||!general_host_valid(d,out,sizeof(*out))||
    scene_host_overlap(packet,sizeof(*packet),out,sizeof(*out)))return EINVAL;
 if(d->private_poison)return EOWNERDEAD;
 if(d->private_active)return EBUSY;
 if(d->private_attempted||(d->scene_attempted&&
    !(d->matrix_continuous&&d->matrix_session_open&&d->scene_protocol==3)))return EALREADY;
 if(d->recorder.record)return EOPNOTSUPP;
 r.context=d->context;r.packet=*packet;r.flags=d->matrix_continuous?REMESA_MATRIX_KEEP_SESSION:0;
 if(packet->controls.schema==3){
  if(!d->matrix_continuous||!d->matrix_session_open||!d->matrix_depth||
     packet->controls.color_id!=d->matrix_color||packet->controls.depth_id!=d->matrix_depth)return EACCES;
  r.flags|=REMESA_MATRIX_PRESERVE_DEPTH;
 }
 if(!remesa_matrix_submit_request_valid(&r))return EINVAL;
 for(unsigned i=0;i<32;i++)if(d->objects[i].cpu)return EBUSY;
 for(unsigned i=0;i<packet->binding_count;i++){
  struct held *b=find(d,packet->bindings[i].id);
  if(!b||!b->gpu_va)return EINVAL;
  if(b->poison)return private_lab_uncertain(d);
 }
 /* Cached owned IDs are local eligibility, never native address authority.
  * The kernel resolves generations/VA/permissions and audits actual mappings
  * under job holds. Do not add redundant4KiB INFO exchanges for each binding. */
 before=r;d->scene_attempted=true;d->scene_protocol=3;
 rc=ioctl(d->fd,DRM_IOCTL_REMESA_MATRIX_SUBMIT,&r);saved=errno;
 d->matrix_response=r;d->matrix_recorded=true;*out=r;
 if(rc<0){
  if(rc==-1&&!memcmp(&r,&before,sizeof r)&&
     (saved==EINVAL||saved==ENOTTY||saved==EOPNOTSUPP||saved==EPERM||saved==EACCES||
      saved==EBUSY||saved==ENOMEM||saved==ENOENT||saved==ENOSPC))return saved;
  return private_lab_uncertain(d);
 }
 if(rc||memcmp(&r,&before,offsetof(struct remesa_matrix_submit_request,operation))||
    r.response_reserved[0]||r.response_reserved[1]||r.retained)return private_lab_uncertain(d);
 if(r.error){
  if(r.error>=0||r.error<-4095||r.cookie||r.phase||r.ledger||
     !((r.stages==8&&!r.operation)||(r.stages==9&&r.operation>d->copy_operation&&
       r.operation>d->private_operation&&r.operation<=UINT32_MAX)))return private_lab_uncertain(d);
  d->matrix_session_open=false;return -r.error;
 }
 if(r.stages!=(d->matrix_continuous?(7U|REMESA_MATRIX_SESSION_READY):15U)||!r.operation||r.operation>UINT32_MAX||
    r.operation<=d->copy_operation||r.operation<=d->private_operation||r.cookie!=r.operation||
    r.core!=0x01130000U||r.revision!=0x10201U||r.pds_base!=0x20000000U||r.phase!=9||r.ledger!=7)
  return private_lab_uncertain(d);
 d->private_operation=r.operation;d->matrix_session_open=d->matrix_continuous;d->matrix_color=d->matrix_continuous?r.packet.controls.color_id:0;
 d->matrix_depth=d->matrix_continuous?r.packet.controls.depth_id:0;return 0;
}
enum remesa_dispatch remesa_linux_memory_matrix_dispatch(void *opaque,
 const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *out)
{
 struct remesa_linux_memory *d=opaque;
 if(!d||!submit||!out||(uintptr_t)submit%_Alignof(struct remesa_gpu_scene_submit)||
    (uintptr_t)out%_Alignof(struct remesa_gpu_scene_receipt)||
    !general_host_valid(d,submit,sizeof(*submit))||!general_host_valid(d,out,sizeof(*out))||
    scene_host_overlap(submit,sizeof(*submit),out,sizeof(*out)))return REMESA_NOT_ISSUED;
 if(!submit->packet||(uintptr_t)submit->packet%_Alignof(struct remesa_scene_packet)||
    !general_host_valid(d,submit->packet,sizeof(*submit->packet))||
    scene_host_overlap(out,sizeof(*out),submit->packet,sizeof(*submit->packet))||
    submit->context!=d->context||!submit->operation)return REMESA_NOT_ISSUED;
 if(d->private_poison)return REMESA_ISSUANCE_AMBIGUOUS;
 struct remesa_matrix_submit_request response;
 int error=remesa_linux_memory_matrix_submit(d,submit->packet,1000,&response);
 if(error)return d->private_poison?REMESA_ISSUANCE_AMBIGUOUS:REMESA_NOT_ISSUED;
 *out=(struct remesa_gpu_scene_receipt){.context=submit->context,.operation=submit->operation,
  .retired=true,.published=true,.acknowledged=true};return REMESA_ISSUED;
}
int remesa_linux_memory_matrix_response(const struct remesa_linux_memory *d,struct remesa_matrix_submit_request *out)
{
 if(!d||!out||(uintptr_t)out%_Alignof(struct remesa_matrix_submit_request)||
    !general_host_valid(d,out,sizeof(*out)))return EINVAL;
 if(!d->matrix_recorded||d->scene_protocol!=3)return ENODATA;
 *out=d->matrix_response;return 0;
}
