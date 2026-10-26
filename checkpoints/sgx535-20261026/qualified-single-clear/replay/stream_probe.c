/* SPDX-License-Identifier: MIT
 * Explicit private backend qualification harness. NOT glxgears/Mesa/animation.
 * Never retries an ioctl or cleans uncertain issued work. NO hardware tests
 * are performed by --describe/--self-test or the build recipe. */
#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>
#include "remesa/memory_uapi.h"
#include "stream_client_profile.h"
#include <string.h>
#include "frame_update.h"
#define STREAM_IOCTL DRM_IOWR(DRM_COMMAND_BASE+5,struct remesa_stream_request)
static const uint64_t fixed_va[11]={0x20000000,0x20080000,0x20028000,0x30000000,0x46000000,0x40001000,0x407d1000,0x407ef000,0x30100000,0x427ef000,0x403e9000};
static const uint64_t fixed_bytes[11]={131072,32768,131072,4096,4096,4096000,122880,33554432,33554432,4096,4096000};
static FILE *journal;static int fd=-1;static uint64_t context;static bool possibly_issued;
static struct stream_client_profile profile;
static uint32_t ids[11];static unsigned char *assets[5];
static int save(const char *path,const void *data,size_t n){FILE *f=fopen(path,"wbx");if(!f)return errno;int e=fwrite(data,1,n,f)==n?0:EIO;if(fclose(f)&&!e)e=errno;return e;}
static int read_exact(const char *path,void *data,size_t n){FILE *f=fopen(path,"rb");if(!f)return errno;int e=fread(data,1,n,f)==n&&fgetc(f)==EOF?0:EINVAL;if(fclose(f)&&!e)e=errno;return e;}
#ifndef REMESA_STREAM_RECORD_HOOK
static int record(unsigned type,bool before,int result,int error,const void *data,size_t n){uint32_t head[]={0x32525354,type,before?0u:1u,(uint32_t)result,(uint32_t)error,n};if(!journal)return EIO;return fwrite(head,sizeof(head),1,journal)==1&&fwrite(data,n,1,journal)==1&&!fflush(journal)?0:EIO;}
#else
#define record REMESA_STREAM_RECORD_HOOK
#endif

static int call(unsigned long code,unsigned type,void *data,size_t n){int e=record(type,true,0,0,data,n);if(e)return e;errno=0;int result=ioctl(fd,code,data),saved=errno;e=record(type,false,result,saved,data,n);return e?e:(result?(saved?saved:EIO):0);}
static int memory(unsigned action,unsigned role,uint64_t offset,void *bytes,unsigned n,bool write){
 struct remesa_memory_request r={0};r.version=1;r.action=action;r.context=context;
 if(role<11)r.id=ids[role];
 if(action==REMESA_MEM_CREATE)r.bytes=fixed_bytes[role];
 if(action==REMESA_MEM_MAP)r.va=fixed_va[role];
 if(action==REMESA_MEM_LEASE)r.flags=write;
 if(action==REMESA_MEM_READ||action==REMESA_MEM_WRITE){r.offset=offset;r.length=n;if(write)memcpy(r.data,bytes,n);}
 int e=call(DRM_IOCTL_REMESA_MEMORY,2,&r,sizeof(r));if(e)return e;
 if(action==REMESA_MEM_OPEN){if(!r.context)return EPROTO;context=r.context;}
 if(action==REMESA_MEM_CREATE){if(!r.id)return EPROTO;ids[role]=r.id;}
 if(action==REMESA_MEM_READ)memcpy(bytes,r.data,n);
 return 0;
}
static int info(unsigned role,bool empty){
 struct remesa_memory_request r={0};struct remesa_memory_info v;
 r.version=1;r.action=REMESA_MEM_INFO;r.context=context;if(role<11)r.id=ids[role];
 int e=call(DRM_IOCTL_REMESA_MEMORY,2,&r,sizeof(r));if(e)return e;memcpy(&v,r.data,sizeof(v));
 if(empty)return v.objects||v.allocated_bytes||v.pinned_objects||v.mapped_pages||v.leases||v.held_jobs||v.quarantined?EPROTO:0;
 return role<11&&(v.object_bytes!=fixed_bytes[role]||v.gpu_va!=fixed_va[role]||v.object_pinned!=1||v.object_mapped_pages!=fixed_bytes[role]/4096||v.verified_pages!=fixed_bytes[role]/4096||v.object_lease||v.held_jobs||v.quarantined)?EPROTO:0;
}
static int upload(unsigned role){
 static bool pds_uploaded;int e;if(role>=5)return EINVAL;
 e=memory(REMESA_MEM_LEASE,role,0,NULL,0,true);if(e)return e;
 for(uint64_t offset=0;offset<fixed_bytes[role];offset+=4096){
  if(role==0&&pds_uploaded){bool dynamic=false;for(unsigned i=0;i<profile.count;i++)if(profile.batches[i].source_offset<offset+4096&&offset<profile.batches[i].source_offset+profile.batches[i].source_capacity)dynamic=true;if(!dynamic)continue;}
  e=memory(REMESA_MEM_WRITE,role,offset,assets[role]+offset,4096,true);if(e)return e;
 }
 e=memory(REMESA_MEM_UNLEASE,role,0,NULL,0,false);if(e)return e;
 e=memory(REMESA_MEM_SYNC,role,0,NULL,0,false);if(!e&&role==0)pds_uploaded=true;return e;
}
static int expect_sealed_ta_write_leases(unsigned completed){
 for(unsigned role=7;role<=8;role++){
  int e=memory(REMESA_MEM_LEASE,role,0,NULL,0,true);
  if(e!=EPERM){fprintf(stderr,"TA_SEAL_UNEXPECTED role=%u completed=%u error=%d; stop,noGPUretry\n",role,completed,e);return e?e:EKEYREJECTED;}
  printf("TA_SEAL_WRITE_LEASE_DENIED role=%u after_completed=%u errno=EPERM\n",role,completed);fflush(stdout);
 }
 return 0;
}
/* Explicit qualification capture only, never linked into production FAST. */
static int capture(unsigned role,const char *path){
 FILE *f=fopen(path,"wbx");if(!f)return errno;int e=memory(REMESA_MEM_LEASE,role,0,NULL,0,false);
 for(uint64_t offset=0;!e&&offset<fixed_bytes[role];offset+=4096){unsigned char block[4096];e=memory(REMESA_MEM_READ,role,offset,block,4096,false);if(!e&&fwrite(block,4096,1,f)!=1)e=EIO;}
 if(!e)e=memory(REMESA_MEM_UNLEASE,role,0,NULL,0,false);
 if(fclose(f)&&!e)e=errno;return e;
}
/* Separate physical coherence qualifier only: CPU reads TA after GPU,
 * then validates subsequent rendering; never linked into production FAST. */
static int read_ta_page(unsigned role,unsigned frame,const char *directory,bool compare_sync){
 unsigned char first[4096],after[4096];char path[4096];
 int e=memory(REMESA_MEM_LEASE,role,0,NULL,0,false);if(e)return e;
 e=memory(REMESA_MEM_READ,role,0,first,4096,false);if(e)return e;
 e=memory(REMESA_MEM_UNLEASE,role,0,NULL,0,false);if(e)return e;
 if(snprintf(path,sizeof(path),"%s/ta-role%u-after%u-lazy.bin",directory,role,frame)>=(int)sizeof(path))return ENAMETOOLONG;
 e=save(path,first,sizeof(first));if(e)return e;
 if(compare_sync){
  e=memory(REMESA_MEM_SYNC,role,0,NULL,0,false);if(e)return e;
  e=memory(REMESA_MEM_LEASE,role,0,NULL,0,false);if(e)return e;
  e=memory(REMESA_MEM_READ,role,0,after,4096,false);if(e)return e;
  e=memory(REMESA_MEM_UNLEASE,role,0,NULL,0,false);if(e)return e;
  if(memcmp(first,after,sizeof(first)))return EKEYREJECTED;
  if(snprintf(path,sizeof(path),"%s/ta-role%u-after%u-full-sync.bin",directory,role,frame)>=(int)sizeof(path))return ENAMETOOLONG;
  e=save(path,after,sizeof(after));if(e)return e;
 }
 printf("TA_COHERENCE_READ role=%u after_completed=%u bytes=4096 lazy_vs_full_sync=%u; next frame must stay exact\n",role,frame,compare_sync?1u:0u);fflush(stdout);return 0;
}
static int expect_static_seal(void){
 for(unsigned role=1;role<5;role++){int e=memory(REMESA_MEM_LEASE,role,0,NULL,0,true);if(e!=EPERM)return e?e:EKEYREJECTED;}
 puts("STATIC_SEAL_4_WRITELEASES_DENIED_AFTER_ACK1");return 0;
}
static int expect_pds_immutable_denied(const char *directory){
 unsigned char before[4096],after[4096],bad[4096];char name[4096];
 int e=memory(REMESA_MEM_LEASE,0,0,NULL,0,true);if(e)return e;
 e=memory(REMESA_MEM_READ,0,0,before,4096,false);if(e)return e;
 memcpy(bad,before,4096);bad[0]^=1;
 e=memory(REMESA_MEM_WRITE,0,0,bad,4096,true);if(e!=EKEYREJECTED)return e?e:EPROTO;
 e=memory(REMESA_MEM_READ,0,0,after,4096,false);if(e)return e;
 if(memcmp(before,after,4096))return EKEYREJECTED;
 e=memory(REMESA_MEM_UNLEASE,0,0,NULL,0,false);if(e)return e;
 if(snprintf(name,sizeof(name),"%s/negative-pds-before.bin",directory)>=(int)sizeof(name))return ENAMETOOLONG;e=save(name,before,4096);if(e)return e;
 if(snprintf(name,sizeof(name),"%s/negative-pds-after.bin",directory)>=(int)sizeof(name))return ENAMETOOLONG;e=save(name,after,4096);if(e)return e;
 puts("PDS_IMMUTABLE_WRITE_EKEYREJECTED_OLD_PAGE_INTACT_AFTER_ACK3");return 0;
}
static int stream_call(struct remesa_stream_request *r,unsigned action,uint64_t sequence){r->version=REMESA_STREAM_VERSION;r->action=action;r->sequence=sequence;return call(STREAM_IOCTL,5,r,sizeof(*r));}
/* Full PDS update files are accepted only if EVERY changed byte belongs to
 * existing typed MVP/LIGHT sources. No CODE/geometry change is ignored. */
static int apply_update(const char *directory,unsigned long frame){
 char name[4096];unsigned char *candidate=malloc(131072),*prepared=malloc(131072);
 const unsigned char *sources[64];size_t lengths[64];int e=ENOMEM;
 if(!candidate||!prepared)goto done;
 if(snprintf(name,sizeof(name),"%s/frame-%06lu-pds.bin",directory,frame)>=(int)sizeof(name)){e=ENAMETOOLONG;goto done;}
 e=read_exact(name,candidate,131072);if(e)goto done;
 if(!profile.count||profile.count>64){e=EINVAL;goto done;}
 for(unsigned i=0;i<profile.count;i++){
  uint64_t off=profile.batches[i].source_offset,n=profile.batches[i].source_capacity;
  if(off>131072||n>131072-off){e=EINVAL;goto done;}
  sources[i]=candidate+off;lengths[i]=n;
 }
 memcpy(prepared,assets[0],131072);
 e=remesa_stream_frame_update(prepared,131072,profile.batches,profile.count,sources,lengths);if(e)goto done;
 if(memcmp(prepared,candidate,131072)){e=EKEYREJECTED;goto done;}
 memcpy(assets[0],prepared,131072);e=0;
done:free(candidate);free(prepared);return e;
}
static int selftest(void){struct remesa_stream_receipt r={0};if(sizeof(r)!=240||sizeof(struct remesa_stream_request)!=656||REMESA_STREAM_VERSION!=5||REMESA_STREAM_PROOF_NOHASH!=111)return 1;for(unsigned i=48;i<240;i++)if(((unsigned char*)&r)[i])return 1;puts("CPU NOHASH ABI5/receipt240/proof111/zeroarrays selftest; GPU0");return 0;}
static int run_client(int argc,char **argv,bool offline_check){
 if(argc==2&&!strcmp(argv[1],"--describe")){printf("NOHASH_PRODUCTION_SEAL_QUALIFICATION_ONLY ABI%u ioctl5 request%zu receipt%zu profile%zu roles11 GEN131072 frame1280x800; NOT glxgears; no retry\n",REMESA_STREAM_VERSION,sizeof(struct remesa_stream_request),sizeof(struct remesa_stream_receipt),sizeof(profile));return 0;}
 if(argc==2&&!strcmp(argv[1],"--self-test"))return selftest();
 bool updates=argc==7&&!strcmp(argv[1],"--run-updates");
 if(!(argc==6&&!strcmp(argv[1],"--run"))&&!updates){fprintf(stderr,"usage: %s --run-updates /dev/dri/card0 ASSETS_DIR NEW_OUTPUT_DIR FRAMES UPDATES_DIR\n",argv[0]);return 2;}
 if(strcmp(argv[2],"/dev/dri/card0"))return 2;
 char *end;errno=0;unsigned long frames=strtoul(argv[5],&end,10);if(errno||*end||!frames||frames>100000)return 2;
 int e=0;char path[4096];struct remesa_stream_request job={0};uint64_t last_operation=0;
 if(snprintf(path,sizeof(path),"%s/stream-profile.bin",argv[3])>=(int)sizeof(path))return 2;
 e=read_exact(path,&profile,sizeof(profile));if(e)goto fail;
 if(profile.magic!=STREAM_CLIENT_MAGIC||profile.version!=STREAM_PROFILE_FORMAT_VERSION||profile.reserved||!profile.count||profile.count>64){e=EINVAL;goto fail;}
 for(unsigned i=0;i<11;i++)if(profile.bindings[i].id||profile.bindings[i].reserved||profile.bindings[i].va!=fixed_va[i]||profile.bindings[i].bytes!=fixed_bytes[i]){e=EINVAL;goto fail;}
 for(unsigned i=0;i<5;i++){assets[i]=malloc(fixed_bytes[i]);if(!assets[i]){e=ENOMEM;goto fail;}if(snprintf(path,sizeof(path),"%s/role%u.bin",argv[3],i)>=(int)sizeof(path)){e=ENAMETOOLONG;goto fail;}e=read_exact(path,assets[i],fixed_bytes[i]);if(e)goto fail;}

 if(offline_check){
  for(unsigned long frame=1;frame<=frames;frame++){e=apply_update(argv[6],frame);if(e)goto fail;}
  puts("CPU_ONLY_FULL_PDS_UPDATES_VALIDATED; no device opened; GPU0");goto fail;
 }

 if(mkdir(argv[4],0700)){e=errno;goto fail;}if(snprintf(path,sizeof(path),"%s/transcript.bin",argv[4])>=(int)sizeof(path)){e=ENAMETOOLONG;goto fail;}journal=fopen(path,"wbx");if(!journal){e=errno;goto fail;}
 fd=open(argv[2],O_RDWR|O_NOFOLLOW|O_CLOEXEC);if(fd<0){e=errno;goto fail;}struct stat st;if(fstat(fd,&st)||!S_ISCHR(st.st_mode)||major(st.st_rdev)!=226||minor(st.st_rdev)){e=ENODEV;goto fail;}
 e=memory(REMESA_MEM_OPEN,11,0,NULL,0,false);if(e)goto fail;e=info(11,true);if(e)goto fail;
 for(unsigned i=0;i<11;i++){e=memory(REMESA_MEM_CREATE,i,0,NULL,0,false);if(e)goto fail;e=memory(REMESA_MEM_PIN,i,0,NULL,0,false);if(e)goto fail;e=memory(REMESA_MEM_MAP,i,0,NULL,0,false);if(e)goto fail;e=info(i,false);if(e)goto fail;job.bindings[i]=(struct remesa_stream_binding){.id=ids[i],.va=fixed_va[i],.bytes=fixed_bytes[i]};}
 job.context=context;job.width=1280;job.height=800;memcpy(job.profile_sha256,profile.profile_sha256,32);
 {int random=open("/dev/urandom",O_RDONLY|O_CLOEXEC);if(random<0){e=errno;goto fail;}ssize_t n=read(random,&job.session,sizeof(job.session));close(random);if(n!=sizeof(job.session)||!job.session){e=EIO;goto fail;}}
 e=stream_call(&job,REMESA_STREAM_OPEN_FAST_NOHASH,0);if(e)goto fail;
 e=expect_sealed_ta_write_leases(0);if(e)goto fail;
 for(unsigned long frame=1;frame<=frames;frame++){
  if(updates){e=apply_update(argv[6],frame);if(e)goto fail;}
 
  for(unsigned i=0;i<(frame==1?5u:1u);i++){e=upload(i);if(e)goto fail;}
  e=stream_call(&job,REMESA_STREAM_BEGIN,frame);if(e)goto fail;
  for(unsigned i=0;i<profile.count;i++){job.batch=profile.batches[i];job.batch.image_generation=frame;e=stream_call(&job,REMESA_STREAM_STAGE,frame);if(e)goto fail;}
  possibly_issued=true;e=stream_call(&job,REMESA_STREAM_FINISH,frame);if(e)goto fail;
  struct remesa_stream_receipt *r=&job.receipt;
  if(r->session!=job.session||r->sequence!=frame||r->generation!=frame||r->operation<=last_operation||r->proof!=111||r->phase!=9||r->ledger!=7||r->error){e=EKEYREJECTED;goto fail;}
  for(unsigned i=48;i<240;i++)if(((unsigned char*)r)[i]){e=EKEYREJECTED;goto fail;}
  if(snprintf(path,sizeof(path),"%s/frame-%06lu-receipt.bin",argv[4],frame)>=(int)sizeof(path)){e=ENAMETOOLONG;goto fail;}e=save(path,r,sizeof(*r));if(e)goto fail;
  if(snprintf(path,sizeof(path),"%s/frame-%06lu-color.bin",argv[4],frame)>=(int)sizeof(path)){e=ENAMETOOLONG;goto fail;}e=capture(5,path);if(e)goto fail;
  if(snprintf(path,sizeof(path),"%s/frame-%06lu-z.bin",argv[4],frame)>=(int)sizeof(path)){e=ENAMETOOLONG;goto fail;}e=capture(10,path);if(e)goto fail;
  /* Inputs are GPU-read-only in this exact profile. Check every byte after
   * genuine retirement/publication, before ACK or any next-frame upload. */
  for(unsigned i=0;i<5;i++){
   if(snprintf(path,sizeof(path),"%s/frame-%06lu-input-role%u.bin",argv[4],frame,i)>=(int)sizeof(path)){e=ENAMETOOLONG;goto fail;}
   e=capture(i,path);if(e)goto fail;
  }
  last_operation=r->operation;e=stream_call(&job,REMESA_STREAM_ACK,frame);if(e)goto fail;
  if(frame==4){e=read_ta_page(7,4,argv[4],false);if(e)goto fail;}
  if(frame==5){e=read_ta_page(8,5,argv[4],true);if(e)goto fail;}
  if(frame==1){e=expect_static_seal();if(e)goto fail;}
  if(frame==3){e=expect_sealed_ta_write_leases(3);if(e)goto fail;e=expect_pds_immutable_denied(argv[4]);if(e)goto fail;}
  printf("frame=%lu operation=%" PRIu64 " retired9 ledger7 NOHASH_PROOF111_RAW_CAPTURED_SEPARATE_QUALIFICATION_ACKed; qualification harness only\n",frame,last_operation);fflush(stdout);
 }
 e=stream_call(&job,REMESA_STREAM_CLOSE,frames+1);if(e)goto fail;
 for(unsigned i=11;i;i--){unsigned role=i-1;e=memory(REMESA_MEM_UNMAP,role,0,NULL,0,false);if(e)goto fail;e=memory(REMESA_MEM_UNPIN,role,0,NULL,0,false);if(e)goto fail;e=memory(REMESA_MEM_DESTROY,role,0,NULL,0,false);if(e)goto fail;}
 e=info(11,true);if(e)goto fail;e=memory(REMESA_MEM_CLOSE,11,0,NULL,0,false);if(e)goto fail;
 puts("STREAM_BACKEND_HARNESS_COMPLETE; source/profile/pixel qualification remains separate; NOT glxgears");
fail:if(e)fprintf(stderr,"STREAM_STOP errno=%d possibly_issued=%u; no ioctl retry, no forced cleanup\n",e,possibly_issued);if(fd>=0)close(fd);if(journal)fclose(journal);for(unsigned i=0;i<5;i++)free(assets[i]);return e?10:0;
}

int main(int argc,char **argv){
 if(argc==5&&!strcmp(argv[1],"--check-updates")){
  char *args[]={argv[0],"--run-updates","/dev/dri/card0",argv[2],"OFFLINE_UNUSED",argv[4],argv[3],NULL};
  return run_client(7,args,true);
 }
 return run_client(argc,argv,false);
}
