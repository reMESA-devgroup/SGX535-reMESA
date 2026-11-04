/* SPDX-License-Identifier: MIT
 * Real Mesa frontend on the native-resource screen. GEM/receipt hooks are
 * modeled; no capability/version override and no GPU result is fabricated.
 */
#define REMESA_LINKED_SCENE_CONTEXT_FIXTURE
#include "test_linked_scene_context.c"
#include "remesa/opengl_lab_context.h"
#include "state_tracker/st_context.h"
#include "GL/gl.h"
#include "remesa_draw.h"
#include "remesa_nir.h"
#include "util/u_upload_mgr.h"

static struct pipe_context *(*original_create)(struct pipe_screen *,void *,unsigned);
static unsigned enabled_contexts;
static bool automatic_context;
static unsigned frontend_case,vertex_attempt;
static void *(*original_vs)(struct pipe_context *,const struct pipe_shader_state *);
static void *(*original_raster)(struct pipe_context *,const struct pipe_rasterizer_state *);
static void (*original_clear)(struct pipe_context *,unsigned,const struct pipe_scissor_state *,const union pipe_color_union *,double,unsigned);
static unsigned observed_clear_mask;
static void *observed_raster;
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
static uint64_t api_va[64];
static unsigned api_fault;
#ifdef REMESA_TEST_OPENGL_NORMAL_DRAW
static unsigned normal_fill_calls;
static int normal_fill(void *priv,const struct remesa_gpu_fill *fill)
{
 struct fixture *f=priv;CHECK(fill->color==0&&fill->destination_x==0&&fill->destination_y==0);
 CHECK(fill->width==fill->destination.width&&fill->height==fill->destination.height);
 CHECK(get(f,fill->destination.id)->length>=(size_t)fill->destination.stride*fill->height);
 normal_fill_calls++;return 0; /* Models the receipt only, never writes pixels. */
}
#endif
static int api_record_mapping(void *priv,uint32_t id,uint64_t va,size_t bytes)
{
 int error=ops.gpu_map(priv,id,va,bytes);
 CHECK(id&&id<=64);if(!error)api_va[id-1]=va;return error;
}
static enum remesa_dispatch api_native(void *priv,const struct remesa_gpu_scene_submit *submit,
 struct remesa_gpu_scene_receipt *out)
{
 const struct remesa_scene_packet *p=submit->packet;
#ifdef REMESA_TEST_OPENGL_PRESERVE_DRAW
 unsigned ordinal=dispatch_calls++;
 CHECK(ordinal<2&&remesa_scene_packet_valid(p)&&p->binding_count==18&&
  p->controls.schema==(ordinal?3:2)&&p->use_count==(ordinal?10:11)&&
  p->window_count==2&&p->controls.ta_bytes==(ordinal?76:104));
#else
 CHECK(remesa_scene_packet_valid(p)&&p->binding_count==18&&p->use_count==11);
 CHECK(!dispatch_calls++);
#endif
 struct fixture *f=priv;
 struct remesa_persistent_scene_footprint footprint={.owner=p->owner,.width=p->controls.width,
  .height=p->controls.height,.allocation_count=18};
 for(unsigned i=0;i<18;i++){
  uint32_t id=p->bindings[i].id;struct backing *b=get(f,id);
  CHECK(!b->lease&&api_va[id-1]);
  footprint.allocations[i].handle=id;footprint.allocations[i].access=p->bindings[i].access;
  footprint.allocations[i].gpu_va=api_va[id-1];footprint.allocations[i].allocation_bytes=b->length;
  if(p->bindings[i].access!=REMESA_ACCESS_READ)
   for(size_t j=0;j<b->length;j++)CHECK(b->bytes[j]==0xa5);
 }
#ifdef REMESA_TEST_OPENGL_PRESERVE_DRAW
 const char *root=getenv("REMESA_CAPTURE_EXPORT_DIR");char original[4096],variant[4096];
 if(root){
  int n=snprintf(original,sizeof original,"%s",root);CHECK(n>0&&(size_t)n<sizeof original);
  n=snprintf(variant,sizeof variant,"%s/%s",original,ordinal?"preserve":"clear");CHECK(n>0&&(size_t)n<sizeof variant);
  CHECK(!setenv("REMESA_CAPTURE_EXPORT_DIR",variant,1));
 }
 scene_capture_export(f,&footprint,p);
 if(root)CHECK(!setenv("REMESA_CAPTURE_EXPORT_DIR",original,1));
#else
 scene_capture_export(f,&footprint,p);
#endif
 *out=(struct remesa_gpu_scene_receipt){.context=submit->context,.operation=submit->operation,
  .retired=true,.published=true,.acknowledged=true};return REMESA_ISSUED;
}
#endif
static void observe_clear(struct pipe_context *ctx,unsigned mask,const struct pipe_scissor_state *scissor,
 const union pipe_color_union *color,double depth,unsigned stencil)
{
 observed_clear_mask|=mask;
 original_clear(ctx,mask,scissor,color,depth,stencil);
 printf("FIXED_CLEAR case=%u mask=%x depth=%g error=%d CPU_clears=%llu\n",frontend_case,mask,depth,
  remesa_context_error(ctx),(unsigned long long)remesa_context_cpu_clear_count(ctx));
}

static void *observe_raster(struct pipe_context *ctx,const struct pipe_rasterizer_state *state)
{
 void *result=original_raster(ctx,state);
 if(result)observed_raster=result;
 if(frontend_case>=3)printf("FIXED_RASTER case=%u accepted=%u half=%u bottom=%u clip=%u/%u cull=%u front_ccw=%u clamp=%u/%u flat=%u scissor=%u\n",
  frontend_case,result!=NULL,state->half_pixel_center,state->bottom_edge_rule,
  state->depth_clip_near,state->depth_clip_far,state->cull_face,state->front_ccw,
  state->clamp_vertex_color,state->clamp_fragment_color,state->flatshade,state->scissor);
 return result;
}
static void *observe_vertex(struct pipe_context *ctx,const struct pipe_shader_state *state)
{
 CHECK(state&&state->type==PIPE_SHADER_IR_NIR&&state->ir.nir);
 char name[80];CHECK(snprintf(name,sizeof name,"resource-vs-%u-%u.nir.txt",frontend_case,vertex_attempt++)>0);
 FILE *file=fopen(name,"wx");CHECK(file);nir_print_shader(state->ir.nir,file);CHECK(!fclose(file));
 if(frontend_case>=3){
  struct remesa_nir_gears_vertex_profile profile;
  CHECK(!remesa_nir_gears_vertex(state->ir.nir,&profile));
  CHECK(profile.normal_from_attribute==1&&profile.uniform_vectors==9);
  struct remesa_shader_ir ir;struct remesa_shader_program program;
  CHECK(!remesa_shader_ir_lower(state->ir.nir,&ir));
  CHECK(!remesa_shader_program_prepare(&ir,&program));
  unsigned char code[336];size_t written=0;
  CHECK(!remesa_shader_program_encode(&program,code,sizeof code,&written)&&written==320);
  CHECK(snprintf(name,sizeof name,"fixed-GL-CODE320-%u.bin",frontend_case)>0);
  fixture_write(".",name,code,written);remesa_shader_program_finish(&program);
 }
 return original_vs(ctx,state);
}
static void inspect_fixed_inputs(struct pipe_context *ctx,struct remesa_device *device,struct fixture *fixture)
{
 const struct remesa_pds_parameter_source_request request={0x20000000,4096,0,36,40};
 unsigned char parameters[160];size_t written=0;
 int parameter_error=remesa_context_vertex_parameter_source(ctx,&request,parameters,sizeof parameters,&written);
 char name[80];
 if(!parameter_error){CHECK(snprintf(name,sizeof name,"fixed-GL-PARAM-%u.bin",frontend_case)>0);fixture_write(".",name,parameters,written);}
 /* Inspect the bindings left by the known glDrawArrays(TRIANGLES,0,3).
  * This is a separate host snapshot, not a replacement draw or GPU proof. */
 const struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
 const struct pipe_draw_start_count_bias draw={.count=3};
 struct remesa_gears_geometry *geometry=NULL;
 int geometry_error=remesa_context_gears_geometry_snapshot(ctx,&info,&draw,&geometry);
 if(!geometry_error){
  struct remesa_gears_geometry_view view;CHECK(!remesa_gears_geometry_inspect(geometry,&view));
  CHECK(snprintf(name,sizeof name,"fixed-GL-PN-%u.bin",frontend_case)>0);fixture_write(".",name,view.vertices,view.bytes);
 }
 remesa_gears_geometry_destroy(geometry);
 unsigned char viewport[28];
 int viewport_error=remesa_context_window_viewport_encode(ctx,24,viewport);
 if(!viewport_error){CHECK(snprintf(name,sizeof name,"fixed-GL-VIEWPORT-%u.bin",frontend_case)>0);fixture_write(".",name,viewport,sizeof viewport);}
 struct remesa_isp_opaque_triangle_request isp={0};int isp_error=remesa_context_linked_isp_snapshot(ctx,&isp);
 CHECK(!isp_error&&isp.depth_bits_present&&isp.depth_test&&isp.depth_write&&
  isp.compare==REMESA_DEPTH_LESS&&isp.rgba_mask==15);
 printf("FIXED_INPUTS case=%u parameters=%d bytes=%zu geometry=%d viewport=%d ISP=%d\n",frontend_case,parameter_error,written,geometry_error,viewport_error,isp_error);
 struct remesa_shader_linkage effective;CHECK(!remesa_context_shader_linkage_snapshot(ctx,&effective));
 CHECK(!remesa_shader_ir_gears_vertex_clamped(&effective.vertex.semantic));
 struct remesa_shader_program_report report;CHECK(!remesa_shader_program_report(&effective.vertex,&report)&&report.binary_bytes==344);
 unsigned char code[352];CHECK(!remesa_shader_program_encode(&effective.vertex,code,sizeof code,&written)&&written==344);
 CHECK(snprintf(name,sizeof name,"fixed-GL-CODE344-%u.bin",frontend_case)>0);fixture_write(".",name,code,written);
 struct remesa_shader_selected_resources resources;
 CHECK(!remesa_shader_program_gears_attribute_resources(&effective.vertex,&resources));
 CHECK(resources.logical_use_bytes==344&&resources.reserved_use_bytes==352);
 /* A retained half-center descriptor is metadata, never implicit execution.
  * Exercise the pending class with all the other old guard bits zero. */
 void *api_raster=observed_raster;CHECK(api_raster);
 const struct pipe_rasterizer_state half_only={.fill_front=PIPE_POLYGON_MODE_FILL,
  .fill_back=PIPE_POLYGON_MODE_FILL,.half_pixel_center=1};
 void *pending_raster=original_raster(ctx,&half_only);CHECK(pending_raster);
 unsigned creates=fixture->creates,maps=fixture->cpu_maps;
 ctx->bind_rasterizer_state(ctx,pending_raster);ctx->draw_vbo(ctx,&info,0,NULL,&draw,1);
 CHECK(remesa_context_error(ctx)==ENOTSUP&&fixture->creates==creates&&fixture->cpu_maps==maps&&!dispatch_calls);
 ctx->bind_rasterizer_state(ctx,api_raster);ctx->delete_rasterizer_state(ctx,pending_raster);
 const struct remesa_linked_scene_request options={.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=1};
 struct remesa_scene_capture capture={0};
 /* An uploader lease must be published before native owner construction;
  * alias checks must run before releasing its mapping/staging storage. */
 struct pipe_resource *upload_resource=NULL;unsigned upload_offset=0;void *upload=NULL;
 u_upload_alloc(ctx->stream_uploader,0,4096,16,&upload_offset,&upload_resource,&upload);
 CHECK(upload_resource&&upload);memset(upload,0,4096);
 unsigned alias_creates=fixture->creates,alias_maps=fixture->cpu_maps;
 CHECK(remesa_context_prepare_gears_homogeneous_scene(ctx,&info,&draw,&options,upload)==EINVAL);
 CHECK(remesa_context_prepare_gears_homogeneous_scene(ctx,&info,&draw,upload,&capture)==EINVAL);
 unsigned mapped_views=0;
 for(unsigned i=0;i<fixture->creates;i++)if(fixture->backing[i].live&&fixture->backing[i].lease){
  CHECK(remesa_context_prepare_gears_homogeneous_scene(ctx,&info,&draw,&options,
   (void *)(fixture->backing[i].bytes+16))==EINVAL);mapped_views++;
 }
 CHECK(!mapped_views); /* Host input maps never acquire GEM leases. */
 CHECK(fixture->creates==alias_creates&&fixture->cpu_maps==alias_maps&&!dispatch_calls);
 int capture_error=remesa_context_prepare_gears_homogeneous_scene(ctx,&info,&draw,&options,&capture);
 remesa_resource_reference(&upload_resource,NULL);
 printf("FIXED_CAPTURE case=%u error=%d allocations=%u maps=%u\n",frontend_case,capture_error,fixture->creates,fixture->cpu_maps);
 fflush(stdout);
 CHECK(!capture_error);
 struct remesa_scene_packet packet;struct remesa_persistent_scene_footprint footprint;
 CHECK(!remesa_persistent_scene_packet_snapshot(device,capture,&packet));
 CHECK(!remesa_persistent_scene_capture_snapshot(device,capture,&footprint));
 CHECK(packet.binding_count==18&&packet.use_count==11&&!dispatch_calls&&!remesa_context_cpu_clear_count(ctx));
 scene_capture_export(fixture,&footprint,&packet);
 CHECK(!remesa_persistent_scene_capture_release(device,capture));
 printf("FIXED_EFFECTIVE case=%u CODE=%zu reserved=%u captured=1 submissions=%u CPU_clears=%llu\n",frontend_case,
  written,resources.reserved_use_bytes,dispatch_calls,(unsigned long long)remesa_context_cpu_clear_count(ctx));

}
static struct pipe_context *create_enabled(struct pipe_screen *screen,void *priv,unsigned flags)
{
 struct pipe_context *ctx=original_create(screen,priv,flags);CHECK(ctx);
#ifndef REMESA_TEST_OPENGL_NORMAL_DRAW
 CHECK(remesa_context_enable_homogeneous_draw_lab(ctx)==(automatic_context?EALREADY:0));
#endif
 enabled_contexts++;original_vs=ctx->create_vs_state;ctx->create_vs_state=observe_vertex;
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
 if(frontend_case>=3){
#ifndef REMESA_TEST_OPENGL_NORMAL_DRAW
  CHECK(!remesa_context_enable_homogeneous_api_lab(ctx,REMESA_HOMOGENEOUS_API_W2_CLAMPED));
  CHECK(remesa_context_enable_homogeneous_api_lab(ctx,REMESA_HOMOGENEOUS_API_W2_CLAMPED)==EALREADY);
#endif
 }
#endif
 original_raster=ctx->create_rasterizer_state;ctx->create_rasterizer_state=observe_raster;
 original_clear=ctx->clear;ctx->clear=observe_clear;observed_clear_mask=0;
 return ctx;
}

struct resource_drawable {struct pipe_frontend_drawable base;struct pipe_resource *color,*depth;};
static bool no_present(struct st_context *st,struct pipe_frontend_drawable *base,enum st_attachment_type attachment)
{(void)st;(void)base;CHECK(attachment==ST_ATTACHMENT_FRONT_LEFT);return false;}
static int frontend_param(struct pipe_frontend_screen *screen,enum st_manager_param param)
{(void)screen;(void)param;return 0;}
static bool validate_resources(struct st_context *st,struct pipe_frontend_drawable *base,
 const enum st_attachment_type *attachments,unsigned count,struct pipe_resource **out,
 struct pipe_resource **resolve)
{
 (void)st;struct resource_drawable *d=(struct resource_drawable *)base;
 for(unsigned i=0;i<count;i++){
  struct pipe_resource *r=attachments[i]==ST_ATTACHMENT_FRONT_LEFT?d->color:
   attachments[i]==ST_ATTACHMENT_DEPTH_STENCIL?d->depth:NULL;
  if(!r)return false;
  out[i]=NULL;remesa_resource_reference(&out[i],r);
 }
 if(resolve)*resolve=NULL;
 return true;
}
static void fixed_function_scene(struct pipe_screen *screen,struct pipe_resource *color,
 struct pipe_resource *depth,struct remesa_device *device,struct fixture *fixture)
{
 struct pipe_frontend_screen frontend={.screen=screen,.get_param=frontend_param};
 struct st_context_attribs attrs={.profile=API_OPENGL_COMPAT,.major=1,.minor=0,
  .visual={.buffer_mask=ST_ATTACHMENT_FRONT_LEFT_MASK|ST_ATTACHMENT_DEPTH_STENCIL_MASK,.color_format=PIPE_FORMAT_B8G8R8A8_UNORM,
   .depth_stencil_format=PIPE_FORMAT_Z24X8_UNORM}};
 enum st_context_error error=ST_CONTEXT_SUCCESS;
 struct st_context *st=st_api_create_context(&frontend,&attrs,&error,NULL);CHECK(st);
 struct resource_drawable drawable={.base={.stamp=1,.ID=9,.fscreen=&frontend,
  .visual=&attrs.visual,.validate=validate_resources},.color=color,.depth=depth};
 drawable.base.flush_front=no_present; /* Required frontend hook; no presentation. */
 CHECK(st_api_make_current(st,&drawable.base,&drawable.base));
#define REMESA_GL_PROC(ret,name,args) typedef ret (GLAPIENTRY *name##_fn)args;name##_fn name=(name##_fn)_mesa_glapi_get_proc_address(#name);CHECK(name)
 REMESA_GL_PROC(void,glEnable,(GLenum));REMESA_GL_PROC(void,glDisable,(GLenum));
 REMESA_GL_PROC(void,glMatrixMode,(GLenum));REMESA_GL_PROC(void,glLoadIdentity,(void));
 REMESA_GL_PROC(void,glLoadMatrixf,(const GLfloat *));REMESA_GL_PROC(void,glViewport,(GLint,GLint,GLsizei,GLsizei));
 REMESA_GL_PROC(void,glLightfv,(GLenum,GLenum,const GLfloat *));REMESA_GL_PROC(void,glLightModelfv,(GLenum,const GLfloat *));
 REMESA_GL_PROC(void,glMaterialfv,(GLenum,GLenum,const GLfloat *));REMESA_GL_PROC(void,glMaterialf,(GLenum,GLenum,GLfloat));
 REMESA_GL_PROC(void,glVertexPointer,(GLint,GLenum,GLsizei,const void *));
 REMESA_GL_PROC(void,glNormalPointer,(GLenum,GLsizei,const void *));
 REMESA_GL_PROC(void,glEnableClientState,(GLenum));REMESA_GL_PROC(void,glDrawArrays,(GLenum,GLint,GLsizei));
 REMESA_GL_PROC(void,glFlush,(void));REMESA_GL_PROC(GLenum,glGetError,(void));
 REMESA_GL_PROC(void,glClear,(GLbitfield));REMESA_GL_PROC(void,glClearColor,(GLfloat,GLfloat,GLfloat,GLfloat));
 REMESA_GL_PROC(void,glClearDepth,(GLdouble));
#undef REMESA_GL_PROC
 const char *witness=getenv("REMESA_BOUND_GEARS_WITNESS_DIR");
#ifndef REMESA_TEST_OPENGL_NORMAL_DRAW
 CHECK(witness);
#endif
 GLfloat coefficients[36],vertices[3][8];char path[4096];
#ifdef REMESA_TEST_OPENGL_NORMAL_DRAW
 (void)witness;(void)path;memset(coefficients,0,sizeof coefficients);
 coefficients[0]=coefficients[1]=coefficients[2]=coefficients[3]=1;
 coefficients[16]=coefficients[21]=coefficients[26]=1;coefficients[31]=2;
 coefficients[28]=.25f;coefficients[34]=1;
 /* Optional qualification input: avoid sample-on-edge ties at both sizes.
  * Changes only supplied GL matrix data, never the encoder or driver gate. */
 if(getenv("REMESA_TEST_NORMAL_Y_SHIFT"))coefficients[29]=1.0f/256.0f;
#ifdef REMESA_TEST_OPENGL_AFFINE_W1_DRAW
 if(getenv("REMESA_TEST_AFFINE_EQUIVALENT")){
  /* Homogeneous scaling keeps the independently qualified NDC oracle. */
  for(unsigned i=16;i<32;i++)coefficients[i]*=.5f;
 }else coefficients[31]=1;
#endif
#ifdef REMESA_TEST_OPENGL_PRESERVE_DRAW
 if(getenv("REMESA_TEST_PRESERVE_BASELINE_B")){
  coefficients[28]=.5f;coefficients[30]=.5f;
#ifdef REMESA_TEST_OPENGL_AFFINE_W1_DRAW
  if(getenv("REMESA_TEST_AFFINE_EQUIVALENT"))coefficients[28]=coefficients[30]=.25f;
#endif
 }
#endif
 const GLfloat dynamic[3][8]={{-.5f,-.5f,0,1,0,0,1,0},{.5f,-.5f,0,1,0,0,1,0},{-.5f,.5f,0,1,0,0,1,0}};
 memcpy(vertices,dynamic,sizeof vertices);
#else
 CHECK(snprintf(path,sizeof path,"%s/coefficients-source.bin",witness)>0);
 FILE *file=fopen(path,"rb");CHECK(file&&fread(coefficients,1,sizeof coefficients,file)==sizeof coefficients&&fgetc(file)==EOF&&!fclose(file));
 CHECK(snprintf(path,sizeof path,"%s/vertices-source.bin",witness)>0);
 file=fopen(path,"rb");CHECK(file&&fread(vertices,1,sizeof vertices,file)==sizeof vertices&&fgetc(file)==EOF&&!fclose(file));
#endif
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
 if(api_fault==1){
#ifdef REMESA_TEST_OPENGL_NORMAL_DRAW
  vertices[0][0]=2;
#else
  vertices[0][0]+=.25f;
#endif
 }
 if(api_fault==2)coefficients[16]*=2;
#endif
 const GLfloat one[4]={1,1,1,1},zero[4]={0,0,0,0};
 const GLfloat light[4]={coefficients[32],coefficients[33],coefficients[34],0};
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();glMatrixMode(GL_PROJECTION);glLoadMatrixf(coefficients+16);
 glLightModelfv(GL_LIGHT_MODEL_AMBIENT,coefficients);
 glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,one);glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,one);
 glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,one);glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,zero);
 glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,0);
 glLightfv(GL_LIGHT0,GL_AMBIENT,coefficients+4);glLightfv(GL_LIGHT0,GL_DIFFUSE,coefficients+8);
 glLightfv(GL_LIGHT0,GL_SPECULAR,coefficients+12);glLightfv(GL_LIGHT0,GL_POSITION,light);
 glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_NORMALIZE);glEnable(GL_DEPTH_TEST);glDisable(GL_DITHER);
 glViewport(0,0,color->width0,color->height0);CHECK(glGetError()==GL_NO_ERROR);
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
 if(api_fault==3)glViewport(0,0,color->width0-1,color->height0);
 if(api_fault==4)glEnable(GL_CULL_FACE);
 if(api_fault==5)glDisable(GL_DEPTH_TEST);
#endif
 glVertexPointer(4,GL_FLOAT,sizeof vertices[0],vertices);
 glNormalPointer(GL_FLOAT,sizeof vertices[0],vertices[0]+4);
 glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);
 glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 CHECK(!remesa_context_error(st->pipe)&&!remesa_context_cpu_clear_count(st->pipe));
#ifdef REMESA_TEST_OPENGL_NORMAL_DRAW
 CHECK(normal_fill_calls==1&&remesa_context_native_fill_count(st->pipe)==1);
#ifndef REMESA_TEST_OPENGL_PRESERVE_DRAW
 glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 CHECK(!remesa_context_error(st->pipe)&&normal_fill_calls==2&&remesa_context_native_fill_count(st->pipe)==2);
#endif
#endif
 CHECK(remesa_resource_wait_writer(st->pipe,color,0)==EBUSY);
 printf("FIXED_CLEAR_MASK case=%u observed=%x expected=%x depth_wait=%d\n",frontend_case,observed_clear_mask,PIPE_CLEAR_COLOR0|PIPE_CLEAR_DEPTH,remesa_resource_wait_writer(st->pipe,depth,0));
 fflush(stdout);CHECK(remesa_resource_wait_writer(st->pipe,depth,0)==EBUSY);
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
 unsigned before_draw_creates=fixture->creates;
#endif
 glDrawArrays(GL_TRIANGLES,0,3);glFlush();
 struct remesa_shader_compile_status vs={0},fs={0};
 (void)remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_VERTEX,&vs);
 (void)remesa_context_shader_compile_status(st->pipe,PIPE_SHADER_FRAGMENT,&fs);
 printf("FIXED_GL version=%u GL_error=%u VS_attempts=%llu VS_error=%d FS_attempts=%llu FS_error=%d pipe=%d submissions=%u\n",
  st->ctx->Version,glGetError(),(unsigned long long)vs.attempts,vs.error,(unsigned long long)fs.attempts,fs.error,remesa_context_error(st->pipe),dispatch_calls);
 fflush(stdout);
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
 (void)device;(void)fixture;
 if(api_fault){
  CHECK(remesa_context_error(st->pipe)==ENOTSUP&&!dispatch_calls&&fixture->creates==before_draw_creates);
  CHECK(!remesa_context_cpu_clear_count(st->pipe));
  printf("FIXED_API_REFUSAL case=%u fault=%u allocations=0 dispatches=0 GPU_operations=0\n",frontend_case,api_fault);
 }else{
 CHECK(!remesa_context_error(st->pipe)&&dispatch_calls==1&&!remesa_context_cpu_clear_count(st->pipe));
 CHECK(!remesa_resource_wait_writer(st->pipe,color,0)&&!remesa_resource_wait_writer(st->pipe,depth,0));
 struct pipe_fence_handle *fence=NULL;st->pipe->flush(st->pipe,&fence,0);
 CHECK(fence&&screen->fence_finish(screen,st->pipe,fence,0));screen->fence_reference(screen,&fence,NULL);
#ifdef REMESA_TEST_OPENGL_PRESERVE_DRAW
 /* New matrix DATA, unchanged selected shader/encoders; no second clear. */
 coefficients[28]=.5f;coefficients[30]=.5f;
#ifdef REMESA_TEST_OPENGL_AFFINE_W1_DRAW
 if(getenv("REMESA_TEST_AFFINE_EQUIVALENT"))coefficients[28]=coefficients[30]=.25f;
#endif
 glMatrixMode(GL_PROJECTION);glLoadMatrixf(coefficients+16);
 glDrawArrays(GL_TRIANGLES,0,3);glFlush();
 CHECK(!remesa_context_error(st->pipe)&&dispatch_calls==2&&normal_fill_calls==1);
 CHECK(!remesa_resource_wait_writer(st->pipe,color,0)&&!remesa_resource_wait_writer(st->pipe,depth,0));
 printf("NORMAL_API_PRESERVE case=%u clear=1 draws=2 retired=2 CPU_clears=0 GPU_operations=0\n",frontend_case);
#else
 /* A second draw cannot silently discard the first or reenter the backend. */
 glDrawArrays(GL_TRIANGLES,0,3);glFlush();
 CHECK(remesa_context_error(st->pipe)==ENOTSUP&&dispatch_calls==1);
 printf("FIXED_API_NATIVE case=%u first_draw_retired=1 second_draw_refused=1 GPU_operations=0\n",frontend_case);
#endif
 }
#else
 inspect_fixed_inputs(st->pipe,device,fixture);
 CHECK(!dispatch_calls&&!remesa_context_cpu_clear_count(st->pipe));
#endif
 st_api_make_current(NULL,NULL,NULL);st_destroy_context(st);
}

int main(void)
{
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
 const char *fault=getenv("REMESA_TEST_API_FAULT");api_fault=fault?(unsigned)strtoul(fault,NULL,10):0;CHECK(api_fault<=5);
#endif
 for(unsigned enable=
#ifdef REMESA_TEST_OPENGL_NORMAL_DRAW
  3
#else
  0
#endif
 ;enable<5;enable++) {
  frontend_case=enable;vertex_attempt=0;
  glsl_type_singleton_init_or_ref();
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
  dispatch_calls=0;memset(api_va,0,sizeof api_va);target_ops.gpu_map=api_record_mapping;
#endif
  struct remesa_device device={0};
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9600,
   .va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,
   .resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
  cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
  cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
  cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
  CHECK(!remesa_persistent_open(&device,&cfg));
#ifdef REMESA_TEST_OPENGL_W2_API_DRAW
#ifdef REMESA_TEST_OPENGL_NORMAL_DRAW
  normal_fill_calls=0;
  CHECK(!remesa_persistent_set_native_draw_scene(&device,api_native));
  CHECK(!remesa_persistent_set_native_fill(&device,normal_fill));
#else
  CHECK(!remesa_persistent_set_native_scene(&device,api_native));
#endif
#else
  CHECK(!remesa_persistent_set_native_scene(&device,context_native));
#endif
  struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
  CHECK(!remesa_screen_enable_linked_attachment_storage(screen));
  CHECK(!screen->caps.graphics&&!screen->caps.accelerated);
  automatic_context=enable>=2;
  if(automatic_context){
#ifdef REMESA_TEST_OPENGL_NORMAL_DRAW
   CHECK(!remesa_screen_enable_native_fill(screen,remesa_persistent_fill_region));
   CHECK(!remesa_screen_enable_native_draws(screen));
#ifdef REMESA_TEST_OPENGL_PRESERVE_DRAW
   CHECK(!remesa_screen_enable_native_preserve_candidate(screen));
#endif
   CHECK(remesa_screen_enable_homogeneous_frontend_lab(screen)==ENOTSUP);
#else
   CHECK(!remesa_screen_enable_homogeneous_frontend_lab(screen));
#endif
   CHECK(screen->caps.graphics&&!screen->caps.accelerated);
   CHECK(screen->shader_caps[PIPE_SHADER_VERTEX].max_inputs==2);
  }
  if(enable){original_create=screen->context_create;screen->context_create=create_enabled;}
  unsigned width=enable==3?1280:enable==4?640:32,height=enable==3?800:enable==4?640:32;
  struct pipe_resource *color=attachment(screen,width,height,false);CHECK(color);
  if(enable>=3){
   struct pipe_resource *depth=attachment(screen,width,height,true);CHECK(depth);
   fixed_function_scene(screen,color,depth,&device,&f);remesa_resource_reference(&depth,NULL);
   remesa_resource_reference(&color,NULL);screen->destroy(screen);
   CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&device));
   glsl_type_singleton_decref();continue;
  }
  struct remesa_opengl_lab_report report={0};unsigned char pixels[4096];memset(pixels,0xa5,sizeof pixels);
  int error=remesa_opengl_lab_draw_arrays_quarter(screen,color,&report,pixels,sizeof pixels);
  printf("RESOURCE_GL enable=%u setup=%d context=%u GL=%u GLSL=%u fbo=%x linked=%u called=%u VS=%u FS=%u pipe=%u submissions=%u\n",
   enable,error,report.context_created,report.computed_version,report.glsl_version,
   report.fbo_status,report.program_linked,report.program_draw_called,
   report.program_vs_error,report.program_fs_error,report.program_draw_pipe_error,dispatch_calls);
  printf("SHADER_LOG %s\nPROGRAM_LOG %s\n",report.shader_log,report.program_log);
  fflush(stdout);
  CHECK(!dispatch_calls);
  for(unsigned i=0;i<sizeof pixels;i++)CHECK(pixels[i]==0xa5);
  CHECK(!report.readback_bytes);
  remesa_resource_reference(&color,NULL);screen->destroy(screen);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&device));
  glsl_type_singleton_decref();
 }
 CHECK(enabled_contexts);return 0;
}
