/* SPDX-License-Identifier: MIT. Actual native client/transport; synthetic kernel
 * memory and receipts only. Never generates pixels or emulates the GPU. */
#include "../../../tools/remesa/scene_fixture.h"
#define __wrap_ioctl previous_gears_memory_ioctl
#include "test_scene_native_client_model.c"
#undef __wrap_ioctl
static bool returned_gears,teardown_gears;
static unsigned reads_retired;
static struct remesa_gears_scene_probe_request model_receipt;
static struct remesa_scene_fixture model_placement;
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list ap;va_start(ap,command);void *arg=va_arg(ap,void *);va_end(ap);assert(fd==99);
 if(command==DRM_IOCTL_REMESA_GEARS_SCENE_PROBE){
  struct remesa_gears_scene_probe_request *r=arg;assert(!submissions++&&r->version==2&&r->context==77&&r->profile>=1&&r->profile<=2&&!r->reserved_input);
  const char *dir=getenv("REMESA_NATIVE_MODEL_OUTPUT");assert(dir);char path[4096];assert(snprintf(path,sizeof path,"%s/placement.bin",dir)>0);FILE *f=fopen(path,"rb");assert(f);
  assert(fread(&model_placement,1,sizeof model_placement,f)==sizeof model_placement&&model_placement.count==18&&fgetc(f)==EOF&&!fclose(f));
  for(unsigned i=0;i<18;i++){
   const unsigned id=r->ids[i],k=id-32;assert(id==model_placement.bindings[i].id&&k<count&&bo[k].pin&&!bo[k].lease&&bo[k].va==model_placement.bindings[i].address&&bo[k].size==model_placement.bindings[i].bytes);
   if(model_placement.bindings[i].access==1){assert(snprintf(path,sizeof path,"%s/model-bo-%u.bin",dir,i)>0);f=fopen(path,"wbx");assert(f&&fwrite(bo[k].bytes,1,bo[k].size,f)==bo[k].size&&!fclose(f));}
  }
  r->operation=r->cookie=1;r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;r->phase=9;r->ledger=7;r->stages=15;model_receipt=*r;returned_gears=true;
  if(getenv("REMESA_GEARS_MODEL_BAD_RECEIPT"))r->phase=8;return 0;
 }
 if(command==DRM_IOCTL_REMESA_GEARS_SCENE_TRACE){
  struct remesa_gears_scene_trace_request *q=arg;struct remesa_gears_scene_trace zero={0};assert(returned_gears&&q->version==2&&!q->reserved&&q->context==77&&!memcmp(&q->trace,&zero,sizeof zero));
  struct remesa_scene_probe_trace *t=&q->trace.prefix;t->version=2;t->bytes=4280;t->context=77;t->profile=model_receipt.profile;t->operation=t->cookie=1;t->phase=9;t->ledger=7;t->stages=15;t->binding_count=18;t->load_count=29;t->ta_count=31;t->raster_count=20;t->use_mask=24;
  t->event_count=3;t->events[0]=(struct remesa_scene_trace_event){4,1,0x2000,0,0,5,1000};t->events[1]=(struct remesa_scene_trace_event){4,1,4,0,3,8,2000};t->events[2]=(struct remesa_scene_trace_event){4,1,0x40001,0,7,9,3000};
  for(unsigned i=0;i<18;i++){struct remesa_scene_trace_binding *b=i<17?&t->bindings[i]:&q->trace.last_binding;*b=(struct remesa_scene_trace_binding){model_receipt.ids[i],model_placement.bindings[i].access,model_placement.bindings[i].address,model_placement.bindings[i].bytes};}
  return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MEMORY);struct remesa_memory_request *r=arg;
 if(r->action==REMESA_MEM_INFO){
  struct remesa_memory_info info={0};for(unsigned k=0;k<count;k++)if(bo[k].bytes){info.objects++;info.allocated_bytes+=bo[k].size;info.pinned_objects+=bo[k].pin;info.mapped_pages+=bo[k].va?bo[k].size/4096:0;info.leases+=bo[k].lease;}
  if(r->id){unsigned k=r->id-32;assert(k<count&&bo[k].bytes);info.object_bytes=bo[k].size;info.gpu_va=bo[k].va;info.object_pinned=bo[k].pin;info.verified_pages=info.object_mapped_pages=bo[k].va?bo[k].size/4096:0;info.object_lease=bo[k].lease;}
  memcpy(r->data,&info,sizeof info);return 0;
 }
 if(r->action==REMESA_MEM_READ&&returned_gears){assert(!teardown_gears);reads_retired++;}
 if(r->action==REMESA_MEM_UNMAP||r->action==REMESA_MEM_UNPIN||r->action==REMESA_MEM_DESTROY){
  unsigned k=r->id-32;assert(k<count&&bo[k].bytes&&!bo[k].lease);if(returned_gears){assert(reads_retired);teardown_gears=true;}
  if(r->action==REMESA_MEM_UNMAP){assert(bo[k].va);bo[k].va=0;}
  if(r->action==REMESA_MEM_UNPIN){assert(bo[k].pin&&!bo[k].va);bo[k].pin=false;}
  if(r->action==REMESA_MEM_DESTROY){assert(!bo[k].pin&&!bo[k].va);free(bo[k].bytes);bo[k].bytes=NULL;}
  return 0;
 }
 return previous_gears_memory_ioctl(fd,command,arg);
}
