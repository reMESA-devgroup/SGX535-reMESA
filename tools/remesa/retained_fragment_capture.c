/* SPDX-License-Identifier: MIT */
/* Evolution of the original retained_triangle_probe_main recorder. No GPU
 * command, shader payload, expected pixel, or completion is constructed here. */
#include "retained_fragment_capture.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int path(char out[4096],const char *dir,const char *name)
{int n=snprintf(out,4096,"%s/%s",dir,name);return n<0||n>=4096?ENAMETOOLONG:0;}
static int write_all(int fd,const void *data,size_t size)
{
 const unsigned char *p=data;
 while(size){ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;
  if(n<=0)return errno?errno:EIO;
  p+=n;size-=(size_t)n;}
 return fsync(fd)?errno:0;
}
int remesa_fragment_save(const char *dir,const char *name,const void *data,size_t size)
{
 char file[4096];int e=path(file,dir,name);if(e)return e;
 int fd=open(file,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd<0)return errno;
 e=write_all(fd,data,size);if(close(fd)&&!e)e=errno;return e;
}
int remesa_fragment_copy(const char *from,const char *dir,const char *name)
{
 unsigned char data[8192];size_t size=0;
 int fd=open(from,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno;
 int e=0;
 while(size<sizeof(data)) {ssize_t n=read(fd,data+size,sizeof(data)-size);
  if(n<0&&errno==EINTR)continue;
  if(n<0){e=errno;break;}if(!n)break;size+=(size_t)n;}
 unsigned char tail;if(!e&&read(fd,&tail,1)!=0)e=EFBIG;
 close(fd);return e?e:remesa_fragment_save(dir,name,data,size);
}
int remesa_fragment_load(const char *dir,const char *name,void *data,size_t size)
{
 char file[4096];int e=path(file,dir,name);if(e)return e;
 int fd=open(file,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno;
 size_t done=0;
 while(done<size) {ssize_t n=read(fd,(char *)data+done,size-done);
  if(n<0&&errno==EINTR)continue;
  if(n<=0){e=n<0?errno:EIO;break;}done+=(size_t)n;}
 unsigned char tail;if(!e&&read(fd,&tail,1)!=0)e=EFBIG;
 close(fd);return e;
}
int remesa_fragment_record_memory(void *p,const struct remesa_memory_request *a,
 const struct remesa_memory_request *b,int rc,int error,bool entering)
{
 struct remesa_fragment_recorder *c=p;
 struct {uint32_t entering;int32_t rc,error;uint32_t reserved;
  struct remesa_memory_request request,response;} r={0};
 _Static_assert(sizeof(r)==8320,"original raw memory record ABI");
 if(!c||!a||c->failed||c->sealed||c->records>=4096)return EIO;
 r.entering=entering;r.rc=rc;r.error=error;r.request=*a;if(b)r.response=*b;
 int e=write_all(c->memory_fd,&r,sizeof(r));
 if(e)c->failed=true;else c->records++;
 return e;
}
int remesa_fragment_record_retained(void *p,const struct remesa_retained_request *a,
 const struct remesa_retained_request *b,int rc,int error)
{
 struct remesa_fragment_recorder *c=p;
 struct {int32_t rc,error;struct remesa_retained_request request,response;} r={0};
 if(!c||!a||c->failed||c->sealed||c->records%2||c->retained_records>1||
    (!!b!=(c->retained_records==1)))return EIO;
 r.rc=rc;r.error=error;r.request=*a;if(b)r.response=*b;
 if(!b)c->submit_begin=c->records;else c->submit_end=c->records;
 int e=write_all(c->retained_fd,&r,sizeof(r));
 if(!e&&b)e=remesa_fragment_save(c->directory,"response.bin",b,sizeof(*b));
 if(e)c->failed=true;else c->retained_records++;
 return e;
}
static int run_auth(struct remesa_fragment_recorder *c,bool pre)
{
 pid_t child=fork();if(child<0)return errno;
 if(!child){execl("/usr/bin/python3","python3",c->authenticator,
   pre?"--admission-only":"--verify-capture","--directory",c->directory,
   "--binding",c->binding,(char *)NULL);_exit(127);}
 /* Bounded verifier process only. A timeout never retires a GPU job. */
 for(unsigned i=0;i<1200;i++) {
  int status;pid_t r=waitpid(child,&status,WNOHANG);
  if(r==child)return WIFEXITED(status)&&!WEXITSTATUS(status)?0:EPROTO;
  if(r<0&&errno!=EINTR)return EOWNERDEAD;
  struct timespec delay={0,100000000};nanosleep(&delay,NULL);
 }
 kill(child,SIGKILL);while(waitpid(child,NULL,0)<0&&errno==EINTR){}
 return ETIMEDOUT;
}
int remesa_fragment_auth_preflight(struct remesa_fragment_recorder *c)
{return !c||!c->directory||!c->binding||!c->authenticator?EINVAL:run_auth(c,true);}

static uint32_t u32(const unsigned char **p)
{uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)(*p)[i]<<(8*i);*p+=4;return v;}
static uint64_t u64(const unsigned char **p)
{uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)(*p)[i]<<(8*i);*p+=8;return v;}
static void identity(const unsigned char **p,struct remesa_evidence_id *id)
{id->context=u64(p);id->operation=u64(p);id->serial=u64(p);id->slot=u32(p);id->generation=u32(p);}
static void bytes(const unsigned char **p,void *out,size_t n){memcpy(out,*p,n);*p+=n;}
int remesa_fragment_evidence_decode(const unsigned char *raw,size_t size,
 struct remesa_linux_fragment_evidence *out)
{
 if(!raw||!out||size!=548||memcmp(raw,"RMSEVID1",8))return EINVAL;
 struct remesa_linux_fragment_evidence e={0};const unsigned char *p=raw+8;
 identity(&p,&e.expected.id);bytes(&p,e.expected.boot,16);bytes(&p,e.expected.source,32);
 bytes(&p,e.expected.request_sha256,32);e.expected.request_bytes=u64(&p);e.expected.resources=u32(&p);
 identity(&p,&e.response.id);bytes(&p,e.response.request_sha256,32);bytes(&p,e.response.response_sha256,32);
 e.response.request_bytes=u64(&p);e.response.response_bytes=u64(&p);
 e.response.ioctl_result=(int32_t)u32(&p);e.response.operation_errno=(int32_t)u32(&p);
#define R(x) e.response.x=u32(&p)
 R(records);R(attempted);R(ta_complete);R(end_render);R(memory_free);R(retired);R(visible);R(acknowledged);R(quarantined);
#undef R
 bytes(&p,e.observer.boot,16);bytes(&p,e.observer.source,32);bytes(&p,e.observer.request_sha256,32);bytes(&p,e.observer.response_sha256,32);
 e.observer.request_bytes=u64(&p);e.observer.response_bytes=u64(&p);
 e.observer.ioctl_result=(int32_t)u32(&p);e.observer.syscall_result=(int32_t)u32(&p);
#define O(x) e.observer.x=u32(&p)
 O(native_records);O(capsule_records);O(source_records);O(request_present);O(response_present);O(closed);O(syscall_exit_seen);O(invalid);O(capsule_invalid);O(source_valid);O(ta_complete);O(end_render);O(memory_free);O(retired);
#undef O
 identity(&p,&e.ownership.id);
#define W(x) e.ownership.x=u32(&p)
 W(records);W(resources);W(pfn_valid);W(mappings_valid);W(held_through_completion);W(visibility_ordered);W(detached);W(released);W(quarantined);
#undef W
 if(p!=raw+size||remesa_retained_completion_verify(&e.expected,&e.response,&e.observer,&e.ownership)!=REMESA_EVIDENCE_OK)return EPROTO;
 *out=e;return 0;
}
int remesa_fragment_authenticate(void *p,const struct remesa_retained_request *r,
 const struct remesa_linux_fragment_capture *capture,struct remesa_linux_fragment_evidence *out)
{
 struct remesa_fragment_recorder *c=p;
 if(!c||!r||!capture||!out||c->failed||c->sealed||c->retained_records!=2||
    c->submit_begin!=c->submit_end||c->records<=c->submit_end||c->records%2)return EPROTO;
 /* Freeze the exact prefix. Later verified framebuffer reads/cleanup are
  * separately journaled, never silently appended to authenticated originals. */
 c->sealed=true;
 char json[4096];int n=snprintf(json,sizeof(json),
  "{\"scope\":\"PROTECTED_CAPTURE_UNAUTHENTICATED\",\"submit_begin\":%llu,\"submit_end\":%llu,\"capture_end\":%llu,\"capture\":{\"owner\":%llu,\"generation\":%llu,\"program_handle\":%u,\"geometry_handle\":%u,\"source_offset\":%u,\"argb\":%u,\"program_bytes\":%llu,\"geometry_bytes\":%llu,\"program_changed\":%s,\"source\":\"",
  (unsigned long long)c->submit_begin,(unsigned long long)c->submit_end,(unsigned long long)c->records,
  (unsigned long long)capture->owner,(unsigned long long)capture->generation,capture->program_handle,
  capture->geometry_handle,capture->source_offset,capture->argb,(unsigned long long)capture->program_bytes,
  (unsigned long long)capture->geometry_bytes,capture->program_changed?"true":"false");
 if(n<0||(size_t)n>=sizeof(json))return EOVERFLOW;
 size_t used=(size_t)n;
#define APPEND(...) do {int k=snprintf(json+used,sizeof(json)-used,__VA_ARGS__);if(k<0||(size_t)k>=sizeof(json)-used)return EOVERFLOW;used+=(size_t)k;} while(0)
 for(unsigned i=0;i<8;i++)APPEND("%02x",capture->source[i]);
 APPEND("\",\"preimage\":\"");for(unsigned i=0;i<8;i++)APPEND("%02x",capture->preimage[i]);
 APPEND("\"");
 if(capture->vertex_staged) {
  APPEND(",\"vertex_staged\":true,\"vertex_source_handle\":%u,\"vertex_source_offset\":%u,\"vertex_use_offset\":%u,\"vertex_source\":\"",
   capture->vertex_source_handle,capture->vertex_source_offset,capture->vertex_use_offset);
  for(unsigned i=0;i<16;i++)APPEND("%02x",capture->vertex_source[i]);
  APPEND("\",\"vertex_preimage\":\"");
  for(unsigned i=0;i<16;i++)APPEND("%02x",capture->vertex_preimage[i]);
  APPEND("\"");
 }
 APPEND("},\"roles\":[");
 for(unsigned i=0;i<10;i++)APPEND("%s{\"id\":%u,\"va\":%llu,\"bytes\":%llu}",i?",":"",r->bindings[i].id,(unsigned long long)r->bindings[i].va,(unsigned long long)r->bindings[i].bytes);
 APPEND("]}\n");
#undef APPEND
 int e=remesa_fragment_save(c->directory,"capture.json",json,used);if(e)return e;
 if(fsync(c->memory_fd)||fsync(c->retained_fd))return EIO;
 if((e=remesa_fragment_copy("/proc/sgx535_native_operation",c->directory,"native.raw"))||
    (e=remesa_fragment_copy("/proc/sgx535_current_operation",c->directory,"capsule.bin"))||
    (e=remesa_fragment_copy("/proc/sgx535_source_guard",c->directory,"source-after.raw")))return e;
 e=run_auth(c,false);if(e)return e;
 unsigned char wire[548];e=remesa_fragment_load(c->directory,"verified-evidence.bin",wire,sizeof(wire));
 if(e)return e;
 struct remesa_linux_fragment_evidence decoded;
 e=remesa_fragment_evidence_decode(wire,sizeof(wire),&decoded);if(e)return e;
 const struct remesa_evidence_id *id=&decoded.expected.id;
 if(id->context!=r->context||id->operation!=r->operation||id->serial!=r->serial||
    id->slot!=r->slot||id->generation!=r->generation)return ESTALE;
 /* Continue in a different file after the sealed authentication prefix. */
 char file[4096];if(path(file,c->directory,"memory-after-auth.records"))return ENAMETOOLONG;
 int fd=open(file,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
 if(close(c->memory_fd)){close(fd);return EIO;}
 c->memory_fd=fd;c->records=0;c->sealed=false;
 *out=decoded;return 0;
}
