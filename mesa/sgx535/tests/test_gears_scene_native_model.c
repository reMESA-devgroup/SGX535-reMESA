/* SPDX-License-Identifier: MIT. Actual native client/transport; synthetic kernel
 * memory and receipts only. No vertex/raster execution or GPU emulation.
 * Optional synthetic row tags check readback copying/origin, not rendering. */
#include "../../../tools/remesa/scene_fixture.h"
#define __wrap_ioctl previous_gears_memory_ioctl
#include "test_scene_native_client_model.c"
#undef __wrap_ioctl
static bool returned_gears,teardown_gears;
static unsigned reads_retired;
static struct remesa_gears_scene_probe_request model_receipt;
static struct remesa_scene_fixture model_placement;
static bool model_check_registered;
#ifdef REMESA_GEARS_MODEL_GENERAL
static bool general_active,general_internal,general_closed,general_failed;
static unsigned general_calls,general_completed,general_reads_before;
static struct remesa_general_session_request general_opened;
#endif
#ifdef REMESA_GEARS_MODEL_OPENGL
#define MODEL_SOURCE_OWNERS 2u
#else
#define MODEL_SOURCE_OWNERS 3u
#endif
static void check_model_exit(void)
{
 unsigned live=0;for(unsigned k=0;k<count;k++)if(bo[k].bytes){live++;assert(bo[k].pin&&bo[k].va&&!bo[k].lease);}
#ifdef REMESA_GEARS_MODEL_GENERAL
 if(general_calls){
  if(general_failed){assert(general_active&&!general_closed&&live==22+MODEL_SOURCE_OWNERS&&!teardown_gears);}
  else assert(submissions==2&&general_completed==2&&general_closed&&!general_active&&live==4+MODEL_SOURCE_OWNERS);
  return;
 }
#endif
 assert(submissions<=1);
 if(getenv("REMESA_GEARS_MODEL_BAD_RECEIPT")||getenv("REMESA_GEARS_MODEL_IOCTL_ERROR")){
  assert(submissions==1&&live==22+MODEL_SOURCE_OWNERS&&!reads_retired&&!teardown_gears);
 }else assert(live==4+MODEL_SOURCE_OWNERS);
}

int __wrap_ioctl(int fd,unsigned long command,...)
{
 if(!model_check_registered){assert(!atexit(check_model_exit));model_check_registered=true;}
 va_list ap;va_start(ap,command);void *arg=va_arg(ap,void *);va_end(ap);assert(fd==99);
#ifdef REMESA_GEARS_MODEL_GENERAL
 if(command==DRM_IOCTL_REMESA_GENERAL_SESSION){
  struct remesa_general_session_request *r=arg;assert(remesa_general_session_request_valid(r));general_calls++;
  unsigned fail_action=getenv("REMESA_GENERAL_MODEL_FAIL_ACTION")?(unsigned)atoi(getenv("REMESA_GENERAL_MODEL_FAIL_ACTION")):0;
  unsigned fail_sequence=getenv("REMESA_GENERAL_MODEL_FAIL_SEQUENCE")?(unsigned)atoi(getenv("REMESA_GENERAL_MODEL_FAIL_SEQUENCE")):0;
  bool fail=r->action==fail_action&&(!fail_sequence||r->sequence==fail_sequence);
  if(r->action==1){
   assert(!general_active&&!general_closed&&r->max_jobs==2);
#ifdef REMESA_GEARS_MODEL_HOMOGENEOUS
   assert(r->profile==3||r->profile==4);
#else
   assert(r->profile==5||r->profile==6);
#endif
   general_opened=*r;general_active=true;r->session=7;r->state=1;r->stages=16;
   r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;
  }else if(r->action==2){
   assert(general_active&&!general_closed&&r->session==7&&r->sequence==general_completed+1&&r->budget_ms==1000);
   if(general_completed)assert(reads_retired>general_reads_before);
   general_reads_before=reads_retired;
   struct remesa_gears_scene_probe_request legacy={.version=2,.profile=general_opened.profile,.context=r->context,.budget_ms=r->budget_ms};
   memcpy(legacy.ids,general_opened.ids,sizeof legacy.ids);
   general_internal=true;int e=__wrap_ioctl(fd,DRM_IOCTL_REMESA_GEARS_SCENE_PROBE,&legacy);general_internal=false;assert(!e);
   general_completed++;model_receipt.operation=model_receipt.cookie=general_completed;
   r->operation=r->cookie=general_completed;r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;
   r->phase=9;r->ledger=7;r->stages=15;r->state=1;
  }else{
   assert(general_active&&general_completed==2&&reads_retired>general_reads_before&&r->sequence==2&&r->session==7);
   r->state=3;r->stages=32;
   if(!fail){general_closed=true;general_active=false;}
  }
  if(fail){general_failed=true;errno=EINTR;return -1;}
  return 0;
 }
#endif
 if(command==DRM_IOCTL_REMESA_GEARS_SCENE_PROBE){
  struct remesa_gears_scene_probe_request *r=arg;
#ifdef REMESA_GEARS_MODEL_GENERAL
  assert(general_internal&&general_active&&submissions<2);submissions++;
#else
  assert(!submissions++);
#endif
  assert(r->version==2&&r->context==77&&r->profile>=1&&r->profile<=REMESA_GEARS_SCENE_PROFILE_COUNT&&!r->reserved_input);
#ifdef REMESA_GEARS_MODEL_OPENGL
  assert(r->profile==5||r->profile==6);
#elif defined(REMESA_GEARS_MODEL_HOMOGENEOUS)
  assert(r->profile==3||r->profile==4);
#else
  assert(r->profile==1||r->profile==2);
#endif
  const char *dir=getenv("REMESA_NATIVE_MODEL_OUTPUT");assert(dir);char path[4096];assert(snprintf(path,sizeof path,"%s/placement.bin",dir)>0);FILE *f=fopen(path,"rb");assert(f);
  assert(fread(&model_placement,1,sizeof model_placement,f)==sizeof model_placement&&model_placement.count==18&&fgetc(f)==EOF&&!fclose(f));
  for(unsigned i=0;i<18;i++){
   const unsigned id=r->ids[i],k=id-32;assert(id==model_placement.bindings[i].id&&k<count&&bo[k].pin&&!bo[k].lease&&bo[k].va==model_placement.bindings[i].address&&bo[k].size==model_placement.bindings[i].bytes);
   if(model_placement.bindings[i].access==1){
#ifdef REMESA_GEARS_MODEL_GENERAL
    assert(snprintf(path,sizeof path,"%s/model-job-%u-bo-%u.bin",dir,submissions,i)>0);
#else
    assert(snprintf(path,sizeof path,"%s/model-bo-%u.bin",dir,i)>0);
#endif
    f=fopen(path,"wbx");assert(f&&fwrite(bo[k].bytes,1,bo[k].size,f)==bo[k].size&&!fclose(f));}
  }
#ifdef REMESA_GEARS_MODEL_OPENGL
  if(getenv("REMESA_OPENGL_MODEL_ROW_TAGS")){
   assert(!getenv("REMESA_GEARS_MODEL_BAD_RECEIPT")&&!getenv("REMESA_GEARS_MODEL_IOCTL_ERROR"));
   unsigned width=r->profile==5?1280:640,height=r->profile==5?800:640;
   /* First frontend allocation is the still-owned COLOR attachment. This
    * asymmetric storage payload exists only in the linked syscall model. */
   assert(bo[0].size==(size_t)width*height*4&&bo[0].va==0x40001000);
   for(unsigned y=0;y<height;y++){uint32_t tag=0xff000000u|y;memcpy(bo[0].bytes+4u*width*y,&tag,4);}
  }
#endif
  r->operation=r->cookie=1;r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;r->phase=9;r->ledger=7;r->stages=15;model_receipt=*r;returned_gears=true;
  if(getenv("REMESA_GEARS_MODEL_BAD_RECEIPT"))r->phase=8;
  if(getenv("REMESA_GEARS_MODEL_IOCTL_ERROR")){errno=EINTR;return -1;}
  return 0;
 }
 if(command==DRM_IOCTL_REMESA_GEARS_SCENE_TRACE){
  struct remesa_gears_scene_trace_request *q=arg;struct remesa_gears_scene_trace zero={0};assert(returned_gears&&q->version==2&&!q->reserved&&q->context==77&&!memcmp(&q->trace,&zero,sizeof zero));
  struct remesa_scene_probe_trace *t=&q->trace.prefix;t->version=2;t->bytes=4280;t->context=77;t->profile=model_receipt.profile;t->operation=t->cookie=1;t->phase=9;t->ledger=7;t->stages=15;t->binding_count=18;t->load_count=29;t->ta_count=31;t->raster_count=20;t->use_mask=24;
  t->event_count=3;t->events[0]=(struct remesa_scene_trace_event){4,1,0x2000,0,0,5,1000};t->events[1]=(struct remesa_scene_trace_event){4,1,4,0,3,8,2000};t->events[2]=(struct remesa_scene_trace_event){4,1,0x40001,0,7,9,3000};
#ifdef REMESA_GEARS_MODEL_GENERAL
  assert(general_active&&!general_failed);t->operation=t->cookie=general_completed;
  for(unsigned k=0;k<3;k++)t->events[k].sequence=general_completed;
#endif
  for(unsigned i=0;i<18;i++){struct remesa_scene_trace_binding *b=i<17?&t->bindings[i]:&q->trace.last_binding;*b=(struct remesa_scene_trace_binding){model_receipt.ids[i],model_placement.bindings[i].access,model_placement.bindings[i].address,model_placement.bindings[i].bytes};}
#ifdef REMESA_GEARS_MODEL_GENERAL
  if(getenv("REMESA_GENERAL_MODEL_BAD_TRACE")&&general_completed==2){t->cookie^=1;general_failed=true;}
#endif
  return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MEMORY);struct remesa_memory_request *r=arg;
 if(r->action==REMESA_MEM_INFO){
  struct remesa_memory_info info={0};for(unsigned k=0;k<count;k++)if(bo[k].bytes){info.objects++;info.allocated_bytes+=bo[k].size;info.pinned_objects+=bo[k].pin;info.mapped_pages+=bo[k].va?bo[k].size/4096:0;info.leases+=bo[k].lease;}
  if(r->id){unsigned k=r->id-32;assert(k<count&&bo[k].bytes);info.object_bytes=bo[k].size;info.gpu_va=bo[k].va;info.object_pinned=bo[k].pin;info.verified_pages=info.object_mapped_pages=bo[k].va?bo[k].size/4096:0;info.object_lease=bo[k].lease;}
  memcpy(r->data,&info,sizeof info);return 0;
 }
 if(r->action==REMESA_MEM_READ&&returned_gears){
#ifdef REMESA_GEARS_MODEL_OPENGL
  /* Actual glReadPixels follows temporary-owner release. It may read only
   * the still-owned COLOR attachment after a valid modeled retirement. */
  assert(!getenv("REMESA_GEARS_MODEL_BAD_RECEIPT")&&!getenv("REMESA_GEARS_MODEL_IOCTL_ERROR"));
  if(teardown_gears){
   unsigned matches=0;for(unsigned i=0;i<18;i++)if(model_receipt.ids[i]==r->id&&model_placement.bindings[i].access==3)matches++;
   assert(matches==1&&r->id==32);
  }
#else
  assert(!teardown_gears);
#endif
  reads_retired++;
 }
 if(r->action==REMESA_MEM_UNMAP||r->action==REMESA_MEM_UNPIN||r->action==REMESA_MEM_DESTROY){
#ifdef REMESA_GEARS_MODEL_GENERAL
  if(general_calls)assert(general_closed&&!general_active&&!general_failed);
#endif
  unsigned k=r->id-32;assert(k<count&&bo[k].bytes&&!bo[k].lease);if(returned_gears){assert(reads_retired);teardown_gears=true;}
  if(r->action==REMESA_MEM_UNMAP){assert(bo[k].va);bo[k].va=0;}
  if(r->action==REMESA_MEM_UNPIN){assert(bo[k].pin&&!bo[k].va);bo[k].pin=false;}
  if(r->action==REMESA_MEM_DESTROY){assert(!bo[k].pin&&!bo[k].va);free(bo[k].bytes);bo[k].bytes=NULL;}
  return 0;
 }
 return previous_gears_memory_ioctl(fd,command,arg);
}
