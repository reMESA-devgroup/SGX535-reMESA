/* SPDX-License-Identifier: MIT
 * Actual Mesa OpenGL -> native GEM client. No authored shader fixture, CPU
 * vertex/raster execution, presentation, gate enabling or automatic retry.
 * Uses the existing collector and separately registered LAB selectors5/6.
 */
#define REMESA_OPENGL_HOMOGENEOUS_NATIVE
/* COLOR/Z frontend owners. API vertex/constants use bounded host storage. */
#define REMESA_NATIVE_CLIENT_SOURCE_OWNERS 2u
#include "gears_scene_native_client.c"
#include "frontend/api.h"
#include "state_tracker/st_context.h"
#include "GL/gl.h"

struct api_bundle {
 struct pipe_frontend_screen frontend;
 struct st_context_attribs attrs;
 struct pipe_frontend_drawable drawable;
 struct pipe_resource *color,*depth;
};
struct api_report {
 uint32_t version,context_created,computed_version,glsl_version;
 uint32_t draw_called,gl_error,pipe_error,retired,retained;
 uint32_t readback_called,readback_bytes,readback_error;
 uint64_t cpu_clears;
};
static int api_param(struct pipe_frontend_screen *screen,enum st_manager_param parameter)
{(void)screen;(void)parameter;return 0;}
static bool api_front(struct st_context *st,struct pipe_frontend_drawable *d,enum st_attachment_type attachment)
{(void)st;(void)d;(void)attachment;return false;}
static bool api_validate(struct st_context *st,struct pipe_frontend_drawable *base,
 const enum st_attachment_type *attachments,unsigned count,struct pipe_resource **out,
 struct pipe_resource **resolve)
{
 (void)st;struct api_bundle *b=(struct api_bundle *)base->fscreen;
 for(unsigned i=0;i<count;i++){
  struct pipe_resource *r=attachments[i]==ST_ATTACHMENT_FRONT_LEFT?b->color:
   attachments[i]==ST_ATTACHMENT_DEPTH_STENCIL?b->depth:NULL;
  if(!r)return false;
  out[i]=NULL;remesa_resource_reference(&out[i],r);
 }
 if(resolve)*resolve=NULL;
 return true;
}
static void input_file(const char *name,void *data,size_t size)
{
 char path[4096];fixture_path(path,name);FILE *f=fopen(path,"rb");
 CHECK(f&&fread(data,1,size,f)==size&&fgetc(f)==EOF&&!fclose(f));
}
int main(int argc,char **argv)
{
 if(argc!=6||(strcmp(argv[5],"--prepare-only")&&strcmp(argv[5],"--execute"))){
  fprintf(stderr,"usage: opengl_homogeneous_native_client borrowed-fd 1280x800|640x640 existing-output-dir pinned-fixture-dir --prepare-only|--execute\n");return 2;
 }
 char *end;errno=0;long fd=strtol(argv[1],&end,10);if(errno||!*argv[1]||*end||fd<0||fd>INT_MAX)return 2;
 unsigned width,height;if(!strcmp(argv[2],"1280x800")){width=1280;height=800;}else if(!strcmp(argv[2],"640x640")){width=height=640;}else return 2;
 borrowed_fd=(int)fd;out_dir=argv[3];fixture_dir=argv[4];prepare_only=!strcmp(argv[5],"--prepare-only");timings[0]=ns();
 CHECK(!st_api_get_current());
 struct remesa_linux_memory *memory=NULL;OK(remesa_linux_memory_begin(fd,&memory));
 struct remesa_device device={0};struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=memory,
  .context=remesa_linux_memory_context(memory),.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,
  .resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
 cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 OK(remesa_persistent_open(&device,&cfg));OK(remesa_persistent_set_native_scene(&device,collect_native));
 glsl_type_singleton_init_or_ref();struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
 OK(remesa_screen_enable_linked_attachment_storage(screen));OK(remesa_screen_enable_homogeneous_frontend_lab(screen));
 struct pipe_resource *color=attachment(screen,width,height,false),*depth=attachment(screen,width,height,true);CHECK(color&&depth);
 /* Heap lifetime also covers retained/ambiguous failures: never leave Mesa
  * referring to expired frontend/drawable stack storage before process exit. */
 struct api_bundle *b=calloc(1,sizeof(*b));CHECK(b);
 b->frontend=(struct pipe_frontend_screen){.screen=screen,.get_param=api_param};b->color=color;b->depth=depth;
 b->attrs=(struct st_context_attribs){.profile=API_OPENGL_COMPAT,.major=1,.minor=0,
  .visual={.buffer_mask=ST_ATTACHMENT_FRONT_LEFT_MASK|ST_ATTACHMENT_DEPTH_STENCIL_MASK,
   .color_format=PIPE_FORMAT_B8G8R8A8_UNORM,.depth_stencil_format=PIPE_FORMAT_Z24X8_UNORM}};
 enum st_context_error context_error=ST_CONTEXT_SUCCESS;
 struct st_context *st=st_api_create_context(&b->frontend,&b->attrs,&context_error,NULL);CHECK(st);
 OK(remesa_context_enable_homogeneous_api_lab(st->pipe,REMESA_HOMOGENEOUS_API_W2_CLAMPED));
 b->drawable=(struct pipe_frontend_drawable){.stamp=1,.ID=9,.fscreen=&b->frontend,
  .visual=&b->attrs.visual,.validate=api_validate,.flush_front=api_front};
 CHECK(st_api_make_current(st,&b->drawable,&b->drawable));
 struct api_report report={.version=1,.context_created=1,.computed_version=st->ctx->Version,
  .glsl_version=st->ctx->Const.GLSLVersion};
#define PROC(ret,name,args) typedef ret (GLAPIENTRY *name##_fn) args;name##_fn name=(name##_fn)_mesa_glapi_get_proc_address(#name);CHECK(name)
 PROC(void,glMatrixMode,(GLenum));PROC(void,glLoadIdentity,(void));PROC(void,glLoadMatrixf,(const GLfloat *));
 PROC(void,glEnable,(GLenum));PROC(void,glDisable,(GLenum));PROC(void,glViewport,(GLint,GLint,GLsizei,GLsizei));
 PROC(void,glLightModelfv,(GLenum,const GLfloat *));PROC(void,glLightfv,(GLenum,GLenum,const GLfloat *));
 PROC(void,glMaterialfv,(GLenum,GLenum,const GLfloat *));PROC(void,glMaterialf,(GLenum,GLenum,GLfloat));
 PROC(void,glVertexPointer,(GLint,GLenum,GLsizei,const void *));PROC(void,glNormalPointer,(GLenum,GLsizei,const void *));
 PROC(void,glEnableClientState,(GLenum));PROC(void,glClearColor,(GLfloat,GLfloat,GLfloat,GLfloat));
 PROC(void,glClearDepth,(GLdouble));PROC(void,glClear,(GLbitfield));PROC(void,glDrawArrays,(GLenum,GLint,GLsizei));
 PROC(void,glFlush,(void));PROC(GLenum,glGetError,(void));
 PROC(void,glReadPixels,(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void *));
#undef PROC
 GLfloat vertices[3][8],coefficients[36];input_file("vertices-source.bin",vertices,sizeof vertices);
 input_file("coefficients-source.bin",coefficients,sizeof coefficients);
 const GLfloat one[4]={1,1,1,1},zero[4]={0,0,0,0};
 const GLfloat light[4]={coefficients[32],coefficients[33],coefficients[34],0};
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();glMatrixMode(GL_PROJECTION);glLoadMatrixf(coefficients+16);
 glLightModelfv(GL_LIGHT_MODEL_AMBIENT,coefficients);
 glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,one);glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,one);
 glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,one);glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,zero);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,0);
 glLightfv(GL_LIGHT0,GL_AMBIENT,coefficients+4);glLightfv(GL_LIGHT0,GL_DIFFUSE,coefficients+8);
 glLightfv(GL_LIGHT0,GL_SPECULAR,coefficients+12);glLightfv(GL_LIGHT0,GL_POSITION,light);
 glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_NORMALIZE);glEnable(GL_DEPTH_TEST);glDisable(GL_DITHER);
 glViewport(0,0,width,height);glVertexPointer(4,GL_FLOAT,sizeof vertices[0],vertices);
 glNormalPointer(GL_FLOAT,sizeof vertices[0],vertices[0]+4);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);
 glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 CHECK(glGetError()==GL_NO_ERROR&&!remesa_context_error(st->pipe));
 report.draw_called=1;glDrawArrays(GL_TRIANGLES,0,3);glFlush();
 report.gl_error=glGetError();report.pipe_error=remesa_context_error(st->pipe);report.cpu_clears=remesa_context_cpu_clear_count(st->pipe);
 int result=report.pipe_error?report.pipe_error:report.gl_error?EIO:0;
 CHECK(!report.cpu_clears);
 if(result==EOWNERDEAD){
  report.retained=1;output(out_dir,"OPENGL-REPORT.bin",&report,sizeof report);
  output(out_dir,"RESULT.bin",&result,sizeof result);output(out_dir,"TIMES.bin",timings,sizeof timings);
  fprintf(stderr,"OpenGL native outcome uncertain; no teardown/retry\n");return 1;
 }
 if(!result){
  struct pipe_fence_handle *fence=NULL;st->pipe->flush(st->pipe,&fence,0);
  CHECK(fence&&screen->fence_finish(screen,st->pipe,fence,0));screen->fence_reference(screen,&fence,NULL);
  CHECK(!remesa_resource_wait_writer(st->pipe,color,0)&&!remesa_resource_wait_writer(st->pipe,depth,0));report.retired=1;
  size_t bytes=(size_t)width*height*4;void *pixels=malloc(bytes);CHECK(pixels);memset(pixels,0xa5,bytes);
  report.readback_called=1;glReadPixels(0,0,width,height,GL_BGRA,GL_UNSIGNED_BYTE,pixels);
  report.readback_error=glGetError();report.pipe_error=remesa_context_error(st->pipe);
  if(!report.readback_error&&!report.pipe_error){report.readback_bytes=bytes;output(out_dir,"GL-READPIXELS-BGRA.bin",pixels,bytes);}
  else result=EIO;
  free(pixels);
 }
 output(out_dir,"OPENGL-REPORT.bin",&report,sizeof report);output(out_dir,"RESULT.bin",&result,sizeof result);
 output(out_dir,"TIMES.bin",timings,sizeof timings);
 if(result&&!(prepare_only&&result==EOPNOTSUPP)){fprintf(stderr,"OpenGL scene error%d; no retry\n",result);return 1;}
 st_api_make_current(NULL,NULL,NULL);st_destroy_context(st);free(b);
 struct remesa_linux_memory_inventory remaining;struct remesa_memory_info info;
 OK(remesa_linux_memory_inventory_snapshot(memory,&remaining));OK(remesa_linux_memory_info(memory,0,&info));
 CHECK(remaining.count==6&&info.objects==6&&info.pinned_objects==6&&!info.leases&&!info.held_jobs&&!info.quarantined);
 output(out_dir,"INVENTORY-REMAINING.bin",&remaining,sizeof remaining);output(out_dir,"INFO-RETURN.bin",&info,sizeof info);
 CHECK(screen->caps.graphics&&!screen->caps.accelerated);
 printf("OpenGL%u.%u native LAB draw=%u retired=%u readback=%u pipe=%u CPU_clears=%llu\n",report.computed_version/10,report.computed_version%10,
  report.draw_called,report.retired,report.readback_bytes,report.pipe_error,(unsigned long long)report.cpu_clears);
 /* Supervisor owns the borrowed fd and six remaining frontend/guard BOs. */
 return 0;
}
