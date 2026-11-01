/* SPDX-License-Identifier: MIT. Transport metadata only; no device or GPU. */
#include "remesa_linux_memory.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static unsigned calls,next_id;static bool uncertain_create;
int __wrap_ioctl(int fd,unsigned long command,...)
{
 assert(fd==99&&command==DRM_IOCTL_REMESA_MEMORY);calls++;
 va_list ap;va_start(ap,command);struct remesa_memory_request *r=va_arg(ap,void *);va_end(ap);
 if(r->action==REMESA_MEM_CREATE){
  if(uncertain_create){errno=EINTR;return -1;}
  r->id=32+next_id++;
 }
 return 0;
}
int main(void)
{
 struct remesa_linux_memory *d=NULL;assert(!remesa_linux_memory_attach(99,77,&d));
 const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();
 struct remesa_linux_memory_inventory v,before;memset(&v,0xa5,sizeof v);before=v;
 assert(remesa_linux_memory_inventory_snapshot(NULL,&v)==EINVAL&&!memcmp(&v,&before,sizeof v));
 assert(remesa_linux_memory_inventory_snapshot(d,NULL)==EINVAL);
 assert(remesa_linux_memory_inventory_snapshot(d,(void *)d)==EINVAL);
 assert(remesa_linux_memory_inventory_snapshot(d,(void *)(UINTPTR_MAX&~(uintptr_t)7))==EINVAL);
 unsigned count=calls;assert(!remesa_linux_memory_inventory_snapshot(d,&v));assert(v.version==1&&v.context==77&&!v.count&&calls==count);
 uint32_t id;assert(!ops->create(d,4096,&id)&&id==32);assert(!ops->pin(d,id));assert(!ops->gpu_map(d,id,0x20000000,4096));
 count=calls;assert(!remesa_linux_memory_inventory_snapshot(d,&v)&&v.count==1&&calls==count);
 assert(v.entries[0].id==32&&v.entries[0].bytes==4096&&v.entries[0].gpu_va==0x20000000&&v.entries[0].flags==REMESA_MEMORY_INVENTORY_LIVE);
 void *cpu=NULL;assert(!ops->cpu_map(d,id,true,&cpu)&&cpu);count=calls;
 unsigned char saved[sizeof v];memcpy(saved,cpu,sizeof saved);
 assert(remesa_linux_memory_inventory_snapshot(d,cpu)==EINVAL&&!memcmp(cpu,saved,sizeof saved)&&calls==count);
 assert(!remesa_linux_memory_inventory_snapshot(d,&v)&&calls==count);
 assert(v.entries[0].flags==(REMESA_MEMORY_INVENTORY_LIVE|REMESA_MEMORY_INVENTORY_LEASE|REMESA_MEMORY_INVENTORY_WRITE));
 assert(!ops->cpu_unmap(d,id,true));assert(!ops->gpu_unmap(d,id,0x20000000,4096));assert(!ops->unpin(d,id));assert(!ops->destroy(d,id));
 count=calls;assert(!remesa_linux_memory_inventory_snapshot(d,&v)&&!v.count&&calls==count);
 /* All32 host slots, including objects not referenced by a scene graph. */
 uint32_t ids[32];for(unsigned i=0;i<32;i++)assert(!ops->create(d,4096,&ids[i]));
 count=calls;assert(!remesa_linux_memory_inventory_snapshot(d,&v)&&v.count==32&&calls==count);
 for(unsigned i=0;i<32;i++)assert(v.entries[i].id==ids[i]&&v.entries[i].bytes==4096&&!v.entries[i].gpu_va&&v.entries[i].flags==REMESA_MEMORY_INVENTORY_LIVE);
 assert(ops->create(d,4096,&id)==ENOSPC&&calls==count);
 for(unsigned i=0;i<32;i++)assert(!ops->destroy(d,ids[i]));
 uncertain_create=true;assert(ops->create(d,4096,&id)==EOWNERDEAD&&!id);count=calls;
 assert(!remesa_linux_memory_inventory_snapshot(d,&v)&&v.count==1&&calls==count);
 assert(!v.entries[0].id&&!v.entries[0].bytes&&!v.entries[0].gpu_va&&v.entries[0].flags==REMESA_MEMORY_INVENTORY_POISON);
 remesa_linux_memory_forget(d); /* Drop modeled metadata only, never recovery. */
 puts("PASS read-only actual transport inventory: empty/32slots/CPU-only/leases/aliases/uncertain CREATE, no ioctl or ownership mutation; GPU0");
 return 0;
}
