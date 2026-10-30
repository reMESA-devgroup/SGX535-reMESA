/* SPDX-License-Identifier: MIT. /dev/null plus interposed ioctl, never GPU. */
#define main probe_main
#include "../private_session_c_probe.c"
#undef main
#include <assert.h>
#include <stdarg.h>

int __wrap_ioctl(int,unsigned long,...);
static struct {unsigned char *data;size_t bytes;uint64_t va;bool pin,lease,write;} bo[13];
static unsigned created,destroyed,sequence,closes,variant;
static bool active;
static unsigned char *pose[2];
int __wrap_ioctl(int fd,unsigned long command,...)
{
 assert(fd>=0);va_list args;va_start(args,command);void *p=va_arg(args,void *);va_end(args);
 if(command==DRM_IOCTL_REMESA_PRIVATE_SESSION_LAB){
  struct remesa_private_session_lab_request *r=p;assert(r->context==77&&r->version==1);
  if(r->action==1){assert(!active&&created==13);active=true;r->session=1;r->state=1;r->stages=16;r->core=0x01130000;r->revision=0x00010201;return 0;}
  assert(active&&r->session==1);
  if(r->action==2){
   assert(r->sequence==sequence+1&&!memcmp(bo[0].data,pose[r->sequence==2],bo[0].bytes));sequence++;
   r->operation=r->cookie=sequence+1;r->state=1;r->stages=15;r->phase=9;r->ledger=7;r->core=0x01130000;r->revision=0x00010201;
   if(variant==1&&sequence==2)r->cookie++;
   return 0;
  }
  assert(r->action==3&&r->sequence==3&&sequence==3);closes++;
  if(variant==3){r->error=-EBUSY;r->state=1;return 0;}
  active=false;r->state=3;r->stages=32;return 0;
 }
 assert(command==DRM_IOCTL_REMESA_MEMORY);struct remesa_memory_request *r=p;
 if(r->action==REMESA_MEM_OPEN){assert(!r->context);r->context=77;return 0;}
 assert(r->context==77);
 if(r->action==REMESA_MEM_CREATE){assert(created<13);r->id=32+created;bo[created].bytes=r->bytes;bo[created].data=calloc(1,r->bytes);assert(bo[created].data);created++;return 0;}
 if(r->action==REMESA_MEM_INFO){
  struct remesa_memory_info info={0};
  for(unsigned i=0;i<created;i++)if(bo[i].data){info.objects++;info.allocated_bytes+=bo[i].bytes;info.pinned_objects+=bo[i].pin;info.mapped_pages+=bo[i].va?bo[i].bytes/4096:0;info.leases+=bo[i].lease;}
  if(r->id){unsigned i=r->id-32;assert(i<created&&bo[i].data);info.object_bytes=bo[i].bytes;info.gpu_va=bo[i].va;info.object_pinned=bo[i].pin;info.object_lease=bo[i].lease;info.object_mapped_pages=info.verified_pages=bo[i].va?bo[i].bytes/4096:0;}
  memcpy(r->data,&info,sizeof(info));return 0;
 }
 if(r->action==REMESA_MEM_CLOSE){assert(!active&&destroyed==13);return 0;}
 unsigned i=r->id-32;assert(i<created&&bo[i].data);
 switch(r->action){
 case REMESA_MEM_PIN:bo[i].pin=true;break;
 case REMESA_MEM_MAP:bo[i].va=r->va;break;
 case REMESA_MEM_UNMAP:assert(!active);bo[i].va=0;break;
 case REMESA_MEM_UNPIN:assert(!active);bo[i].pin=false;break;
 case REMESA_MEM_DESTROY:
  assert(!active);if(variant==4){errno=EBUSY;return -1;}
  free(bo[i].data);bo[i].data=NULL;destroyed++;break;
 case REMESA_MEM_LEASE:
  if(variant==6&&!active&&i==0){errno=EINTR;return -1;}
  assert(!bo[i].lease);bo[i].lease=true;bo[i].write=r->flags;break;
 case REMESA_MEM_UNLEASE:assert(bo[i].lease);bo[i].lease=false;break;
 case REMESA_MEM_SYNC:assert(!active);break;
 case REMESA_MEM_READ:
  assert(bo[i].lease&&!bo[i].write&&r->offset+r->length<=bo[i].bytes);memcpy(r->data,bo[i].data+r->offset,r->length);
  if(variant==2&&sequence==2&&i==0&&!r->offset)r->data[0]^=1;
  break;
 case REMESA_MEM_WRITE:
  assert(bo[i].lease&&bo[i].write&&r->offset+r->length<=bo[i].bytes);
  if(variant==5&&active){errno=EINTR;return -1;}
  memcpy(bo[i].data+r->offset,r->data,r->length);break;
 default:abort();
 }
 return 0;
}
int main(int argc,char **argv)
{
 assert(argc==4);pose[0]=malloc(131072);pose[1]=malloc(131072);assert(pose[0]&&pose[1]);
 assert(!load(argv[1],"role0.bin",pose[0],131072)&&!load(argv[2],"role0.bin",pose[1],131072));
 for(variant=0;variant<=6;variant++){
  char output[4096];int n=snprintf(output,sizeof(output),"%s/model-%u",argv[3],variant);assert(n>0&&(unsigned)n<sizeof(output));
  char *args[]={"model-c-probe","--execute","/dev/null",argv[1],output,argv[2],NULL};
  int e=probe_main(6,args);
  if(!variant)assert(!e&&created==13&&destroyed==13&&sequence==3&&closes==1&&!active);
  else{
   assert(e&&destroyed==0);
   if(variant==1||variant==2)assert(sequence==2&&!closes&&active);
   if(variant==3)assert(sequence==3&&closes==1&&active);
   if(variant==4)assert(sequence==3&&closes==1&&!active);
   if(variant==5)assert(sequence==1&&!closes&&active&&bo[0].lease);
   if(variant==6)assert(!sequence&&!closes&&!active&&created==1);
  }
  /* Explicit host MODEL teardown, not a kernel acknowledgement or GPU fence. */
  for(unsigned i=0;i<13;i++)free(bo[i].data);
  memset(bo,0,sizeof(bo));created=destroyed=sequence=closes=0;active=false;
 }
 free(pose[0]);free(pose[1]);puts("PASS C collector model: three poses, no reupload/sync of unchanged inputs, receipt/input/upload/CLOSE/cleanup failures retain backing; /dev/null only, GPU0");return 0;
}
