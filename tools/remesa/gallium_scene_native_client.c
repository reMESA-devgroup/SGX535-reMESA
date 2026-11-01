/* SPDX-License-Identifier: MIT
 * Closed LAB client of actual bound Gallium/NIR -> GEM scene submission.
 * Borrows the supervisor's fd. Never opens a node, enables a gate, installs a
 * module, retries, presents pixels or performs CPU vertex/raster execution.
 * Diagnostic reads happen INSIDE the native callback after authenticated
 * kernel retirement and BEFORE capture consumption frees temporary owners.
 * This collector is separate from the normal driver path.
 * Exact profile fixtures are required. Kernel profiles7/8 (or9/10 with
 * --matrix-window) must independently
 * qualify this relocated Gallium graph: use the matching closed kernel before
 * executing physically. --prepare-only never enters the scene ioctl.
 */
#include "remesa_linux_memory.h"
#include "remesa_screen.h"
#include "remesa_resource_ref.h"
#include "scene_fixture.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "nir_builder.h"
#include "util/u_inlines.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CHECK(x) assert(x)
#define OK(x) do{int e=(x);if(e){fprintf(stderr,"line%u error%d\n",__LINE__,e);return 1;}}while(0)
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
_Static_assert(sizeof(struct remesa_linux_memory_inventory)==784,"LAB host inventory record");
static void output(const char *dir,const char *name,const void *data,size_t bytes)
{char path[4096];assert(snprintf(path,sizeof path,"%s/%s",dir,name)>0);FILE *f=fopen(path,"wbx");assert(f);assert(fwrite(data,1,bytes,f)==bytes);assert(!fclose(f));}
static uint64_t ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static uint32_t translate(const uint32_t old[17],const struct remesa_scene_packet *p,uint32_t id)
{for(unsigned i=0;i<17;i++)if(old[i]==id)return p->bindings[i].id;assert(0);return 0;}
static void expected_packet(const char *path,const struct remesa_scene_packet *p)
{
 FILE *f=fopen(path,"rb");assert(f);struct remesa_scene_packet expected;assert(fread(&expected,1,sizeof expected,f)==sizeof expected&&fgetc(f)==EOF);assert(!fclose(f));assert(remesa_scene_packet_valid(&expected));
 uint32_t old[17];for(unsigned i=0;i<17;i++)old[i]=expected.bindings[i].id;
 expected.owner=p->owner;
 for(unsigned i=0;i<17;i++)expected.bindings[i].id=p->bindings[i].id;
 expected.actions.region_id=translate(old,p,expected.actions.region_id);expected.actions.table_id=translate(old,p,expected.actions.table_id);expected.actions.parameter_id=translate(old,p,expected.actions.parameter_id);
 uint32_t *control_ids[]={&expected.controls.color_id,&expected.controls.depth_id,&expected.controls.ta_id,&expected.controls.background_id,&expected.controls.event_id};
 for(unsigned i=0;i<5;i++)*control_ids[i]=translate(old,p,*control_ids[i]);
 for(unsigned i=0;i<expected.use_count;i++)expected.uses[i].id=translate(old,p,expected.uses[i].id);
 for(unsigned i=0;i<expected.window_count;i++){expected.windows[i].index_id=translate(old,p,expected.windows[i].index_id);expected.windows[i].vertex_id=translate(old,p,expected.windows[i].vertex_id);}
 assert(!memcmp(&expected,p,sizeof expected));
}
static nir_shader *linked_shader(unsigned fragment)
{
 static const nir_shader_compiler_options options={0};
 nir_builder b=nir_builder_init_simple_shader(fragment?MESA_SHADER_FRAGMENT:MESA_SHADER_VERTEX,&options,"bound three-input transport");
 if(fragment){
  nir_variable *in=nir_variable_create(b.shader,nir_var_shader_in,glsl_vec4_type(),"color");
  nir_variable *out=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"result");
  in->data.location=VARYING_SLOT_COL0;out->data.location=FRAG_RESULT_DATA0;
  nir_store_deref(&b,nir_build_deref_var(&b,out),nir_load_deref(&b,nir_build_deref_var(&b,in)),15);
 }else{
  b.shader->info.io_lowered=true;nir_def *z=nir_imm_int(&b,0),*in[3];
  for(unsigned k=0;k<3;k++)in[k]=nir_load_input(&b,4,32,z,.base=k,.dest_type=nir_type_float32,
   .io_semantics={.location=VERT_ATTRIB_GENERIC0+k,.num_slots=1});
  nir_def *light=nir_fsat(&b,nir_fdot3(&b,in[1],in[2]));
  nir_store_output(&b,in[0],z,.write_mask=15,.src_type=nir_type_float32,
   .io_semantics={.location=VARYING_SLOT_POS,.num_slots=1});
  nir_store_output(&b,nir_vec4(&b,light,light,light,light),z,.base=1,.write_mask=15,.src_type=nir_type_float32,
   .io_semantics={.location=VARYING_SLOT_COL0,.num_slots=1});
 }
 nir_validate_shader(b.shader,"bound raw attribute transport");return b.shader;
}

static struct pipe_resource *attachment(struct pipe_screen *s,unsigned w,unsigned h,bool depth)
{
 const struct pipe_resource t={.target=PIPE_TEXTURE_2D,.format=depth?PIPE_FORMAT_Z24X8_UNORM:PIPE_FORMAT_B8G8R8A8_UNORM,
  .bind=depth?PIPE_BIND_DEPTH_STENCIL:PIPE_BIND_RENDER_TARGET,.width0=w,.height0=h,.depth0=1,.array_size=1};
 return s->resource_create(s,&t);
}

static const char *out_dir,*fixture_dir;static bool prepare_only,matrix_window;
static uint64_t timings[5];
static void fixture_path(char path[4096],const char *name)
{CHECK(snprintf(path,4096,"%s/%s",fixture_dir,name)>0);}
static void check_placement(struct remesa_linux_memory *m,const struct remesa_scene_packet *p)
{
 char path[4096];fixture_path(path,"packet.bin");expected_packet(path,p);
 fixture_path(path,"placement.bin");FILE *f=fopen(path,"rb");CHECK(f);
 struct remesa_scene_fixture expected,actual={.version=1,.count=17};
 CHECK(fread(&expected,1,sizeof expected,f)==sizeof expected&&fgetc(f)==EOF&&!fclose(f));
 CHECK(expected.version==1&&expected.count==17);
 struct remesa_linux_memory_inventory inv;CHECK(!remesa_linux_memory_inventory_snapshot(m,&inv)&&inv.count==20);
 for(unsigned i=0;i<17;i++){
  unsigned found=0;
  for(unsigned k=0;k<inv.count;k++)if(inv.entries[k].id==p->bindings[i].id){
   CHECK(inv.entries[k].flags==REMESA_MEMORY_INVENTORY_LIVE);
   actual.bindings[i].address=inv.entries[k].gpu_va;actual.bindings[i].bytes=inv.entries[k].bytes;
   actual.bindings[i].id=p->bindings[i].id;actual.bindings[i].access=p->bindings[i].access;found++;
  }
  CHECK(found==1&&actual.bindings[i].address==expected.bindings[i].address&&actual.bindings[i].bytes==expected.bindings[i].bytes&&actual.bindings[i].access==expected.bindings[i].access);
 }
 output(out_dir,"packet.bin",p,sizeof(*p));output(out_dir,"placement.bin",&actual,sizeof actual);
 output(out_dir,"INVENTORY-OWNERS.bin",&inv,sizeof inv);
}
static void guards(struct remesa_linux_memory *m)
{
 struct remesa_linux_memory_inventory inv;CHECK(!remesa_linux_memory_inventory_snapshot(m,&inv));
 const struct remesa_persistent_ops *o=remesa_linux_memory_ops();
 const uint64_t ranges[3][2]={{0x20000000,0x28000000},{0x30000000,0x40000000},{0x40000000,0xe0000000}};
 unsigned n=0;uint32_t ids[4]={0};uint64_t addresses[4]={0};
 for(unsigned g=0;g<3;g++){
  uint64_t lo=UINT64_MAX,hi=0;
  for(unsigned i=0;i<inv.count;i++){
   uint64_t va=inv.entries[i].gpu_va;CHECK(va&&inv.entries[i].flags==REMESA_MEMORY_INVENTORY_LIVE);
   if(va>=ranges[g][0]&&va<ranges[g][1]){if(va<lo)lo=va;if(va+inv.entries[i].bytes>hi)hi=va+inv.entries[i].bytes;}
  }
  if(!hi)continue;
  const uint64_t at[2]={lo-4096,hi};
  for(unsigned k=0;k<2;k++){
   if(at[k]<ranges[g][0]||at[k]+4096>ranges[g][1])continue;
   CHECK(n<4);addresses[n]=at[k];CHECK(!o->create(m,4096,&ids[n]));CHECK(!o->pin(m,ids[n]));CHECK(!o->gpu_map(m,ids[n],at[k],4096));
   unsigned char a5[4096];memset(a5,0xa5,sizeof a5);CHECK(!remesa_linux_memory_upload_pages(m,ids[n],0,a5,sizeof a5));n++;
  }
 }
 CHECK(n==4);output(out_dir,"GUARD_IDS.bin",ids,sizeof ids);output(out_dir,"GUARD_VA.bin",addresses,sizeof addresses);
}
static void dump_backing(struct remesa_linux_memory *m,const struct remesa_scene_packet *p,bool after)
{
 struct remesa_linux_memory_inventory inv;CHECK(!remesa_linux_memory_inventory_snapshot(m,&inv)&&inv.count==24);
 output(out_dir,after?"INVENTORY-AFTER.bin":"INVENTORY-BEFORE.bin",&inv,sizeof inv);
 for(unsigned i=0;i<inv.count;i++){
  CHECK(inv.entries[i].flags==REMESA_MEMORY_INVENTORY_LIVE);
  uint32_t id=inv.entries[i].id;bool rw=false;
  for(unsigned k=0;k<17;k++)if(p->bindings[k].id==id)rw=p->bindings[k].access!=REMESA_ACCESS_READ;
  if(rw&&(!after||(id!=p->controls.color_id&&id!=p->controls.depth_id)))continue;
  struct remesa_memory_info info;CHECK(!remesa_linux_memory_info(m,id,&info));
  CHECK(info.object_bytes==inv.entries[i].bytes&&info.gpu_va==inv.entries[i].gpu_va&&info.object_pinned&&info.verified_pages==info.object_mapped_pages&&!info.leases&&!info.held_jobs&&!info.quarantined);
  const struct remesa_persistent_ops *o=remesa_linux_memory_ops();void *cpu=NULL;CHECK(!o->cpu_map(m,id,false,&cpu)&&cpu);
  char name[80];CHECK(snprintf(name,sizeof name,"%s-id-%u.bin",after?"after":"before",id)>0);output(out_dir,name,cpu,inv.entries[i].bytes);CHECK(!o->cpu_unmap(m,id,false));
 }
 struct remesa_memory_info info;CHECK(!remesa_linux_memory_info(m,0,&info));
 CHECK(info.objects==24&&info.pinned_objects==24&&!info.leases&&!info.held_jobs&&!info.quarantined);
 output(out_dir,after?"INFO-AFTER.bin":"INFO-BEFORE.bin",&info,sizeof info);
}
static enum remesa_dispatch collect_native(void *priv,const struct remesa_gpu_scene_submit *submit,struct remesa_gpu_scene_receipt *receipt)
{
 struct remesa_linux_memory *m=priv;CHECK(submit&&submit->packet&&submit->context==remesa_linux_memory_context(m));
 timings[1]=ns();check_placement(m,submit->packet);guards(m);dump_backing(m,submit->packet,false);timings[2]=ns();
 if(prepare_only)return REMESA_NOT_ISSUED;
 enum remesa_dispatch result=matrix_window?remesa_linux_memory_gallium_matrix_scene_lab_dispatch(m,submit,receipt):remesa_linux_memory_gallium_scene_lab_dispatch(m,submit,receipt);timings[3]=ns();
 struct remesa_scene_probe_request response;int e=remesa_linux_memory_scene_lab_response(m,&response);
 if(!e)output(out_dir,"RESPONSE.bin",&response,sizeof response);
 if(result==REMESA_ISSUED){
  CHECK(!e&&receipt->retired&&receipt->published&&receipt->acknowledged);
  dump_backing(m,submit->packet,true);timings[4]=ns();
 }
 return result;
}
int main(int argc,char **argv)
{
 if((argc!=6&&argc!=7)||(argc==7&&strcmp(argv[6],"--matrix-window"))||(strcmp(argv[5],"--prepare-only")&&strcmp(argv[5],"--execute"))){
  fprintf(stderr,"usage: gallium_scene_native_client borrowed-fd 1280x800|640x640 existing-output-dir pinned-fixture-dir --prepare-only|--execute [--matrix-window]\n");return 2;
 }
 char *end;errno=0;long fd=strtol(argv[1],&end,10);if(errno||!*argv[1]||*end||fd<0||fd>INT_MAX)return 2;
 unsigned width,height;if(!strcmp(argv[2],"1280x800")){width=1280;height=800;}else if(!strcmp(argv[2],"640x640")){width=height=640;}else return 2;
 matrix_window=argc==7;out_dir=argv[3];fixture_dir=argv[4];prepare_only=!strcmp(argv[5],"--prepare-only");timings[0]=ns();
 struct remesa_linux_memory *memory=NULL;OK(remesa_linux_memory_begin(fd,&memory));
 struct remesa_device device={0};struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=memory,.context=remesa_linux_memory_context(memory),.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 OK(remesa_persistent_open(&device,&cfg));OK(remesa_persistent_set_native_scene(&device,collect_native));
 glsl_type_singleton_init_or_ref();struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);OK(remesa_screen_enable_linked_attachment_storage(screen));
 struct pipe_resource *color=attachment(screen,width,height,false),*depth=attachment(screen,width,height,true);CHECK(color&&depth);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 const struct pipe_framebuffer_state fb={.width=width,.height=height,.nr_cbufs=1,.cbufs={{.texture=color,.format=PIPE_FORMAT_B8G8R8A8_UNORM}},.zsbuf={.texture=depth,.format=PIPE_FORMAT_Z24X8_UNORM}};
 ctx->set_framebuffer_state(ctx,&fb);OK(remesa_context_error(ctx));
 const struct pipe_blend_state blend={.rt={{.colormask=PIPE_MASK_RGBA}}};
 const struct pipe_depth_stencil_alpha_state less={.depth_enabled=true,.depth_writemask=true,.depth_func=PIPE_FUNC_LESS};
 void *bs=ctx->create_blend_state(ctx,&blend),*ds=ctx->create_depth_stencil_alpha_state(ctx,&less);CHECK(bs&&ds);
 ctx->bind_blend_state(ctx,bs);ctx->bind_depth_stencil_alpha_state(ctx,ds);OK(remesa_context_error(ctx));
 struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=linked_shader(0)};void *vs=ctx->create_vs_state(ctx,&sh);CHECK(vs);ctx->bind_vs_state(ctx,vs);sh.ir.nir=linked_shader(1);void *fs=ctx->create_fs_state(ctx,&sh);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 float raw[3][12]={{width*.25f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.75f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.25f,height*.75f,.125f,1,0,0,1,0,0,0,1,0}};
 if(matrix_window){
  const float clip[3][4]={{-.5f,.5f,-.75f,1},{.5f,.5f,-.75f,1},{-.5f,-.5f,-.75f,1}};
  for(unsigned i=0;i<3;i++)memcpy(raw[i],clip[i],sizeof clip[i]);
 }
 const struct pipe_resource vb_template={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.bind=PIPE_BIND_VERTEX_BUFFER,.width0=sizeof raw,.height0=1,.depth0=1,.array_size=1};struct pipe_resource *vb=screen->resource_create(screen,&vb_template);CHECK(vb);ctx->buffer_subdata(ctx,vb,0,0,sizeof raw,raw);OK(remesa_context_error(ctx));
 struct pipe_vertex_buffer binding={.buffer.resource=vb};vb->reference.count++;ctx->set_vertex_buffers(ctx,1,&binding);OK(remesa_context_error(ctx));
 struct pipe_vertex_element elements[3]={0};for(unsigned i=0;i<3;i++){elements[i].src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;elements[i].src_stride=48;elements[i].src_offset=i*16;}void *el=ctx->create_vertex_elements_state(ctx,3,elements);CHECK(el);ctx->bind_vertex_elements_state(ctx,el);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};struct pipe_draw_start_count_bias draw={.count=3};
 struct remesa_linked_scene_request options={.state={.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}},.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=width==640?.25f:1};
 OK(remesa_context_linked_isp_snapshot(ctx,&options.state.isp));
 if(matrix_window){
  const struct pipe_viewport_state viewport={.scale={width*.5f,-(float)height*.5f,.5f},.translate={width*.5f,height*.5f,.5f}};
  ctx->set_viewport_states(ctx,0,1,&viewport);OK(remesa_context_error(ctx));
  OK(remesa_context_window_viewport_snapshot(ctx,24,&options.state.viewport));
 }

 struct pipe_fence_handle *fence=NULL;int e=remesa_context_submit_linked_scene(ctx,&info,&draw,&options,&fence);
 output(out_dir,"TIMES.bin",timings,sizeof timings);output(out_dir,"RESULT.bin",&e,sizeof e);
 if(!e||(prepare_only&&e==EOPNOTSUPP)){
  struct remesa_linux_memory_inventory remaining;struct remesa_memory_info info;
  OK(remesa_linux_memory_inventory_snapshot(memory,&remaining));OK(remesa_linux_memory_info(memory,0,&info));
  CHECK(remaining.count==7&&info.objects==7&&info.pinned_objects==7&&!info.leases&&!info.held_jobs&&!info.quarantined);
  output(out_dir,"INVENTORY-REMAINING.bin",&remaining,sizeof remaining);output(out_dir,"INFO-RETURN.bin",&info,sizeof info);
 }
 if(prepare_only){CHECK(e==EOPNOTSUPP&&!fence);return 0;}
 if(e){fprintf(stderr,"scene error%d; no teardown/retry\n",e);return 1;}
 CHECK(fence&&screen->fence_finish(screen,ctx,fence,0));OK(remesa_resource_wait_writer(ctx,color,0));OK(remesa_resource_wait_writer(ctx,depth,0));screen->fence_reference(screen,&fence,NULL);
 CHECK(!screen->caps.graphics&&!screen->caps.accelerated&&!remesa_context_cpu_clear_count(ctx));
 /* Parent still owns the borrowed fd and remaining frontend/guard GEM BOs.
  * Retired temporary owners have already been collected before destruction. */
 return 0;
}
