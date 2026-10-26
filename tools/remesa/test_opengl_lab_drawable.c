/* SPDX-License-Identifier: MIT
 * Real driver resources and frontend API; publication is an injected mock.
 * This does not create GLX, render pixels or submit commands.
 */
int inherited_driver_fixture(void);
#define main inherited_driver_fixture
#include "../../mesa/sgx535/tests/test_driver_nir.c"
#undef main
#include "opengl_lab_drawable.h"
#include "state_tracker/st_context.h"
#include "remesa_resource_ref.h"
struct publisher { unsigned calls; int error; struct pipe_resource *source; };
static int publish(void *priv,struct pipe_context *ctx,struct pipe_resource *source)
{
 struct publisher *p=priv;assert(ctx&&source&&ctx->screen==source->screen);
 p->calls++;p->source=source;return p->error;
}
#ifdef REMESA_DRAWABLE_REAL_GL
#include "GL/gl.h"
static struct pipe_rasterizer_state observed_raster;
static unsigned observed_raster_calls;
static void *(*real_create_raster)(struct pipe_context *,const struct pipe_rasterizer_state *);
static void *trace_raster(struct pipe_context *ctx,const struct pipe_rasterizer_state *r)
{
 observed_raster=*r;observed_raster_calls++;
 void *result=real_create_raster(ctx,r);
 if(!result)fprintf(stderr,"default raster rejected: half_pixel=%u bottom_edge=%u clamp_vertex=%u clamp_fragment=%u flat=%u cull=%u fill=%u/%u depth_clip=%u/%u scissor=%u discard=%u offset=%u multisample=%u\n",
  r->half_pixel_center,r->bottom_edge_rule,r->clamp_vertex_color,r->clamp_fragment_color,
  r->flatshade,r->cull_face,r->fill_front,r->fill_back,r->depth_clip_near,r->depth_clip_far,
  r->scissor,r->rasterizer_discard,r->offset_tri,r->multisample);
 return result;
}
static int get_param(struct pipe_frontend_screen *screen,enum st_manager_param param)
{ (void)screen;(void)param;return 0; }
static void real_context(struct pipe_screen *screen,struct recorder *rec)
{
 struct pipe_frontend_screen frontend={.screen=screen,.get_param=get_param};
 struct st_context_attribs attrs={.profile=API_OPENGL_COMPAT,.major=1,.minor=0,
  .visual={.buffer_mask=ST_ATTACHMENT_FRONT_LEFT_MASK|ST_ATTACHMENT_BACK_LEFT_MASK,
   .color_format=PIPE_FORMAT_B8G8R8A8_UNORM,.depth_stencil_format=PIPE_FORMAT_NONE}};
 enum st_context_error error=ST_CONTEXT_SUCCESS;
 struct st_context *st=st_api_create_context(&frontend,&attrs,&error,NULL);assert(st);
 struct publisher p={0};struct remesa_lab_drawable *d=NULL;
 assert(!remesa_lab_drawable_create(&frontend,10,32,32,true,publish,&p,&d));
 struct pipe_frontend_drawable *base=remesa_lab_drawable_base(d);
 assert(st_api_make_current(st,base,base));
 typedef void (GLAPIENTRY *enum_fn)(GLenum);
 typedef void (GLAPIENTRY *clear_color_fn)(GLfloat,GLfloat,GLfloat,GLfloat);
 typedef void (GLAPIENTRY *read_fn)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void *);
 typedef GLenum (GLAPIENTRY *error_fn)(void);
 enum_fn draw=(enum_fn)_mesa_glapi_get_proc_address("glDrawBuffer");
 enum_fn read=(enum_fn)_mesa_glapi_get_proc_address("glReadBuffer");
 enum_fn clear=(enum_fn)_mesa_glapi_get_proc_address("glClear");
 clear_color_fn color=(clear_color_fn)_mesa_glapi_get_proc_address("glClearColor");
 read_fn pixels=(read_fn)_mesa_glapi_get_proc_address("glReadPixels");
 error_fn gl_error=(error_fn)_mesa_glapi_get_proc_address("glGetError");
 assert(draw&&read&&clear&&color&&pixels&&gl_error);
 for(unsigned frame=0;frame<2;frame++) {
  draw(GL_BACK);color(frame?1:0,frame?0:1,frame?1:0,1);clear(GL_COLOR_BUFFER_BIT);
  assert(gl_error()==GL_NO_ERROR&&!remesa_context_error(st->pipe));
  read(GL_BACK);unsigned char data[4096];pixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,data);
  assert(gl_error()==GL_NO_ERROR);
  for(unsigned i=0;i<1024;i++) {
   assert(data[i*4]==(frame?255:0)&&data[i*4+1]==(frame?0:255));
   assert(data[i*4+2]==(frame?255:0)&&data[i*4+3]==255);
  }
  assert(base->flush_swapbuffers(st,base));
  read(GL_FRONT);memset(data,0,sizeof(data));pixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,data);
  assert(gl_error()==GL_NO_ERROR);
  for(unsigned i=0;i<1024;i++)assert(data[i*4]==(frame?255:0)&&data[i*4+1]==(frame?0:255));
 }
 assert(p.calls==2);
 /* A new default BACK allocation is undefined, so this first draw does not
  * request preservation of either CPU-cleared frame. No pixel is fabricated:
  * the inherited backend only records commands and explicit mock retirement. */
 assert(!remesa_lab_drawable_resize(d,32,32));
 enum st_attachment_type both[]={ST_ATTACHMENT_FRONT_LEFT,ST_ATTACHMENT_BACK_LEFT};
 struct pipe_resource *fresh[2]={NULL,NULL};
 assert(base->validate(st,base,both,2,fresh,NULL));
 typedef GLuint (GLAPIENTRY *create_fn)(GLenum);
 typedef void (GLAPIENTRY *source_fn)(GLuint,GLsizei,const GLchar *const *,const GLint *);
 typedef void (GLAPIENTRY *object_fn)(GLuint);
 typedef void (GLAPIENTRY *getiv_fn)(GLuint,GLenum,GLint *);
 typedef void (GLAPIENTRY *log_fn)(GLuint,GLsizei,GLsizei *,GLchar *);
 typedef GLuint (GLAPIENTRY *program_fn)(void);
 typedef void (GLAPIENTRY *attach_fn)(GLuint,GLuint);
 typedef GLint (GLAPIENTRY *location_fn)(GLuint,const GLchar *);
 typedef void (GLAPIENTRY *gen_fn)(GLsizei,GLuint *);
 typedef void (GLAPIENTRY *bind_fn)(GLenum,GLuint);
 typedef void (GLAPIENTRY *data_fn)(GLenum,ptrdiff_t,const void *,GLenum);
 typedef void (GLAPIENTRY *pointer_fn)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void *);
 typedef void (GLAPIENTRY *viewport_fn)(GLint,GLint,GLsizei,GLsizei);
 typedef void (GLAPIENTRY *arrays_fn)(GLenum,GLint,GLsizei);
 typedef void (GLAPIENTRY *delete_buffers_fn)(GLsizei,const GLuint *);
 typedef void (GLAPIENTRY *flush_fn)(void);
 #define PROC(type,name) type name=(type)_mesa_glapi_get_proc_address("gl" #name);assert(name)
 PROC(create_fn,CreateShader);PROC(source_fn,ShaderSource);PROC(object_fn,CompileShader);
 PROC(getiv_fn,GetShaderiv);PROC(log_fn,GetShaderInfoLog);PROC(object_fn,DeleteShader);
 PROC(program_fn,CreateProgram);PROC(attach_fn,AttachShader);PROC(object_fn,LinkProgram);
 PROC(getiv_fn,GetProgramiv);PROC(log_fn,GetProgramInfoLog);PROC(object_fn,UseProgram);
 PROC(object_fn,DeleteProgram);PROC(location_fn,GetAttribLocation);PROC(gen_fn,GenBuffers);
 PROC(bind_fn,BindBuffer);PROC(data_fn,BufferData);PROC(pointer_fn,VertexAttribPointer);
 PROC(object_fn,EnableVertexAttribArray);PROC(object_fn,DisableVertexAttribArray);
 PROC(enum_fn,Disable);PROC(viewport_fn,Viewport);PROC(arrays_fn,DrawArrays);
 PROC(delete_buffers_fn,DeleteBuffers);PROC(flush_fn,Flush);
 #undef PROC
 const GLchar *texts[2]={
  "#version 100\nattribute vec4 position;void main(){gl_Position=position;}\n",
  "#version 100\nprecision highp float;void main(){gl_FragColor=vec4(0.0,1.0,0.0,1.0);}\n"};
 GLuint shaders[2]={CreateShader(GL_VERTEX_SHADER),CreateShader(GL_FRAGMENT_SHADER)};
 for(unsigned i=0;i<2;i++) {
  assert(shaders[i]);ShaderSource(shaders[i],1,&texts[i],NULL);CompileShader(shaders[i]);
  GLint compiled=0;GetShaderiv(shaders[i],GL_COMPILE_STATUS,&compiled);
  if(!compiled){GLchar log[1024];GetShaderInfoLog(shaders[i],sizeof(log),NULL,log);fprintf(stderr,"shader %u: %s\n",i,log);}
  assert(compiled);
 }
 GLuint program=CreateProgram();assert(program);
 AttachShader(program,shaders[0]);AttachShader(program,shaders[1]);LinkProgram(program);
 GLint linked=0;GetProgramiv(program,GL_LINK_STATUS,&linked);
 if(!linked){GLchar log[1024];GetProgramInfoLog(program,sizeof(log),NULL,log);fprintf(stderr,"link: %s\n",log);}
 assert(linked);UseProgram(program);
 GLint slot=GetAttribLocation(program,"position");assert(slot>=0);
 const GLfloat vertices[12]={-.5f,-.5f,0,1,.5f,-.5f,0,1,-.5f,.5f,0,1};
 GLuint vbo=0;GenBuffers(1,&vbo);assert(vbo);BindBuffer(GL_ARRAY_BUFFER,vbo);
 BufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
 VertexAttribPointer(slot,4,GL_FLOAT,GL_FALSE,16,NULL);EnableVertexAttribArray(slot);
 draw(GL_BACK);Disable(GL_DITHER);Viewport(0,0,32,32);
 assert(gl_error()==GL_NO_ERROR&&!remesa_context_error(st->pipe));
 assert(!rec->draws&&!rec->shader_prepares&&!rec->shader_submits);
 real_create_raster=st->pipe->create_rasterizer_state;st->pipe->create_rasterizer_state=trace_raster;
 DrawArrays(GL_TRIANGLES,0,3);Flush();
 GLenum draw_error=gl_error();int pipe_error=remesa_context_error(st->pipe);
 struct remesa_shader_compile_status vs={0},fs={0};
 assert(!remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_VERTEX,&vs));
 assert(!remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_FRAGMENT,&fs));
 fprintf(stderr,"default BACK draw: GL=%#x pipe=%d VS=%zu/%d FS=%zu/%d prepares=%u submits=%u draws=%u\n",
  draw_error,pipe_error,vs.binary_bytes,vs.error,fs.binary_bytes,fs.error,
  rec->shader_prepares,rec->shader_submits,rec->draws);
 /* Canonical integer vertices put half-pixel samples on the diagonal.
  * Reject before preparing shaders; never guess the hardware edge rule. */
 assert(draw_error==GL_NO_ERROR&&pipe_error==ENOTSUP);
 assert(observed_raster_calls&&observed_raster.half_pixel_center&&observed_raster.bottom_edge_rule);
 assert(vs.attempts&&fs.attempts&&!vs.error&&!fs.error);
 assert(vs.binary_bytes==0&&fs.binary_bytes==8);
 assert(!rec->shader_prepares&&!rec->shader_submits&&!rec->draws);
 const GLfloat shifted[12]={-.484375f,-.484375f,0,1,.515625f,-.484375f,0,1,-.484375f,.515625f,0,1};
 BufferData(GL_ARRAY_BUFFER,sizeof(shifted),shifted,GL_STATIC_DRAW);
 DrawArrays(GL_TRIANGLES,0,3);Flush();
 assert(gl_error()==GL_NO_ERROR&&!remesa_context_error(st->pipe));
 assert(rec->shader_prepares==1&&rec->shader_submits==1&&rec->draws==1);
 assert(rec->packet.argb==UINT32_C(0xff00ff00)&&rec->packet.index_count==3);
 struct fence *recorded=rec->last;assert(recorded&&recorded->programs&&!recorded->retired);
 assert(!remesa_shader_draw_programs_validate(&recorded->programs->snapshot));
 const struct remesa_shader_draw_inputs *inputs=&recorded->programs->inputs;
 assert(inputs->count==3&&!inputs->index_size&&!inputs->preserve_color);
 assert(inputs->viewport.scale[0]==16&&inputs->viewport.scale[1]==-16);
 size_t offset=inputs->vertex_offset+inputs->element_offset+inputs->start*inputs->vertex_stride;
 assert(offset<=inputs->vertex_bytes&&sizeof(shifted)<=inputs->vertex_bytes-offset);
 assert(!memcmp((const unsigned char *)inputs->vertex_data+offset,shifted,sizeof(shifted)));
 const float window[3][4]={{8.25f,23.75f,.5f,1},{24.25f,23.75f,.5f,1},{8.25f,7.75f,.5f,1}};
 for(unsigned v=0;v<3;v++)assert(!memcmp(rec->packet.positions[rec->packet.indices[v]],window[v],sizeof(window[v])));
 struct pipe_fence_handle *fence=NULL;st->pipe->flush(st->pipe,&fence,0);assert(fence);
 assert(!screen->fence_finish(screen,st->pipe,fence,0));
 recorded->retired=true;assert(!screen->fence_finish(screen,st->pipe,fence,0));
 recorded->target->published=true;assert(screen->fence_finish(screen,st->pipe,fence,0));
 screen->fence_reference(screen,&fence,NULL);
 int32_t stamp=base->stamp;assert(base->flush_swapbuffers(st,base));
 assert(p.calls==3&&p.source==fresh[1]&&base->stamp==stamp+1);
 for(unsigned i=0;i<2;i++)remesa_resource_reference(&fresh[i],NULL);
 st->pipe->create_rasterizer_state=real_create_raster;
 UseProgram(0);DisableVertexAttribArray(slot);BindBuffer(GL_ARRAY_BUFFER,0);
 DeleteBuffers(1,&vbo);DeleteProgram(program);
 for(unsigned i=0;i<2;i++)DeleteShader(shaders[i]);
 assert(gl_error()==GL_NO_ERROR&&!remesa_context_error(st->pipe));
 puts("PASS actual Mesa default BACK draw: tied geometry rejected before prepare; edge-independent bound GLSL/NIR -> Gallium recording draw1, explicit mock retirement/publication and acknowledged swap; NO GPU/pixel generation");
 st_api_make_current(NULL,NULL,NULL);st_api_destroy_drawable(base);st_destroy_context(st);
 remesa_lab_drawable_destroy(d);st_screen_destroy(&frontend);
 puts("PASS actual Mesa GL default back clear/readback, acknowledged swap/front readback, two distinct CPU clear frames; GPU submissions0");
}
#endif
int main(void)
{
 struct recorder rec={0};struct remesa_device device={.ops=&ops,.priv=&rec,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&device);assert(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);assert(ctx);
 struct st_context *st=calloc(1,sizeof(*st));assert(st);st->pipe=ctx;
 struct pipe_frontend_screen frontend={.screen=screen};struct publisher p={0};
 struct remesa_lab_drawable *d=NULL;
 assert(remesa_lab_drawable_create(&frontend,1,1280,800,true,publish,&p,&d)==ENOTSUP&&!d);
 assert(!remesa_lab_drawable_create(&frontend,1,32,32,true,publish,&p,&d));
 struct pipe_frontend_drawable *base=remesa_lab_drawable_base(d);
 assert(base->visual->depth_stencil_format==PIPE_FORMAT_NONE);
 enum st_attachment_type both[]={ST_ATTACHMENT_FRONT_LEFT,ST_ATTACHMENT_BACK_LEFT};
 struct pipe_resource *original[2]={NULL,NULL};
 assert(base->validate(st,base,both,2,original,NULL));
 assert(original[0]!=original[1]&&original[0]&&original[1]);
 struct pipe_resource *sentinel=(void *)(uintptr_t)1,*untouched[2]={sentinel,sentinel};
 enum st_attachment_type bad[]={ST_ATTACHMENT_FRONT_LEFT,ST_ATTACHMENT_DEPTH_STENCIL};
 assert(!base->validate(st,base,bad,2,untouched,NULL));
 assert(untouched[0]==sentinel&&untouched[1]==sentinel);
 assert(remesa_lab_drawable_error(d)==ENOTSUP);
 assert(!base->validate(st,base,both,3,untouched,NULL));
 int32_t stamp=base->stamp;p.error=EAGAIN;
 assert(!base->flush_swapbuffers(st,base)&&base->stamp==stamp&&p.source==original[1]);
 assert(remesa_lab_drawable_error(d)==EAGAIN);
 p.error=0;assert(base->flush_swapbuffers(st,base)&&base->stamp==stamp+1);
 struct pipe_resource *swapped[2]={NULL,NULL};
 assert(base->validate(st,base,both,2,swapped,NULL));
 assert(swapped[0]==original[1]&&swapped[1]==original[0]);
 assert(base->flush_front(st,base,ST_ATTACHMENT_FRONT_LEFT)&&p.source==swapped[0]);
 unsigned calls=p.calls;
 assert(!base->flush_front(st,base,ST_ATTACHMENT_BACK_LEFT)&&p.calls==calls);
 stamp=base->stamp;assert(remesa_lab_drawable_resize(d,64,32)==ENOTSUP&&base->stamp==stamp);
 assert(!remesa_lab_drawable_resize(d,32,32)&&base->stamp==stamp+1);
 struct pipe_resource *fresh[2]={NULL,NULL};assert(base->validate(st,base,both,2,fresh,NULL));
 assert(fresh[0]!=swapped[0]&&fresh[0]!=swapped[1]&&fresh[1]!=fresh[0]);
 /* Outstanding state-tracker references keep old backing alive after resize. */
 assert(original[0]->width0==32&&original[1]->height0==32);
 base->stamp=INT32_MAX;calls=p.calls;
 assert(!base->flush_swapbuffers(st,base)&&p.calls==calls);
 assert(remesa_lab_drawable_resize(d,32,32)==EOVERFLOW);
 base->stamp=1;
 for(unsigned i=0;i<2;i++) {
  remesa_resource_reference(&original[i],NULL);remesa_resource_reference(&swapped[i],NULL);
  remesa_resource_reference(&fresh[i],NULL);
 }
 remesa_lab_drawable_destroy(d);d=NULL;
 assert(!remesa_lab_drawable_create(&frontend,2,32,32,false,publish,&p,&d));
 base=remesa_lab_drawable_base(d);calls=p.calls;
 assert(!base->flush_swapbuffers(st,base)&&p.calls==calls);
 assert(!base->validate(st,base,both,2,untouched,NULL));
 assert(!base->validate(NULL,base,both,1,untouched,NULL));
 remesa_lab_drawable_destroy(d);free(st);ctx->destroy(ctx);
#ifdef REMESA_DRAWABLE_REAL_GL
 real_context(screen,&rec);
#endif
 screen->destroy(screen);
 assert(rec.allocs==rec.frees&&rec.shader_allocs==rec.shader_frees);
#ifdef REMESA_DRAWABLE_REAL_GL
 assert(rec.draws==1&&rec.shader_prepares==1&&rec.shader_submits==1&&rec.fence_destroys==1);
#else
 assert(!rec.draws&&!rec.shader_prepares);
#endif
 puts("PASS real-driver frontend front/back binding, ack-only swap, error rollback, stamp, resize lifetime, singlebuffer/depth/1280 rejects; GPU submissions0");
 return 0;
}
