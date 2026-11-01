/* SPDX-License-Identifier: MIT. CPU syscall model only; never creates pixels. */
#include "remesa_linux_memory.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct {unsigned char *bytes;uint64_t size,va;bool pin,lease,write;} bo[32];
static unsigned count,submissions;
int __wrap_ioctl(int fd,unsigned long command,...)
{
 assert(fd==99);va_list ap;va_start(ap,command);void *arg=va_arg(ap,void*);va_end(ap);
 if(command==DRM_IOCTL_REMESA_SCENE_PROBE){
  struct remesa_scene_probe_request *r=arg;assert(!submissions++&&r->context==77&&r->profile>=5&&r->profile<=6);
  const char *dir=getenv("REMESA_NATIVE_MODEL_OUTPUT");assert(dir);
  char path[4096];assert(snprintf(path,sizeof path,"%s/placement.bin",dir)>0);FILE *f=fopen(path,"rb");assert(f);
  struct {uint32_t version,count;struct {uint64_t address,bytes;uint32_t id,access;} bindings[32];} p;
  assert(fread(&p,1,sizeof p,f)==sizeof p&&p.count==17);fclose(f);
  for(unsigned i=0;i<17;i++){
   assert(r->ids[i]==p.bindings[i].id);unsigned k=r->ids[i]-32;assert(k<count&&bo[k].pin&&!bo[k].lease&&bo[k].va==p.bindings[i].address&&bo[k].size==p.bindings[i].bytes);
   if(p.bindings[i].access==1){assert(snprintf(path,sizeof path,"%s/model-bo-%u.bin",dir,i)>0);f=fopen(path,"wbx");assert(f);assert(fwrite(bo[k].bytes,1,bo[k].size,f)==bo[k].size);assert(!fclose(f));}
  }
  r->operation=r->cookie=1;r->core=0x01130000;r->revision=0x10201;r->pds_base=0x20000000;r->phase=9;r->ledger=7;r->stages=15;return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MEMORY);struct remesa_memory_request *r=arg;
 if(r->action==REMESA_MEM_OPEN){assert(!r->context);r->context=77;return 0;}
 assert(r->context==77);
 if(r->action==REMESA_MEM_CREATE){assert(count<32);r->id=32+count;bo[count].size=r->bytes;bo[count].bytes=calloc(1,r->bytes);assert(bo[count].bytes);count++;return 0;}
 if(r->action==REMESA_MEM_INFO){struct remesa_memory_info info={.objects=count};for(unsigned i=0;i<count;i++){info.allocated_bytes+=bo[i].size;info.pinned_objects+=bo[i].pin;info.mapped_pages+=bo[i].va?bo[i].size/4096:0;info.leases+=bo[i].lease;}memcpy(r->data,&info,sizeof info);return 0;}
 unsigned i=r->id-32;assert(i<count);
 switch(r->action){
 case REMESA_MEM_PIN:assert(!bo[i].pin);bo[i].pin=true;break;
 case REMESA_MEM_MAP:assert(bo[i].pin&&!bo[i].va);for(unsigned k=0;k<count;k++)if(bo[k].va)assert(r->va+bo[i].size<=bo[k].va||bo[k].va+bo[k].size<=r->va);bo[i].va=r->va;break;
 case REMESA_MEM_LEASE:assert(!bo[i].lease);bo[i].lease=true;bo[i].write=r->flags;break;
 case REMESA_MEM_READ:case REMESA_MEM_WRITE:
  assert(bo[i].lease&&r->offset<=bo[i].size&&r->length<=bo[i].size-r->offset);
  if(r->action==REMESA_MEM_WRITE){assert(bo[i].write);memcpy(bo[i].bytes+r->offset,r->data,r->length);}else memcpy(r->data,bo[i].bytes+r->offset,r->length);break;
 case REMESA_MEM_UNLEASE:assert(bo[i].lease);bo[i].lease=false;break;
 case REMESA_MEM_SYNC:assert(bo[i].pin&&!bo[i].lease);break;
 default:assert(0);
 }
 return 0;
}
