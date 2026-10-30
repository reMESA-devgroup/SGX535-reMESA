/* SPDX-License-Identifier: MIT
 * Bounded C transport LAB collector, not a renderer or performance frontend.
 * No module/control/boot changes, source rearm, CPU rendering or pixel oracle.
 */
#define _POSIX_C_SOURCE 200809L
#include "remesa_linux_memory.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct object {uint32_t id;uint64_t va;size_t bytes;bool pinned,mapped;};
static double now(void)
{struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return t.tv_sec+t.tv_nsec/1e9;}
static int path(char out[4096],const char *directory,const char *name)
{int n=snprintf(out,4096,"%s/%s",directory,name);return n<0||n>=4096?ENAMETOOLONG:0;}
static int load(const char *directory,const char *name,void *data,size_t bytes)
{
 char p[4096];int e=path(p,directory,name);if(e)return e;
 FILE *f=fopen(p,"rb");if(!f)return errno;
 bool good=fread(data,1,bytes,f)==bytes&&fgetc(f)==EOF;int closed=fclose(f);
 return good&&!closed?0:EIO;
}
static int save(const char *directory,const char *name,const void *data,size_t bytes)
{
 char p[4096];int e=path(p,directory,name);if(e)return e;
 int fd=open(p,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(fd<0)return errno;
 const unsigned char *pbytes=data;size_t at=0;
 while(at<bytes){ssize_t n=write(fd,pbytes+at,bytes-at);if(n<=0){int result=n<0?errno:EIO;close(fd);return result;}at+=(size_t)n;}
 return close(fd)?errno:0;
}
static uint32_t le32(const unsigned char *b)
{return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static uint64_t le64(const unsigned char *b)
{return le32(b)|(uint64_t)le32(b+4)<<32;}
static int profile(const char *assets,struct object objects[13])
{
 unsigned char raw[5592];int e=load(assets,"stream-profile.bin",raw,sizeof(raw));if(e)return e;
 if(le32(raw)!=0x32505352||le32(raw+4)!=2||!le32(raw+8)||le32(raw+8)>64||le32(raw+12))return EINVAL;
 uint64_t total=0;
 for(unsigned i=0;i<11;i++){
  const unsigned char *p=raw+208+i*24;uint64_t va=le64(p+8),bytes=le64(p+16);
  if(le32(p+4)||!va||va%4096||va>=UINT64_C(0x100000000)||!bytes||bytes%4096||
     bytes>128u*1024u*1024u||bytes>UINT64_C(0x100000000)-va)return EINVAL;
  objects[i].va=va;objects[i].bytes=(size_t)bytes;total+=bytes;
 }
 if(objects[0].bytes!=131072||objects[5].bytes!=4096000||objects[10].bytes!=4096000)return EINVAL;
 if(objects[5].va<4096)return EINVAL;
 objects[11]=(struct object){.va=objects[5].va-4096,.bytes=4096};
 objects[12]=(struct object){.va=objects[9].va+objects[9].bytes,.bytes=4096};total+=8192;
 if(total>128u*1024u*1024u)return EINVAL;
 for(unsigned i=0;i<13;i++){
  if(objects[i].va+objects[i].bytes>UINT64_C(0x100000000))return EINVAL;
  for(unsigned j=0;j<i;j++)if(objects[i].va<objects[j].va+objects[j].bytes&&objects[j].va<objects[i].va+objects[i].bytes)return EINVAL;
 }
 return 0;
}
static struct remesa_private_session_lab_request request(unsigned action,uint64_t context,uint64_t session,uint64_t seq,const uint32_t *ids)
{
 struct remesa_private_session_lab_request r={.version=1,.action=action,.context=context,.session=session,.sequence=seq};
 if(action==1){memcpy(r.ids,ids,sizeof(r.ids));r.max_jobs=3;}
 if(action==2)r.budget_ms=10000;
 return r;
}
static int snapshot(struct remesa_linux_memory *m,const struct object *o,unsigned frame,const char *output)
{
 struct remesa_memory_info infos[14];int e=remesa_linux_memory_info(m,0,&infos[13]);if(e)return e;
 if(infos[13].leases||infos[13].held_jobs||infos[13].quarantined)return EBUSY;
 for(unsigned i=0;i<13;i++){
  e=remesa_linux_memory_info(m,o[i].id,&infos[i]);if(e)return e;
  if(infos[i].gpu_va!=o[i].va||infos[i].object_bytes!=o[i].bytes||infos[i].object_pinned!=1||
     infos[i].object_lease||infos[i].leases||infos[i].held_jobs||infos[i].quarantined||
     infos[i].object_mapped_pages!=o[i].bytes/4096||infos[i].verified_pages!=o[i].bytes/4096)return EPROTO;
 }
 char name[80];snprintf(name,sizeof(name),"frame-%06u-INFO.bin",frame);return save(output,name,infos,sizeof(infos));
}
#define TRY(expression) do {e=(expression);if(e)goto done;} while(0)
int main(int argc,char **argv)
{
 if(argc!=6||strcmp(argv[1],"--execute")){
  fprintf(stderr,"LAB ONLY: %s --execute DEVICE ASSETS NEW_OUTPUT NEXT_PDS_DIRECTORY\n",argv[0]);return 2;
 }
 const char *device=argv[2],*assets=argv[3],*output=argv[4],*next=argv[5],*stage="profile";
 struct object objects[13]={0};struct remesa_linux_memory *m=NULL;
 const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();
 unsigned char *images[5]={0},*next_pds=NULL,*changed=NULL,guards[4096];
 uint32_t ids[11]={0};int fd=-1,e=0,cleanup_error=0;bool attempted=false,closed=false,clean=false;
 unsigned frames=0,pages=0;double prepare=0,opened=0,submit[3]={0},upload[3]={0},readback[3]={0};
 uint64_t session=0;struct remesa_private_session_lab_request r={0};char name[80];double start=0;
 if(mkdir(output,0700)){perror("new output directory");return 1;}
 TRY(profile(assets,objects));
 for(unsigned i=0;i<5;i++){
  images[i]=malloc(objects[i].bytes);if(!images[i]){e=ENOMEM;goto done;}
  snprintf(name,sizeof(name),"role%u.bin",i);TRY(load(assets,name,images[i],objects[i].bytes));
 }
 next_pds=malloc(objects[0].bytes);changed=calloc(objects[0].bytes/4096,1);
 if(!next_pds||!changed){e=ENOMEM;goto done;}TRY(load(next,"role0.bin",next_pds,objects[0].bytes));
 /* Plan dirty pages once, before opening the device. No full-buffer compare
  * or diagnostic hash is inserted into the submission transport. */
 for(size_t at=0;at<objects[0].bytes;at+=4096)if(memcmp(images[0]+at,next_pds+at,4096)){changed[at/4096]=1;pages++;}
 if(!pages){e=EINVAL;goto done;}memset(guards,0xa5,sizeof(guards));
 stage="memory-open";fd=open(device,O_RDWR|O_CLOEXEC);if(fd<0){e=errno;goto done;}
 struct stat st;if(fstat(fd,&st)||!S_ISCHR(st.st_mode)){e=ENODEV;goto done;}
 TRY(remesa_linux_memory_begin(fd,&m));stage="prepare";start=now();
 for(unsigned i=0;i<13;i++){
  TRY(ops->create(m,objects[i].bytes,&objects[i].id));
  TRY(ops->pin(m,objects[i].id));objects[i].pinned=true;
  TRY(ops->gpu_map(m,objects[i].id,objects[i].va,objects[i].bytes));objects[i].mapped=true;
  if(i<5||i>=11){
   TRY(remesa_linux_memory_upload_pages(m,objects[i].id,0,i<5?images[i]:guards,objects[i].bytes));
   TRY(ops->sync_for_device(m,objects[i].id));
  }
  if(i<11)ids[i]=objects[i].id;
 }
 prepare=now()-start;stage="session-open";
 r=request(1,remesa_linux_memory_context(m),0,0,ids);TRY(save(output,"OPEN-REQUEST.bin",&r,sizeof(r)));
 attempted=true;start=now();e=remesa_linux_memory_private_lab_open(m,ids,3,&r);opened=now()-start;
 {int saved=save(output,"OPEN-RESPONSE.bin",&r,sizeof(r));if(!e)e=saved;}if(e)goto done;session=r.session;
 for(unsigned frame=1;frame<=3;frame++){
  const unsigned char *pds=frame==2?next_pds:images[0];stage="PDS-upload";start=now();
  if(frame>1)for(size_t at=0;at<objects[0].bytes;at+=4096)if(changed[at/4096])
   TRY(remesa_linux_memory_upload_pages(m,ids[0],at,pds+at,4096));
  upload[frame-1]=now()-start;stage="submit";
  r=request(2,remesa_linux_memory_context(m),session,frame,NULL);
  snprintf(name,sizeof(name),"frame-%06u-REQUEST.bin",frame);TRY(save(output,name,&r,sizeof(r)));
  start=now();e=remesa_linux_memory_private_lab_submit(m,10000,&r);submit[frame-1]=now()-start;
  snprintf(name,sizeof(name),"frame-%06u-RESPONSE.bin",frame);
  {int saved=save(output,name,&r,sizeof(r));if(!e)e=saved;}if(e)goto done;
  stage="LAB-readback";start=now();TRY(snapshot(m,objects,frame,output));
  for(unsigned i=0;i<13;i++)if(i<6||i==10||i>=11){
   void *cpu=NULL;TRY(ops->cpu_map(m,objects[i].id,false,&cpu));
   snprintf(name,sizeof(name),"frame-%06u-role%u.bin",frame,i);TRY(save(output,name,cpu,objects[i].bytes));
   if(i<5&&memcmp(cpu,i?images[i]:pds,objects[i].bytes)){e=EILSEQ;goto done;}
   if(i>=11&&memcmp(cpu,guards,sizeof(guards))){e=EILSEQ;goto done;}
   TRY(ops->cpu_unmap(m,objects[i].id,false));
  }
  readback[frame-1]=now()-start;frames=frame;
 }
 stage="session-close";r=request(3,remesa_linux_memory_context(m),session,3,NULL);
 TRY(save(output,"CLOSE-REQUEST.bin",&r,sizeof(r)));e=remesa_linux_memory_private_lab_close(m,&r);
 {int saved=save(output,"CLOSE-RESPONSE.bin",&r,sizeof(r));if(!e)e=saved;}if(e)goto done;closed=true;
 done:
 if(m&&(!attempted||closed)){
  for(unsigned i=13;i-->0;)if(objects[i].id){
   if(objects[i].mapped){cleanup_error=ops->gpu_unmap(m,objects[i].id,objects[i].va,objects[i].bytes);if(cleanup_error)break;}
   if(objects[i].pinned){cleanup_error=ops->unpin(m,objects[i].id);if(cleanup_error)break;}
   cleanup_error=ops->destroy(m,objects[i].id);if(cleanup_error)break;objects[i].id=0;
  }
  if(!cleanup_error){cleanup_error=remesa_linux_memory_detach(m);if(!cleanup_error){m=NULL;clean=true;}}
 }
 if(m)remesa_linux_memory_forget(m);
 if(fd>=0)close(fd);
 for(unsigned i=0;i<5;i++)free(images[i]);
 free(next_pds);free(changed);
 char result[2048];int n=snprintf(result,sizeof(result),"{\"status\":\"%s\",\"stage\":\"%s\",\"error\":%d,\"cleanup_error\":%d,\"memory_cleanup\":\"%s\",\"frames_completed\":%u,\"session_closed\":%s,\"changed_PDS_pages\":%u,\"prepare_seconds\":%.9f,\"open_seconds\":%.9f,\"submit_seconds\":[%.9f,%.9f,%.9f],\"PDS_upload_seconds\":[%.9f,%.9f,%.9f],\"LAB_readback_seconds\":[%.9f,%.9f,%.9f],\"hardware_driver_qualified\":false,\"scope\":\"C transport LAB collector, pixel oracle pending, not FPS\"}\n",
  !e&&!cleanup_error&&closed&&clean?"C_SESSION_COLLECTED_ORACLE_PENDING":"FAILED",stage,e,cleanup_error,clean?"complete":"retained",frames,closed?"true":"false",pages,prepare,opened,submit[0],submit[1],submit[2],upload[0],upload[1],upload[2],readback[0],readback[1],readback[2]);
 if(n<0||(unsigned)n>=sizeof(result))return 1;
 int saved=save(output,"RESULT.json",result,(size_t)n);fputs(result,stdout);
 return e||cleanup_error||!closed||!clean||saved?1:0;
}
