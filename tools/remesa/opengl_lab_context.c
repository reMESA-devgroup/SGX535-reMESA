/* SPDX-License-Identifier: MIT
 * Real Mesa25.2 state_tracker frontend on a caller-supplied Gallium screen.
 * Isolated LAB use only. Default DRI rejection and capabilities are untouched.
 */
#include "opengl_lab_context.h"
#include "remesa_screen.h"
#include "remesa_device.h"
#include "remesa_resource_ref.h"
#include "frontend/api.h"
#include "state_tracker/st_context.h"
#include "GL/gl.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
struct lab_drawable {struct pipe_frontend_drawable base;struct pipe_resource *color;};
static int frontend_param(struct pipe_frontend_screen *f,enum st_manager_param p)
{(void)f;(void)p;return 0;}
static bool validate_drawable(struct st_context *st,struct pipe_frontend_drawable *base,
 const enum st_attachment_type *attachments,unsigned count,struct pipe_resource **out,
 struct pipe_resource **resolve)
{
 struct lab_drawable *d=(struct lab_drawable *)base;(void)st;
 for(unsigned i=0;i<count;i++) {
  if(attachments[i]!=ST_ATTACHMENT_FRONT_LEFT)return false;
  out[i]=NULL;remesa_resource_reference(&out[i],d->color);
 }
 if(resolve)*resolve=NULL;
 return true;
}
static bool flush_front(struct st_context *st,struct pipe_frontend_drawable *d,
 enum st_attachment_type a)
{(void)st;(void)d;return a==ST_ATTACHMENT_FRONT_LEFT;}
static int run_lab(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *out,bool do_clear,bool use_fbo,bool array_draw,bool quarter,void *readback,size_t capacity)
{
 if(!screen||!color||!out||color->screen!=screen||color->target!=PIPE_TEXTURE_2D||
    color->format!=PIPE_FORMAT_B8G8R8A8_UNORM||color->width0!=32||color->height0!=32||
    color->depth0!=1||color->last_level||!(color->bind&PIPE_BIND_RENDER_TARGET))return EINVAL;
 if(quarter) {
  if(!readback||capacity!=4096)return EINVAL;
  uintptr_t rb=(uintptr_t)readback,rp=(uintptr_t)out;
  if(rb>UINTPTR_MAX-capacity||rp>UINTPTR_MAX-sizeof(*out)||
     (rb<rp+sizeof(*out)&&rp<rb+capacity))return EINVAL;
 }
 /* Do not steal another thread's frontend current context. */
 if(st_api_get_current())return EBUSY;
 struct remesa_opengl_lab_report r={0};int result=0;
 struct pipe_frontend_screen frontend={.screen=screen,.get_param=frontend_param};
 struct st_context_attribs attrs={.profile=API_OPENGL_COMPAT,.major=1,.minor=0,
  .visual={.buffer_mask=ST_ATTACHMENT_FRONT_LEFT_MASK,.color_format=PIPE_FORMAT_B8G8R8A8_UNORM,
   .depth_stencil_format=PIPE_FORMAT_NONE}};
 enum st_context_error error=ST_CONTEXT_SUCCESS;
 struct st_context *st=st_api_create_context(&frontend,&attrs,&error,NULL);
 struct lab_drawable drawable={.base={.stamp=1,.ID=1,.fscreen=&frontend,
  .visual=&attrs.visual,.validate=validate_drawable,.flush_front=flush_front},.color=color};
 bool made_current=false;
 if(!st){result=ENOTSUP;goto finish;}
 r.context_created=1;r.internal_target=st->internal_target;r.computed_version=st->ctx->Version;r.glsl_version=st->ctx->Const.GLSLVersion;
 if(!st_api_make_current(st,&drawable.base,&drawable.base)){result=EPROTO;goto destroy;}
 made_current=true;
 typedef const GLubyte *(GLAPIENTRY *get_string_fn)(GLenum);
 typedef void (GLAPIENTRY *clear_color_fn)(GLclampf,GLclampf,GLclampf,GLclampf);
 typedef void (GLAPIENTRY *clear_fn)(GLbitfield);
 typedef GLenum (GLAPIENTRY *get_error_fn)(void);
 get_string_fn get_string=(get_string_fn)_mesa_glapi_get_proc_address("glGetString");
 clear_color_fn clear_color=(clear_color_fn)_mesa_glapi_get_proc_address("glClearColor");
 clear_fn api_clear=(clear_fn)_mesa_glapi_get_proc_address("glClear");
 get_error_fn get_error=(get_error_fn)_mesa_glapi_get_proc_address("glGetError");
 if(!get_string||!clear_color||!api_clear||!get_error){result=ENOTSUP;goto destroy;}
 const GLubyte *version=get_string(GL_VERSION);
 if(version)snprintf(r.version_string,sizeof(r.version_string),"%s",(const char *)version);
 if(do_clear){clear_color(0,1,0,1);api_clear(GL_COLOR_BUFFER_BIT);}
 r.gl_error=get_error();r.pipe_error=remesa_context_error(st->pipe);
 if(r.gl_error||r.pipe_error){result=EIO;goto destroy;}
 /* The no-clear quarter case draws into a new FBO below. Its dummy window
  * attachment has never been written and must not be read on the qualified
  * backend. Keep the initial readback only for the other CPU/clear probes. */
 if(!quarter) {
 struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *transfer=NULL;
 const void *pixels=st->pipe->texture_map(st->pipe,color,0,PIPE_MAP_READ,&box,&transfer);
 if(!pixels||!transfer){result=EIO;goto destroy;}
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++) {
  uint32_t pixel;memcpy(&pixel,(const unsigned char *)pixels+y*transfer->stride+x*4,4);
  r.green_pixels+=pixel==UINT32_C(0xff00ff00);
 }
 st->pipe->texture_unmap(st->pipe,transfer);
 if(do_clear&&r.green_pixels!=1024){result=EPROTO;goto destroy;}
 }
 /* A GL renderbuffer uses only render-target storage, unlike a texture
  * that would additionally request unsupported sampler binding. */
 if(use_fbo) {
  typedef void (GLAPIENTRY *gen_fbo_fn)(GLsizei,GLuint *);
  typedef void (GLAPIENTRY *bind_fbo_fn)(GLenum,GLuint);
  typedef void (GLAPIENTRY *storage_fn)(GLenum,GLenum,GLsizei,GLsizei);
  typedef void (GLAPIENTRY *attach_rb_fn)(GLenum,GLenum,GLenum,GLuint);
  typedef GLenum (GLAPIENTRY *status_fn)(GLenum);
  typedef void (GLAPIENTRY *get_rb_fn)(GLenum,GLenum,GLint *);
  gen_fbo_fn gen_fb=(gen_fbo_fn)_mesa_glapi_get_proc_address("glGenFramebuffers");
  gen_fbo_fn gen_rb=(gen_fbo_fn)_mesa_glapi_get_proc_address("glGenRenderbuffers");
  bind_fbo_fn bind_fb=(bind_fbo_fn)_mesa_glapi_get_proc_address("glBindFramebuffer");
  bind_fbo_fn bind_rb=(bind_fbo_fn)_mesa_glapi_get_proc_address("glBindRenderbuffer");
  storage_fn storage=(storage_fn)_mesa_glapi_get_proc_address("glRenderbufferStorage");
  attach_rb_fn attach_rb=(attach_rb_fn)_mesa_glapi_get_proc_address("glFramebufferRenderbuffer");
  status_fn status=(status_fn)_mesa_glapi_get_proc_address("glCheckFramebufferStatus");
  get_rb_fn get_rb=(get_rb_fn)_mesa_glapi_get_proc_address("glGetRenderbufferParameteriv");
  if(gen_fb&&gen_rb&&bind_fb&&bind_rb&&storage&&attach_rb&&status) {
   GLuint fb=0,rb=0;gen_fb(1,&fb);gen_rb(1,&rb);
   if(fb&&rb) {
    bind_rb(GL_RENDERBUFFER,rb);storage(GL_RENDERBUFFER,GL_RGBA8,32,32);
    bind_fb(GL_FRAMEBUFFER,fb);attach_rb(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,rb);
    r.fbo_created=1;r.fbo_status=status(GL_FRAMEBUFFER);r.fbo_gl_error=get_error();
    if(get_rb) {
     GLint width=0,height=0,format=0,samples=0;
     get_rb(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&width);
     get_rb(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&height);
     get_rb(GL_RENDERBUFFER,GL_RENDERBUFFER_INTERNAL_FORMAT,&format);
     /* Mesa's fbobject.c admits this query only with ARB_framebuffer_object
      * on desktop GL. EXT renderbuffers still have defined single-sample
      * storage from glRenderbufferStorage, but querying SAMPLES is INVALID_ENUM.
      * Preserve API errors instead of letting a later compiler probe consume
      * an unrelated pending error before the real draw. */
     r.fbo_samples_query_supported=st->ctx->Extensions.ARB_framebuffer_object;
     if(r.fbo_samples_query_supported)
      get_rb(GL_RENDERBUFFER,GL_RENDERBUFFER_SAMPLES,&samples);
     r.fbo_width=width;r.fbo_height=height;r.fbo_internal_format=format;r.fbo_samples=samples;
    }
    GLenum query_error=get_error();if(!r.fbo_gl_error)r.fbo_gl_error=query_error;
    if(r.fbo_gl_error){result=EIO;goto destroy;}
    if(r.fbo_status!=GL_FRAMEBUFFER_COMPLETE){result=ENOTSUP;goto destroy;}
   }
  }
 }
 /* Object allocation alone does not demonstrate compiler/shader support. */
 typedef GLuint (GLAPIENTRY *create_shader_fn)(GLenum);
 typedef void (GLAPIENTRY *source_fn)(GLuint,GLsizei,const GLchar *const *,const GLint *);
 typedef void (GLAPIENTRY *compile_fn)(GLuint);
 typedef void (GLAPIENTRY *getiv_fn)(GLuint,GLenum,GLint *);
 typedef void (GLAPIENTRY *getlog_fn)(GLuint,GLsizei,GLsizei *,GLchar *);
 typedef void (GLAPIENTRY *delete_fn)(GLuint);
 create_shader_fn create=(create_shader_fn)_mesa_glapi_get_proc_address("glCreateShader");
 source_fn source=(source_fn)_mesa_glapi_get_proc_address("glShaderSource");
 compile_fn compile=(compile_fn)_mesa_glapi_get_proc_address("glCompileShader");
 getiv_fn getiv=(getiv_fn)_mesa_glapi_get_proc_address("glGetShaderiv");
 getlog_fn getlog=(getlog_fn)_mesa_glapi_get_proc_address("glGetShaderInfoLog");
 delete_fn delete=(delete_fn)_mesa_glapi_get_proc_address("glDeleteShader");
 if(!quarter&&create&&source&&compile&&getiv&&getlog&&delete) {
  GLuint shader=create(GL_FRAGMENT_SHADER);r.shader_object=shader!=0;
  if(shader) {
   const GLchar *text="#version 110\nvoid main(){gl_FragColor=vec4(0.0,1.0,0.0,1.0);}\n";
   GLint compiled=0;source(shader,1,&text,NULL);compile(shader);
   getiv(shader,GL_COMPILE_STATUS,&compiled);r.shader_compiled=compiled!=0;
   getlog(shader,sizeof(r.shader_log),NULL,r.shader_log);delete(shader);
  }
  r.shader_error=get_error();
 }
 /* Probe only the compiler's advertised source-language alternative. This
  * does not request/claim an ES context or alter the computed desktop version. */
 typedef GLuint (GLAPIENTRY *create_program_fn)(void);
 typedef void (GLAPIENTRY *attach_fn)(GLuint,GLuint);
 create_program_fn create_program=(create_program_fn)_mesa_glapi_get_proc_address("glCreateProgram");
 attach_fn attach=(attach_fn)_mesa_glapi_get_proc_address("glAttachShader");
 compile_fn link=(compile_fn)_mesa_glapi_get_proc_address("glLinkProgram");
 getiv_fn get_program=(getiv_fn)_mesa_glapi_get_proc_address("glGetProgramiv");
 getlog_fn program_log=(getlog_fn)_mesa_glapi_get_proc_address("glGetProgramInfoLog");
 delete_fn delete_program=(delete_fn)_mesa_glapi_get_proc_address("glDeleteProgram");
 if(create&&source&&compile&&getiv&&getlog&&delete&&create_program&&attach&&link&&get_program&&program_log&&delete_program) {
  const GLchar *texts[2]={"#version 100\nprecision highp float;void main(){gl_FragColor=vec4(0.0,1.0,0.0,1.0);}\n",
   "#version 100\nattribute vec4 position;void main(){gl_Position=position;}\n"};
  GLuint shaders[2]={create(GL_FRAGMENT_SHADER),create(GL_VERTEX_SHADER)};
  GLint compiled[2]={0,0};
  for(unsigned i=0;i<2;i++)if(shaders[i]) {
   source(shaders[i],1,&texts[i],NULL);compile(shaders[i]);getiv(shaders[i],GL_COMPILE_STATUS,&compiled[i]);
  }
  r.es_fs_compiled=compiled[0]!=0;r.es_vs_compiled=compiled[1]!=0;
  if(compiled[0]&&compiled[1]) {
   GLuint program=create_program();
   if(program) {
    attach(program,shaders[0]);attach(program,shaders[1]);link(program);
    GLint linked=0;get_program(program,GL_LINK_STATUS,&linked);r.program_linked=linked!=0;
    program_log(program,sizeof(r.program_log),NULL,r.program_log);
    typedef void (GLAPIENTRY *use_fn)(GLuint);
    typedef GLint (GLAPIENTRY *location_fn)(GLuint,const GLchar *);
    typedef void (GLAPIENTRY *gen_fn)(GLsizei,GLuint *);
    typedef void (GLAPIENTRY *bind_fn)(GLenum,GLuint);
    typedef void (GLAPIENTRY *buffer_fn)(GLenum,ptrdiff_t,const void *,GLenum);
    typedef void (GLAPIENTRY *pointer_fn)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void *);
    typedef void (GLAPIENTRY *enable_fn)(GLuint);
    typedef void (GLAPIENTRY *draw_fn)(GLenum,GLsizei,GLenum,const void *);
    typedef void (GLAPIENTRY *arrays_fn)(GLenum,GLint,GLsizei);
    typedef void (GLAPIENTRY *disable_fn)(GLenum);
    typedef void (GLAPIENTRY *viewport_program_fn)(GLint,GLint,GLsizei,GLsizei);
    typedef void (GLAPIENTRY *flush_fn)(void);
    typedef void (GLAPIENTRY *delete_buffers_fn)(GLsizei,const GLuint *);
    use_fn use=(use_fn)_mesa_glapi_get_proc_address("glUseProgram");
    location_fn location=(location_fn)_mesa_glapi_get_proc_address("glGetAttribLocation");
    gen_fn gen=(gen_fn)_mesa_glapi_get_proc_address("glGenBuffers");
    bind_fn bind=(bind_fn)_mesa_glapi_get_proc_address("glBindBuffer");
    buffer_fn buffer=(buffer_fn)_mesa_glapi_get_proc_address("glBufferData");
    pointer_fn pointer=(pointer_fn)_mesa_glapi_get_proc_address("glVertexAttribPointer");
    enable_fn enable=(enable_fn)_mesa_glapi_get_proc_address("glEnableVertexAttribArray");
    draw_fn draw=(draw_fn)_mesa_glapi_get_proc_address("glDrawElements");
    arrays_fn arrays=(arrays_fn)_mesa_glapi_get_proc_address("glDrawArrays");
    disable_fn disable=(disable_fn)_mesa_glapi_get_proc_address("glDisable");
    viewport_program_fn viewport_program=(viewport_program_fn)_mesa_glapi_get_proc_address("glViewport");
    flush_fn flush_program=(flush_fn)_mesa_glapi_get_proc_address("glFlush");
    delete_buffers_fn delete_buffers=(delete_buffers_fn)_mesa_glapi_get_proc_address("glDeleteBuffers");
    if(linked&&use&&location&&gen&&bind&&buffer&&pointer&&enable&&draw&&arrays&&disable&&viewport_program&&flush_program&&delete_buffers) {
     GLint slot=location(program,"position");GLuint buffers[2]={0,0};gen(2,buffers);GLuint vbo=buffers[0],ibo=buffers[1];
     if(slot>=0&&vbo&&ibo) {
      const GLfloat canonical[12]={-.5f,-.5f,0,1,.5f,-.5f,0,1,-.5f,.5f,0,1};
      const GLfloat shifted[12]={-.484375f,-.484375f,0,1,.515625f,-.484375f,0,1,-.484375f,.515625f,0,1};
      const GLfloat *vertices=quarter?shifted:canonical;
      const GLushort indices[3]={0,1,2};
      use(program);bind(GL_ARRAY_BUFFER,vbo);buffer(GL_ARRAY_BUFFER,sizeof(canonical),vertices,GL_STATIC_DRAW);
      pointer(slot,4,GL_FLOAT,GL_FALSE,16,NULL);enable(slot);
      if(!array_draw){bind(GL_ELEMENT_ARRAY_BUFFER,ibo);buffer(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);}
      disable(GL_DITHER);viewport_program(0,0,32,32);
      if(array_draw)arrays(GL_TRIANGLES,0,3);
      else draw(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,NULL);
      flush_program();r.program_draw_called=1;r.program_draw_indexed=!array_draw;
      r.program_draw_gl_error=get_error();r.program_draw_pipe_error=remesa_context_error(st->pipe);
      struct remesa_shader_compile_status vs={0},fs={0};
      (void)remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_VERTEX,&vs);
      (void)remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_FRAGMENT,&fs);
      r.program_vs_attempts=vs.attempts;r.program_vs_error=vs.error;
      r.program_fs_attempts=fs.attempts;r.program_fs_error=fs.error;
      r.program_vs_bytes=vs.binary_bytes;r.program_fs_bytes=fs.binary_bytes;
      if(quarter) {
       r.readback_error=r.program_draw_pipe_error ? r.program_draw_pipe_error :
                        r.program_draw_gl_error ? EIO : ENOTSUP;
       typedef void (GLAPIENTRY *read_pixels_fn)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void *);
       read_pixels_fn read_pixels=(read_pixels_fn)_mesa_glapi_get_proc_address("glReadPixels");
       if(!r.program_draw_gl_error&&!r.program_draw_pipe_error&&read_pixels) {
        uint32_t pixels[1024];memset(pixels,0xa5,sizeof(pixels));
        read_pixels(0,0,32,32,GL_BGRA,GL_UNSIGNED_BYTE,pixels);r.readback_called=1;
        GLenum read_error=get_error();int pipe_error=remesa_context_error(st->pipe);
        r.readback_error=read_error?EIO:pipe_error;
        if(!r.readback_error) {
         for(unsigned i=0;i<1024;i++) {
          r.readback_green+=pixels[i]==UINT32_C(0xff00ff00);
          r.readback_zero+=pixels[i]==0;
          r.readback_other+=pixels[i]!=0&&pixels[i]!=UINT32_C(0xff00ff00);
         }
         memcpy(readback,pixels,sizeof(pixels));r.readback_bytes=sizeof(pixels);
        }
       }
      }
      use(0);bind(GL_ARRAY_BUFFER,0);bind(GL_ELEMENT_ARRAY_BUFFER,0);
     }
     delete_buffers(2,buffers);
    }
    delete_program(program);
   }
  } else for(unsigned i=0;i<2;i++)if(shaders[i]&&!compiled[i]) {
   getlog(shaders[i],sizeof(r.program_log),NULL,r.program_log);break;
  }
  for(unsigned i=0;i<2;i++)if(shaders[i])delete(shaders[i]);
  r.program_gl_error=get_error();
 }
 /* Fixed-function compatibility calls are tested independently of GLSL source
  * admission. The unchanged driver may reject their generated shaders/state. */
 typedef void (GLAPIENTRY *viewport_fn)(GLint,GLint,GLsizei,GLsizei);
 typedef void (GLAPIENTRY *begin_fn)(GLenum);
 typedef void (GLAPIENTRY *value4_fn)(GLfloat,GLfloat,GLfloat,GLfloat);
 typedef void (GLAPIENTRY *void_fn)(void);
 viewport_fn viewport=(viewport_fn)_mesa_glapi_get_proc_address("glViewport");
 begin_fn begin=(begin_fn)_mesa_glapi_get_proc_address("glBegin");
 value4_fn color4=(value4_fn)_mesa_glapi_get_proc_address("glColor4f");
 value4_fn vertex4=(value4_fn)_mesa_glapi_get_proc_address("glVertex4f");
 void_fn end=(void_fn)_mesa_glapi_get_proc_address("glEnd");
 void_fn flush=(void_fn)_mesa_glapi_get_proc_address("glFlush");
 if(!use_fbo&&viewport&&begin&&color4&&vertex4&&end&&flush) {
  viewport(0,0,32,32);color4(0,1,0,1);begin(GL_TRIANGLES);
  vertex4(-0.5f,-0.5f,0,1);vertex4(0.5f,-0.5f,0,1);vertex4(0,0.5f,0,1);
  end();flush();r.triangle_called=1;r.triangle_gl_error=get_error();
  r.triangle_pipe_error=remesa_context_error(st->pipe);
  struct remesa_shader_compile_status vs={0},fs={0};
  (void)remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_VERTEX,&vs);
  (void)remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_FRAGMENT,&fs);
  r.vs_attempts=vs.attempts;r.vs_error=vs.error;
  r.fs_attempts=fs.attempts;r.fs_error=fs.error;
 }
 destroy:
 if(made_current){st_api_make_current(NULL,NULL,NULL);st_api_destroy_drawable(&drawable.base);}
 st_destroy_context(st);
 /* Ask the actual frontend to enforce2.1; never override its computed limit. */
 if(!quarter) {
  attrs.major=2;attrs.minor=1;error=ST_CONTEXT_SUCCESS;
  st=st_api_create_context(&frontend,&attrs,&error,NULL);
  r.gl21_created=st!=NULL;r.gl21_error=error;r.gl21_bad_version=ST_CONTEXT_ERROR_BAD_VERSION;
  if(st)st_destroy_context(st);
 }
 finish:
 st_screen_destroy(&frontend);*out=r;return result;
}

int remesa_opengl_lab_clear(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *out)
{ return run_lab(screen,color,out,true,false,false,false,NULL,0); }
int remesa_opengl_lab_triangle(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *out)
{ return run_lab(screen,color,out,false,false,false,false,NULL,0); }
int remesa_opengl_lab_fbo_triangle(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *out)
{ return run_lab(screen,color,out,false,true,false,false,NULL,0); }
int remesa_opengl_lab_fbo_arrays(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *out)
{ return run_lab(screen,color,out,false,true,true,false,NULL,0); }
int remesa_opengl_lab_draw_arrays_quarter(struct pipe_screen *screen,struct pipe_resource *color,
 struct remesa_opengl_lab_report *out,void *readback,size_t capacity)
{ return run_lab(screen,color,out,false,true,true,true,readback,capacity); }
