/* SPDX-License-Identifier: MIT */
/* External physical qualification, never installed in the rendering loop.
 * CPU uploads/reference checks are not rendering. Only the native Gallium clear callbacks
 * may produce the tested rectangle. No CPU or alternative submission fallback. */
#define _POSIX_C_SOURCE 200809L
#include "remesa_screen.h"
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
static unsigned native_ioctls,memory_ioctls,creates,leases;
int __real_ioctl(int,unsigned long,...);
int __wrap_ioctl(int,unsigned long,...);
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list ap;va_start(ap,command);void *arg=va_arg(ap,void *);va_end(ap);
 if(command==DRM_IOCTL_REMESA_MEMORY){struct remesa_memory_request *r=arg;
  memory_ioctls++;creates+=r->action==REMESA_MEM_CREATE;leases+=r->action==REMESA_MEM_LEASE;}
 int rc=__real_ioctl(fd,command,arg);
 if(command==DRM_IOCTL_REMESA_TWOD_FILL){native_ioctls++;last_copy=*(struct remesa_twod_fill_request *)arg;}
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
  printf("{\"request_bytes\":%zu,\"frontend\":\"Gallium clear_texture/clear_render_target/clear\",\"operations\":48,\"GPU_operations\":0}\n",sizeof last_copy);return 0;
 }
 if(argc!=6||strcmp(argv[1],"--run"))return 2;
 char *end;unsigned long w=strtoul(argv[3],&end,10);if(*end)return 2;
 unsigned long h=strtoul(argv[4],&end,10);if(*end)return 2;
 if(!((w==1280&&h==800)||(w==640&&h==640)||(w==641&&h==479)))return 2;
 unsigned width=w,height=h,stride=(width+2)*4;
 struct pipe_box all={.width=width+2,.height=height+4,.depth=1};
 struct pipe_box box={.x=1,.y=1,.width=width,.height=height,.depth=1};
 int fd=-1,error=0,cleanup=0,result=1,uncertain=0;
 struct remesa_linux_memory *transport=NULL;struct remesa_device device={0};
 struct pipe_screen *screen=NULL;struct pipe_context *c=NULL;
 struct pipe_resource *images[2]={0};struct pipe_surface *surface=NULL;FILE *files[2]={0};
 uint64_t checked[2]={0},wall=0,minimum=UINT64_MAX,maximum=0,upload=0,readback=0,phase_wall[3]={0};
 unsigned operations=0,copy_memory_ioctls=0,copy_leases=0,copy_creates=0;uint64_t cpu_copies=0;
 for(unsigned i=0;i<2;i++){char name[4096];
  if(snprintf(name,sizeof name,"%s-%s.bin",argv[5],i?"destination":"source")>=(int)sizeof name)goto done;
  files[i]=fopen(name,"wbx");if(!files[i])goto done;
 }
 fd=open(argv[2],O_RDWR|O_CLOEXEC);if(fd<0)goto done;
 error=remesa_linux_memory_begin(fd,&transport);if(error)goto done;
 struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=transport,
  .context=remesa_linux_memory_context(transport),.va_start=0x40001000,.va_end=0x42000000,
  .byte_budget=32u*1024u*1024u,.resource_only_backend=true,
  .resource_limits={4096,4096,4,16u*1024u*1024u}};
 error=remesa_persistent_open(&device,&cfg);if(error)goto done;
 error=remesa_persistent_set_native_fill(&device,remesa_linux_memory_native_fill);if(error)goto done;
 screen=remesa_screen_create(&device);if(!screen)goto done;
 error=remesa_screen_enable_native_fill(screen,remesa_persistent_fill_region);if(error)goto done;
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
 for(unsigned phase=0;phase<3;phase++){
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
     last_copy.operation!=phase*16+iteration+1||!last_copy.cookie||native_ioctls!=phase*16+iteration+1)goto done;
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
 }
 result=0;
done:
 if(c){struct pipe_framebuffer_state empty={0};c->set_framebuffer_state(c,&empty);if(surface&&!--surface->reference.count)c->surface_destroy(c,surface);c->destroy(c);}
 drop_resource(images[0]);drop_resource(images[1]);if(screen)screen->destroy(screen);
 if(device.priv){cleanup=remesa_persistent_close(&device);if(cleanup)result=1;}
 if(transport&&!uncertain&&!cleanup){cleanup=remesa_linux_memory_detach(transport);if(!cleanup)transport=NULL;else result=1;}
 if(transport)remesa_linux_memory_forget(transport); /* host metadata only, never retirement */
 if(fd>=0&&close(fd))result=1;
 for(unsigned i=0;i<2;i++)if(files[i]&&fclose(files[i]))result=1;
 printf("{\"status\":\"%s\",\"width\":%u,\"height\":%u,\"stride\":%u,\"logical_bytes\":%zu,\"source_bytes_checked\":%"PRIu64",\"destination_bytes_checked\":%"PRIu64",\"copy_operations\":%u,\"native_ioctls\":%u,\"cpu_copies\":%"PRIu64",\"copy_memory_ioctls\":%u,\"copy_leases\":%u,\"copy_creates\":%u,\"operation\":%llu,\"cookie\":%llu,\"core\":%u,\"revision\":%u,\"stages\":%u,\"error\":%d,\"retained\":%u,\"uncertain\":%d,\"transport_error\":%d,\"cleanup_error\":%d,\"copy_ioctl_wall_ns\":%llu,\"copy_min_ns\":%llu,\"copy_max_ns\":%llu,\"phase0_wall_ns\":%llu,\"phase1_wall_ns\":%llu,\"phase2_wall_ns\":%llu,\"upload_wall_ns\":%llu,\"readback_check_capture_wall_ns\":%llu}\n",
  result?"FAIL":"GALLIUM_NATIVE_FILL_THREE_APIS_RETIRED_GUARDS_PASS",width,height,stride,(size_t)stride*(height+4),checked[0],checked[1],operations,native_ioctls,cpu_copies,copy_memory_ioctls,copy_leases,copy_creates,
  (unsigned long long)last_copy.operation,(unsigned long long)last_copy.cookie,last_copy.core,last_copy.revision,last_copy.stages,last_copy.error,last_copy.retained,uncertain,error,cleanup,
  (unsigned long long)wall,(unsigned long long)minimum,(unsigned long long)maximum,(unsigned long long)phase_wall[0],(unsigned long long)phase_wall[1],(unsigned long long)phase_wall[2],(unsigned long long)upload,(unsigned long long)readback);
 return result;
}
