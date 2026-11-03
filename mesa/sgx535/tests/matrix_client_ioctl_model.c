/* SPDX-License-Identifier: MIT
 * Link only into offline client tests with --wrap=ioctl. Heap storage and
 * modeled receipts; NO GPU execution, software rasterization or pixel proof.
 */
#include "remesa/memory_uapi.h"
#include "remesa/matrix_submit_uapi.h"
#include "remesa/twod_fill_uapi.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct model_bo {unsigned char *data;uint64_t bytes,va;unsigned generation;int pinned,lease;};
static struct model_bo objects[32];
static unsigned fills,draws;
static uint64_t operation;
int __wrap_ioctl(int fd,unsigned long command,...);
static struct model_bo *object(unsigned id)
{
 struct model_bo *b=&objects[id&31];
 assert(b->data&&b->generation==id>>5);return b;
}
static void info(struct remesa_memory_request *r)
{
 struct remesa_memory_info i={0};
 for(unsigned n=0;n<32;n++)if(objects[n].data){
  struct model_bo *b=&objects[n];i.objects++;i.allocated_bytes+=b->bytes;
  i.pinned_objects+=b->pinned;i.leases+=b->lease;i.mapped_pages+=b->va?b->bytes/4096:0;
 }
 if(r->id){
  struct model_bo *b=object(r->id);i.object_bytes=b->bytes;i.gpu_va=b->va;
  i.object_pinned=b->pinned;i.object_lease=b->lease;
  i.object_mapped_pages=i.verified_pages=b->va?b->bytes/4096:0;
 }
 memcpy(r->data,&i,sizeof i);
}
int __wrap_ioctl(int fd,unsigned long command,...)
{
 assert(fd==99);va_list ap;va_start(ap,command);void *data=va_arg(ap,void *);va_end(ap);
 if(command==DRM_IOCTL_REMESA_MEMORY){
  struct remesa_memory_request *r=data;assert(r->version==1);
  if(r->action==REMESA_MEM_OPEN){assert(!r->context);r->context=77;return 0;}
  assert(r->context==77);
  if(r->action==REMESA_MEM_INFO){info(r);return 0;}
  if(r->action==REMESA_MEM_CLOSE){for(unsigned i=0;i<32;i++)assert(!objects[i].data);return 0;}
  if(r->action==REMESA_MEM_CREATE){
   assert(r->bytes&&!(r->bytes%4096));
   for(unsigned i=0;i<32;i++)if(!objects[i].data){
    struct model_bo *b=&objects[i];b->data=calloc(1,r->bytes);assert(b->data);
    b->bytes=r->bytes;b->generation++;r->id=(b->generation<<5)|i;return 0;
   }
   assert(0);
  }
  struct model_bo *b=object(r->id);
  switch(r->action){
  case REMESA_MEM_PIN:assert(!b->pinned);b->pinned=1;break;
  case REMESA_MEM_UNPIN:assert(b->pinned&&!b->va&&!b->lease);b->pinned=0;break;
  case REMESA_MEM_MAP:assert(b->pinned&&!b->va);b->va=r->va;break;
  case REMESA_MEM_UNMAP:assert(b->va&&!b->lease);b->va=0;break;
  case REMESA_MEM_DESTROY:assert(!b->va&&!b->lease&&!b->pinned);free(b->data);b->data=NULL;break;
  case REMESA_MEM_LEASE:assert(!b->lease&&b->pinned);b->lease=1;break;
  case REMESA_MEM_UNLEASE:assert(b->lease);b->lease=0;break;
  case REMESA_MEM_READ:case REMESA_MEM_WRITE:
   assert(b->lease&&r->offset<=b->bytes&&r->length<=b->bytes-r->offset);
   if(r->action==REMESA_MEM_READ)memcpy(r->data,b->data+r->offset,r->length);
   else memcpy(b->data+r->offset,r->data,r->length);
   break;
  case REMESA_MEM_SYNC:assert(b->pinned&&!b->lease);break;
  default:assert(0);
  }
  return 0;
 }
 if(command==DRM_IOCTL_REMESA_TWOD_FILL){
  struct remesa_twod_fill_request *r=data;struct model_bo *b=object(r->destination.id);
  assert(!fills++&&!draws&&r->context==77&&b->pinned&&b->va&&!b->lease);
  assert(!r->color&&!r->destination_x&&!r->destination_y);
  assert(r->width==r->destination.width&&r->height==r->destination.height);
  /* Do not draw or mutate pixels. The freshly allocated zeros are unchanged. */
  r->operation=r->cookie=++operation;r->stages=31;r->core=0x01130000;r->revision=0x10201;return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MATRIX_SUBMIT); /* Every LAB ioctl fails. */
 struct remesa_matrix_submit_request *r=data;
 assert(fills==1&&!draws++&&remesa_matrix_submit_request_valid(r));
 for(unsigned i=0;i<18;i++){
  struct model_bo *b=object(r->packet.bindings[i].id);assert(b->pinned&&b->va&&!b->lease);
 }
 r->operation=r->cookie=++operation;r->core=0x01130000;r->revision=0x10201;
 r->pds_base=0x20000000;r->phase=9;r->ledger=7;r->stages=15;
 fprintf(stdout,"MODEL_ONLY ordinary Mesa->fill->matrix transport: one fill, one submit, GPU_operations=0\n");
 return 0;
}
