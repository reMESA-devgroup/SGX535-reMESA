/* SPDX-License-Identifier: MIT */
/* External physical qualification, never installed in the rendering loop.
 * CPU uploads/reference checks are not rendering. Only the native Gallium clear callbacks
 * may produce the tested rectangle. No CPU or alternative submission fallback. */
#define _POSIX_C_SOURCE 200809L
#include "remesa_screen.h"
#include "remesa_loader.h"
#include "remesa_linux_memory.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
static struct remesa_twod_fill_request last_copy;
static unsigned native_ioctls,memory_ioctls,creates,leases,native_copies,session_closes;
static int owned_fd=-1;
static struct remesa_twod_copy_request copy_receipt;
int __real_ioctl(int,unsigned long,...);
int __wrap_ioctl(int,unsigned long,...);
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list ap;va_start(ap,command);void *arg=va_arg(ap,void *);va_end(ap);
 if(command==DRM_IOCTL_REMESA_MEMORY){struct remesa_memory_request *r=arg;
  if(r->action==REMESA_MEM_OPEN)owned_fd=fd;
  session_closes+=r->action==REMESA_MEM_CLOSE;
  memory_ioctls++;creates+=r->action==REMESA_MEM_CREATE;leases+=r->action==REMESA_MEM_LEASE;}
 int rc=__real_ioctl(fd,command,arg);
 if(command==DRM_IOCTL_REMESA_TWOD_FILL){native_ioctls++;last_copy=*(struct remesa_twod_fill_request *)arg;}
 if(command==DRM_IOCTL_REMESA_TWOD_COPY){native_copies++;copy_receipt=*(struct remesa_twod_copy_request *)arg;}
 return rc;
}
static uint64_t now(void)
{struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))abort();return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static unsigned char pattern(size_t at,int source)
{return (unsigned char)((at*37u)^(at>>8)^(at>>19)^(source?0x5au:0xa5u));}
static void drop_resource(struct pipe_resource *r)
{if(r&&!--r->reference.count)r->screen->resource_destroy(r->screen,r);}
int main(int argc,char **argv)
{
 if(argc==2&&!strcmp(argv[1],"--describe")){
  printf("{\"request_bytes\":%zu,\"frontend\":\"Gallium clear_texture/clear_render_target/clear\",\"operations\":49,\"GPU_operations\":0}\n",sizeof last_copy);return 0;
 }
 if(argc!=6||strcmp(argv[1],"--run"))return 2;
 char *end;unsigned long w=strtoul(argv[3],&end,10);if(*end)return 2;
 unsigned long h=strtoul(argv[4],&end,10);if(*end)return 2;
 if(!((w==1280&&h==800)||(w==640&&h==640)||(w==641&&h==479)))return 2;
 unsigned width=w,height=h,stride=(width+2)*4;
 struct pipe_box all={.width=width+2,.height=height+4,.depth=1};
 struct pipe_box box={.x=1,.y=1,.width=width,.height=height,.depth=1};
 int fd=-1,error=0,cleanup=0,result=1,uncertain=0;
 struct remesa_native_2d_report factory={0};
 struct pipe_screen *screen=NULL;struct pipe_context *c=NULL;
 struct pipe_resource *images[2]={0};struct pipe_surface *surface=NULL;FILE *files[2]={0};
 uint64_t checked[2]={0},wall=0,minimum=UINT64_MAX,maximum=0,upload=0,readback=0,phase_wall[3]={0},native_copy_wall=0;
 unsigned operations=0,copy_memory_ioctls=0,copy_leases=0,copy_creates=0;uint64_t cpu_copies=0;
 for(unsigned i=0;i<2;i++){char name[4096];
  if(snprintf(name,sizeof name,"%s-%s.bin",argv[5],i?"destination":"source")>=(int)sizeof name)goto done;
  files[i]=fopen(name,"wbx");if(!files[i])goto done;
 }
 fd=open(argv[2],O_RDWR|O_CLOEXEC);if(fd<0)goto done;
 fprintf(stderr,"session_open_start_ns=%"PRIu64"\n",now());
 screen=remesa_loader_native_2d_screen(fd,&factory);
 if(!screen){error=factory.error;uncertain=factory.retained!=NULL;goto done;}
 fprintf(stderr,"session_open_end_ns=%"PRIu64"\n",now());
 if(owned_fd<0||owned_fd==fd||!(fcntl(owned_fd,F_GETFD)&FD_CLOEXEC)){error=EPROTO;goto done;}
 if(close(fd)){fd=-1;error=EOWNERDEAD;goto done;}fd=-1; /* Original descriptor is independent. */
 c=screen->context_create(screen,NULL,0);if(!c)goto done;
 struct pipe_resource shape={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .width0=width+2,.height0=height+4,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 uint64_t start=now();
 for(unsigned i=0;i<2;i++){
  images[i]=screen->resource_create(screen,&shape);if(!images[i])goto done;
  struct pipe_transfer *t=NULL;unsigned char *p=c->texture_map(c,images[i],0,PIPE_MAP_WRITE,&all,&t);
  if(!p||t->stride!=stride||t->layer_stride!=(size_t)stride*(height+4))goto done;
  for(size_t at=0;at<t->layer_stride;at++)p[at]=pattern(at,!i);
  c->texture_unmap(c,t);error=remesa_context_error(c);if(error)goto done;
 }
 upload=now()-start;
 struct pipe_surface tmpl={.format=shape.format};surface=c->create_surface(c,images[1],&tmpl);if(!surface)goto done;
 struct pipe_framebuffer_state fb={.width=width+2,.height=height+4,.nr_cbufs=1,.cbufs={{.texture=images[1],.format=shape.format}}};
 c->set_framebuffer_state(c,&fb);error=remesa_context_error(c);if(error)goto done;
 const unsigned char pixel[4]={0x56,0x34,0x12,0x7f};
 const union pipe_color_union red={.f={1,0,0,1}},green={.f={0,1,0,1}};
 struct pipe_scissor_state scissor={.minx=1,.miny=1,.maxx=width+1,.maxy=height+1};
 unsigned memory_before_copy=memory_ioctls;
 fprintf(stderr,"native_copy_start_ns=%"PRIu64"\n",now());
 uncertain=1;start=now();c->resource_copy_region(c,images[1],0,1,1,0,images[0],0,&box);native_copy_wall=now()-start;
 error=remesa_context_error(c);if(error)goto done;
 if(native_copies!=1||copy_receipt.operation!=1||copy_receipt.stages!=31||copy_receipt.error||copy_receipt.retained||
    copy_receipt.core!=0x01130000||copy_receipt.revision!=0x00010201||!copy_receipt.cookie||memory_ioctls!=memory_before_copy||
    remesa_context_native_copy_count(c)!=1||remesa_context_cpu_copy_count(c)){error=EPROTO;goto done;}
 uncertain=0;
 fprintf(stderr,"native_copy_retired_ns=%"PRIu64"\n",now());
 start=now();
 for(unsigned i=0;i<2;i++){
  struct pipe_transfer *t=NULL;unsigned char *p=c->texture_map(c,images[i],0,PIPE_MAP_READ,&all,&t);
  if(!p||t->stride!=stride||t->layer_stride!=(size_t)stride*(height+4))goto done;
  for(size_t at=0;at<t->layer_stride;at++){
   size_t row=at/stride,col=at%stride;
   int copied=row>=1&&row<=height&&col>=4&&col<(size_t)(width+1)*4;
   unsigned char expected=pattern(at,!i||copied);
   if(p[at]!=expected){
    fprintf(stderr,"logical guard/readback mismatch image=%u offset=%zu\n",i,at);
    c->texture_unmap(c,t);error=EILSEQ;goto done;
   }checked[i]++;
  }
  if(fwrite(p,1,t->layer_stride,files[i])!=t->layer_stride){c->texture_unmap(c,t);goto done;}
  c->texture_unmap(c,t);error=remesa_context_error(c);if(error)goto done;
 }
 readback+=now()-start;
 for(unsigned phase=0;phase<3;phase++){
 fprintf(stderr,"fill_phase%u_start_ns=%"PRIu64"\n",phase,now());
 unsigned memory_before=memory_ioctls,leases_before=leases,creates_before=creates;
 for(unsigned iteration=0;iteration<16;iteration++){
  uncertain=1;start=now();
  if(!phase)c->clear_texture(c,images[1],0,&box,pixel);
  else if(phase==1)c->clear_render_target(c,surface,&red,1,1,width,height,false);
  else c->clear(c,PIPE_CLEAR_COLOR0,&scissor,&green,0,0);
  uint64_t elapsed=now()-start;wall+=elapsed;phase_wall[phase]+=elapsed;if(elapsed<minimum)minimum=elapsed;if(elapsed>maximum)maximum=elapsed;
  error=remesa_context_error(c);if(error)goto done;
  if(last_copy.stages!=31||last_copy.error||last_copy.retained||
     last_copy.core!=0x01130000||last_copy.revision!=0x00010201||
     last_copy.operation!=phase*16+iteration+2||!last_copy.cookie||native_ioctls!=phase*16+iteration+1)goto done;
  operations++;
 }
 uncertain=0;copy_memory_ioctls+=memory_ioctls-memory_before;copy_leases+=leases-leases_before;copy_creates+=creates-creates_before;
 cpu_copies=remesa_context_cpu_clear_count(c);
 if(cpu_copies||copy_memory_ioctls||copy_leases||copy_creates||remesa_context_native_fill_count(c)!=(phase+1)*16)goto done;
 start=now();
 for(unsigned i=0;i<2;i++){
  struct pipe_transfer *t=NULL;unsigned char *p=c->texture_map(c,images[i],0,PIPE_MAP_READ,&all,&t);
  if(!p||t->stride!=stride||t->layer_stride!=(size_t)stride*(height+4))goto done;
  for(size_t at=0;at<t->layer_stride;at++){
   size_t row=at/stride,col=at%stride;
   int copied=row>=1&&row<=height&&col>=4&&col<(size_t)(width+1)*4;
   uint32_t color=phase==0?0x7f123456u:phase==1?0xffff0000u:0xff00ff00u;
   unsigned char expected=i&&copied?(unsigned char)(color>>(8*(at%4))):pattern(at,!i);
   if(p[at]!=expected){
    fprintf(stderr,"logical guard/readback mismatch image=%u offset=%zu\n",i,at);
    c->texture_unmap(c,t);error=EILSEQ;goto done;
   }checked[i]++;
  }
  if(fwrite(p,1,t->layer_stride,files[i])!=t->layer_stride){c->texture_unmap(c,t);goto done;}
  c->texture_unmap(c,t);error=remesa_context_error(c);if(error)goto done;
 }
 readback+=now()-start;
 fprintf(stderr,"fill_phase%u_readback_done_ns=%"PRIu64"\n",phase,now());
 }
 result=0;
done:
 fprintf(stderr,"screen_close_start_ns=%"PRIu64"\n",now());
 if(screen)screen->destroy(screen); /* Children must keep the duplicate alive. */
 if(screen&&c&&fcntl(owned_fd,F_GETFD)<0){cleanup=EOWNERDEAD;result=1;}
 if(c){struct pipe_framebuffer_state empty={0};c->set_framebuffer_state(c,&empty);if(surface&&!--surface->reference.count)c->surface_destroy(c,surface);c->destroy(c);}
 drop_resource(images[0]);drop_resource(images[1]);
 int closed=owned_fd>=0&&fcntl(owned_fd,F_GETFD)<0&&errno==EBADF;
 if(screen&&(!closed||session_closes!=1)){cleanup=EBUSY;result=1;} /* No forced close on retained ownership. */
 fprintf(stderr,"screen_close_end_ns=%"PRIu64"\n",now());
 if(fd>=0&&close(fd))result=1;
 for(unsigned i=0;i<2;i++)if(files[i]&&fclose(files[i]))result=1;
 printf("{\"status\":\"%s\",\"width\":%u,\"height\":%u,\"stride\":%u,\"logical_bytes\":%zu,\"source_bytes_checked\":%"PRIu64",\"destination_bytes_checked\":%"PRIu64",\"copy_operations\":%u,\"native_ioctls\":%u,\"cpu_copies\":%"PRIu64",\"copy_memory_ioctls\":%u,\"copy_leases\":%u,\"copy_creates\":%u,\"operation\":%llu,\"cookie\":%llu,\"core\":%u,\"revision\":%u,\"stages\":%u,\"error\":%d,\"retained\":%u,\"uncertain\":%d,\"transport_error\":%d,\"cleanup_error\":%d,\"copy_ioctl_wall_ns\":%llu,\"copy_min_ns\":%llu,\"copy_max_ns\":%llu,\"phase0_wall_ns\":%llu,\"phase1_wall_ns\":%llu,\"phase2_wall_ns\":%llu,\"native_copy_ioctls\":%u,\"native_copy_wall_ns\":%llu,\"session_closes\":%u,\"owned_fd_closed\":%d,\"upload_wall_ns\":%llu,\"readback_check_capture_wall_ns\":%llu}\n",
  result?"FAIL":"NATIVE_2D_OWNED_FACTORY_COPY_FILL_GUARDS_PASS",width,height,stride,(size_t)stride*(height+4),checked[0],checked[1],operations,native_ioctls,cpu_copies,copy_memory_ioctls,copy_leases,copy_creates,
  (unsigned long long)last_copy.operation,(unsigned long long)last_copy.cookie,last_copy.core,last_copy.revision,last_copy.stages,last_copy.error,last_copy.retained,uncertain,error,cleanup,
  (unsigned long long)wall,(unsigned long long)minimum,(unsigned long long)maximum,(unsigned long long)phase_wall[0],(unsigned long long)phase_wall[1],(unsigned long long)phase_wall[2],native_copies,(unsigned long long)native_copy_wall,session_closes,closed,(unsigned long long)upload,(unsigned long long)readback);
 return result;
}
