/* SPDX-License-Identifier: MIT */
/* Bounded real GEM/PTE resource check through existing transport. NO GPU draw.
 * Compile/run is separate from the root controller's temporary resource gate. */
#define _GNU_SOURCE
#include "remesa_linux_memory.h"
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
struct item {const char *name;size_t bytes;uint64_t va;uint32_t id;bool pinned,mapped;};
static FILE *journal;
static int record(void *p,const struct remesa_memory_request *in,
 const struct remesa_memory_request *out,int result,int error,bool before)
{
 (void)p;uint32_t h[4]={before?0u:1u,(uint32_t)result,(uint32_t)error,sizeof(*in)};
 const struct remesa_memory_request *r=before?in:out;
 if(!r||fwrite(h,sizeof(h),1,journal)!=1||fwrite(r,sizeof(*r),1,journal)!=1||fflush(journal))return EIO;
 return 0;
}
static unsigned char pattern(size_t i,unsigned role)
{return (unsigned char)((i>>12)^i^((role+1)*73u));}
int main(int argc,char **argv)
{
 struct remesa_scene_layout layout;int e=remesa_scene_layout_build(1280,800,0,&layout);if(e)return 2;
 struct item items[3]={{"color",1280u*800u*4u,UINT64_C(0x40001000),0,false,false},
  {"z",1280u*800u*4u,UINT64_C(0x403e9000),0,false,false},
  {"region",(size_t)layout.allocation_pages*4096u,UINT64_C(0x407d1000),0,false,false}};
 if(argc==2&&!strcmp(argv[1],"--describe")){
  printf("RESOURCE_ONLY1280x800 COLOR=%zu Z=%zu REGION=%zu cookie7=%u; NO_SUBMISSION\n",items[0].bytes,items[1].bytes,items[2].bytes,layout.cookie[7]);return 0;
 }
 if(argc!=3||strcmp(argv[1],"--resource-only")||strcmp(argv[2],"/dev/dri/card0"))return 2;
 for(unsigned i=1;i<3;i++)if(items[i-1].va+items[i-1].bytes>items[i].va)return 2;
 journal=fopen("resource-transcript.bin","wbx");if(!journal)return 3;
 int fd=open(argv[2],O_RDWR|O_NOFOLLOW|O_CLOEXEC);if(fd<0){perror("open DRM");fclose(journal);return 4;}
 struct stat st;if(fstat(fd,&st)||!S_ISCHR(st.st_mode)||major(st.st_rdev)!=226||minor(st.st_rdev)!=0){close(fd);fclose(journal);return 5;}
 struct remesa_linux_memory *memory=NULL;const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();
 const struct remesa_memory_recorder recorder={record,NULL};
 e=remesa_linux_memory_begin_recorded(fd,&memory,&recorder);if(e)goto failed;
 for(unsigned role=0;role<3;role++){
  struct item *b=&items[role];void *p=NULL;
  e=ops->create(memory,b->bytes,&b->id);if(e)goto failed;
  e=ops->pin(memory,b->id);if(e)goto failed;b->pinned=true;
  e=ops->gpu_map(memory,b->id,b->va,b->bytes);if(e)goto failed;b->mapped=true;
  e=remesa_linux_memory_validate_backing(memory,b->id,b->va,b->bytes);if(e)goto failed;
  e=ops->cpu_map(memory,b->id,true,&p);if(e||!p){if(!e)e=EPROTO;goto failed;}
  for(size_t i=0;i<b->bytes;i++)if(((unsigned char *)p)[i]){e=EIO;goto failed;}
  for(size_t i=0;i<b->bytes;i++)((unsigned char *)p)[i]=pattern(i,role);
  e=ops->cpu_unmap(memory,b->id,true);if(e)goto failed;
  e=ops->sync_for_device(memory,b->id);if(e)goto failed;
  e=ops->cpu_map(memory,b->id,false,&p);if(e||!p){if(!e)e=EPROTO;goto failed;}
  char file[64];snprintf(file,sizeof(file),"%s-CPU-roundtrip.bin",b->name);
  FILE *raw=fopen(file,"wbx");if(!raw){e=errno;goto failed;}
  if(fwrite(p,1,b->bytes,raw)!=b->bytes){fclose(raw);e=EIO;goto failed;}fclose(raw);
  for(size_t i=0;i<b->bytes;i++)if(((unsigned char *)p)[i]!=pattern(i,role)){e=EIO;goto failed;}
  e=ops->cpu_unmap(memory,b->id,false);if(e)goto failed;
  e=remesa_linux_memory_validate_backing(memory,b->id,b->va,b->bytes);if(e)goto failed;
  struct remesa_memory_info info;e=remesa_linux_memory_info(memory,b->id,&info);if(e)goto failed;
  printf("{\"role\":\"%s\",\"id\":%u,\"GPU_VA\":%llu,\"bytes\":%llu,\"verified_PTE_pages\":%u,\"initial_zero\":true,\"CPU_roundtrip\":true}\n",b->name,b->id,(unsigned long long)info.gpu_va,(unsigned long long)info.object_bytes,info.verified_pages);fflush(stdout);
 }
 for(unsigned i=3;i;i--){struct item *b=&items[i-1];
  e=ops->gpu_unmap(memory,b->id,b->va,b->bytes);if(e)goto failed;b->mapped=false;
  e=ops->unpin(memory,b->id);if(e)goto failed;b->pinned=false;
  e=ops->destroy(memory,b->id);if(e)goto failed;b->id=0;
 }
 {struct remesa_memory_info info;e=remesa_linux_memory_info(memory,0,&info);if(e)goto failed;
  if(info.allocated_bytes||info.objects||info.pinned_objects||info.mapped_pages||info.leases||info.held_jobs||info.quarantined){e=EPROTO;goto failed;}}
 e=remesa_linux_memory_detach(memory);if(e)goto failed;memory=NULL;
 close(fd);fclose(journal);puts("RESOURCE_GEM_PTE_ROUNDTRIP_PASS CPU-written data; GPU_submissions=0");return 0;
failed:
 fprintf(stderr,"RESOURCE_PROBE_BLOCKED errno=%d; no retry or forced retirement\n",e);
 /* No job/dispatch API is called. FD death invokes the service's existing
  * per-file lease cleanup; uncertain mappings remain subject to quarantine.
  * Do not force object cleanup after a possibly partial operation. */
 close(fd);if(memory)remesa_linux_memory_forget(memory);fclose(journal);return 10;
}
