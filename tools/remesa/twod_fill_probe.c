/* SPDX-License-Identifier: MIT */
/* Qualification client of the typed native fill endpoint. Source patterns/oracles are CPU data; only the native
 * ioctl may perform the tested copy. No software copy/presentation fallback. */
#define _POSIX_C_SOURCE 200809L
#include "remesa/memory_uapi.h"
#include "remesa/twod_fill_uapi.h"
#include "remesa_linux_memory.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
static struct remesa_linux_memory *transport;
static uint64_t copy_wall_ns,copy_min_ns=UINT64_MAX,copy_max_ns;
static unsigned copy_operations;
static int transport_error;
static uint64_t now_ns(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))abort();return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static uint64_t context;
static int memory(int fd,struct remesa_memory_request *r)
{r->version=1;r->context=context;return ioctl(fd,DRM_IOCTL_REMESA_MEMORY,r);}
static int action(int fd,unsigned op,unsigned id)
{struct remesa_memory_request r={.action=op,.id=id};return memory(fd,&r);}
static unsigned char pattern(uint64_t offset,int source)
{return (unsigned char)((offset*37u)^(offset>>8)^(offset>>19)^(source?0x5au:0xa5u));}
static int create(int fd,uint64_t bytes,uint64_t va,unsigned *id)
{
 const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();int error;
 (void)fd;
 error=ops->create(transport,(size_t)bytes,id);
 if(!error)error=ops->pin(transport,*id);
 if(!error)error=ops->gpu_map(transport,*id,va,(size_t)bytes);
 if(error){errno=error;return -1;}return 0;
}
static int transfer(int fd,unsigned id,uint64_t bytes,int source,int write,
 unsigned width,unsigned height,unsigned stride,FILE *capture,uint64_t *checked)
{
 struct remesa_memory_request r={.action=REMESA_MEM_LEASE,.id=id,.flags=write?1:0};
 int result=-1,saved=0;
 if(memory(fd,&r))return -1;
 for(uint64_t offset=0;offset<bytes;offset+=4096){
  unsigned length=bytes-offset<4096?(unsigned)(bytes-offset):4096;
  r=(struct remesa_memory_request){.action=write?REMESA_MEM_WRITE:REMESA_MEM_READ,.id=id,.offset=offset,.length=length};
  if(write)for(unsigned j=0;j<length;j++)r.data[j]=pattern(offset+j,source);
  if(memory(fd,&r))goto done;
  if(!write){
   if(fwrite(r.data,1,length,capture)!=length)goto done;
   for(unsigned j=0;j<length;j++){
    uint64_t at=offset+j,row=at/stride,col=at%stride;
    int copied=row>=1&&row<=(uint64_t)height&&col>=4&&col<((uint64_t)width+1)*4;
    unsigned char expected=(!source&&copied)?(unsigned char)(0x7f123456u>>(8*(at%4))):pattern(at,source);
    if(r.data[j]!=expected){fprintf(stderr,"readback mismatch %s offset=%"PRIu64" actual=%u expected=%u\n",source?"source":"destination",at,r.data[j],expected);errno=EILSEQ;goto done;}
    (*checked)++;
   }
  }
 }
 result=0;
 done:saved=errno;if(action(fd,REMESA_MEM_UNLEASE,id))return -1;errno=saved;return result;
}
int main(int argc,char **argv)
{
 _Static_assert(sizeof(struct remesa_twod_fill_request)==104,"fixed native fill ABI");
 if(argc==2&&!strcmp(argv[1],"--describe")){
  printf("{\"scope\":\"ABI description only; no device opened\",\"request_bytes\":%zu,\"ioctl\":%lu}\n",sizeof(struct remesa_twod_fill_request),(unsigned long)DRM_IOCTL_REMESA_TWOD_FILL);return 0;
 }
 if(argc!=6||strcmp(argv[1],"--run")){
  fprintf(stderr,"usage: %s --describe | --run /dev/dri/card0 WIDTH HEIGHT NEW_CAPTURE_PREFIX\n",argv[0]);return 2;
 }
 char *end;unsigned long w=strtoul(argv[3],&end,10);if(*end||!w||w>4093||w==8)return 2;
 unsigned long h=strtoul(argv[4],&end,10);if(*end||!h||h>4093||h==8)return 2;
 unsigned width=(unsigned)w,height=(unsigned)h,stride=(width+2)*4;
 uint64_t bytes=(((uint64_t)stride*(height+2)+4095)&~4095ULL)+4096,checked[2]={0,0};
 /* Kept within the proven nonoverlapping two-BO test addresses. */
 if(bytes>0x00fff000)return 2;
 char source_name[4096],destination_name[4096];
 if(snprintf(source_name,sizeof source_name,"%s-source.bin",argv[5])>=(int)sizeof source_name||
    snprintf(destination_name,sizeof destination_name,"%s-destination.bin",argv[5])>=(int)sizeof destination_name)return 2;
 FILE *capture[2]={fopen(source_name,"wbx"),NULL};
 if(!capture[0]){perror("exclusive source capture");return 1;}
 capture[1]=fopen(destination_name,"wbx");if(!capture[1]){perror("exclusive destination capture");fclose(capture[0]);return 1;}
 int fd=open(argv[2],O_RDWR|O_CLOEXEC),result=1,uncertain=0,cleanup=0;unsigned ids[2]={0,0};
 struct remesa_twod_fill_request copy={0};
 if(fd<0){perror("DRM open");goto done;}
 int begin_error=remesa_linux_memory_begin(fd,&transport);
 if(begin_error){errno=begin_error;perror("GEM endpoint open");goto done;}
 context=remesa_linux_memory_context(transport);
 if(create(fd,bytes,0x40001000,&ids[0])||create(fd,bytes,0x41000000,&ids[1])){perror("GEM create/pin/map");goto done;}
 for(unsigned i=0;i<2;i++)if(transfer(fd,ids[i],bytes,!i,1,width,height,stride,NULL,&checked[i])){perror("source/guard upload");goto done;}
 copy=(struct remesa_twod_fill_request){
  .destination={ids[1],width+2,height+2,stride},
  .destination_x=1,.destination_y=1,.width=width,.height=height,.color=0x7f123456,.budget_ms=1000};
 struct remesa_twod_fill_request input=copy;
 for(unsigned iteration=0;iteration<16;iteration++){
  copy=input;uncertain=1;
  copy.version=1;copy.context=context;
  uint64_t start=now_ns();int rc=ioctl(fd,DRM_IOCTL_REMESA_TWOD_FILL,&copy);transport_error=rc<0?errno:rc;uint64_t elapsed=now_ns()-start;
  copy_wall_ns+=elapsed;if(elapsed<copy_min_ns)copy_min_ns=elapsed;if(elapsed>copy_max_ns)copy_max_ns=elapsed;
  if(transport_error){errno=transport_error;perror("native fill endpoint; outcome uncertain");goto done;}
  if(copy.error||copy.retained||copy.stages!=REMESA_TWOD_FILL_ALL||!copy.operation||!copy.cookie||
     copy.core!=0x01130000||copy.revision!=0x00010201){fprintf(stderr,"native copy failed/uncertain\n");goto done;}
  copy_operations++;
 }
 uncertain=0;
 for(unsigned i=0;i<2;i++)if(transfer(fd,ids[i],bytes,!i,0,width,height,stride,capture[i],&checked[i])){perror("physical readback/guards");goto done;}
 result=0;
 done:
 if(fd>=0&&context&&!uncertain){cleanup=action(fd,REMESA_MEM_CLOSE,0);if(cleanup){perror("GEM close");result=1;}}
 remesa_linux_memory_forget(transport);transport=NULL;
 if(fd>=0&&close(fd)){perror("DRM close");result=1;}
 for(unsigned i=0;i<2;i++)if(fclose(capture[i])){perror("capture close");result=1;}
 printf("{\"status\":\"%s\",\"width\":%u,\"height\":%u,\"stride\":%u,\"bo_bytes\":%"PRIu64",\"source_bytes_checked\":%"PRIu64",\"destination_bytes_checked\":%"PRIu64",\"operation\":%llu,\"cookie\":%llu,\"core\":%u,\"revision\":%u,\"stages\":%u,\"error\":%d,\"retained\":%u,\"uncertain\":%d,\"cleanup_error\":%d,\"copy_ioctl_wall_ns\":%llu,\"transport_error\":%d,\"copy_operations\":%u,\"copy_min_ns\":%llu,\"copy_max_ns\":%llu}\n",
  result?"FAIL":"NATIVE_FILL_RETIRED_AND_FULL_READBACK_GUARDS_PASS",width,height,stride,bytes,checked[0],checked[1],
  (unsigned long long)copy.operation,(unsigned long long)copy.cookie,copy.core,copy.revision,copy.stages,copy.error,copy.retained,uncertain,cleanup,
  (unsigned long long)copy_wall_ns,transport_error,copy_operations,(unsigned long long)copy_min_ns,(unsigned long long)copy_max_ns);
 return result;
}
