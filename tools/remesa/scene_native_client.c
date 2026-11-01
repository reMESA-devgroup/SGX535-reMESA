/* SPDX-License-Identifier: MIT
 * Closed LAB bridge: actual Mesa owners on the actual Linux memory provider.
 * Borrows an already-open fd from a supervising collector; never opens a node,
 * enables a gate, reboots, uploads fixture images or presents a CPU frame.
 * Parent retains that SAME drm_file and all GEM resources after child exit.
 * Failed/uncertain attempts perform no teardown; parent closes fd for kernel
 * quarantine. Success guarantees retirement before host capture release.
 */
#include "remesa_linux_memory.h"
#include "remesa_shader_program.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define OK(x) do{int error=(x);if(error){fprintf(stderr,"line%u error%d\n",__LINE__,error);return 1;}}while(0)
static void output(const char *dir,const char *name,const void *data,size_t bytes)
{char path[4096];assert(snprintf(path,sizeof path,"%s/%s",dir,name)>0);FILE *f=fopen(path,"wbx");assert(f);assert(fwrite(data,1,bytes,f)==bytes);assert(!fclose(f));}
static uint64_t ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static struct remesa_shader_program vertex(unsigned kind)
{
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_VERTEX};
 if(kind<2){
  ir.count=1;ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_GEARS_VERTEX,
   .dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,
   .input_slot=kind,.bits={kind?9:10,kind?37:41,0x3f800000,1}};
 }else if(kind==2){
  ir.count=8;
  for(unsigned n=0;n<6;n++)ir.instructions[n]=(struct remesa_ir_instruction){
   .dst=n,.components=n<3||n==5?4:1,.type=REMESA_IR_FLOAT32};
  for(unsigned n=0;n<3;n++){ir.instructions[n].op=REMESA_IR_INPUT;ir.instructions[n].input_slot=n;}
  ir.instructions[3].op=REMESA_IR_DOT3;ir.instructions[3].src[0].reg=1;ir.instructions[3].src[1].reg=2;
  ir.instructions[4].op=REMESA_IR_SATURATE;ir.instructions[4].src[0].reg=3;
  ir.instructions[5].op=REMESA_IR_COMPOSE4;
  for(unsigned n=0;n<4;n++)ir.instructions[5].src[n].reg=4;
  ir.instructions[6]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32};
  ir.instructions[7]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,.src={{5,0}}};
 }else{
  ir.count=2;
  ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32};
  ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 }
 struct remesa_shader_program p;assert(!remesa_shader_program_prepare(&ir,&p));return p;
}
static struct remesa_shader_program fragment(unsigned kind)
{
 struct remesa_shader_program p;
 if(!kind){assert(!remesa_shader_program_prepare_color(0xff00ff00,&p));return p;}
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_FRAGMENT};
 if(kind==1){
  ir.count=2;ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32,.input_slot=1};
  ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 }else{
  ir.count=1;ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_FLAT_COLOR_INPUT,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER,.input_slot=1};
 }
 assert(!remesa_shader_program_prepare(&ir,&p));return p;
}

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
int main(int argc,char **argv)
{
 if(argc!=5){fprintf(stderr,"usage: scene_native_client borrowed-fd 1280x800|640x640 existing-output-dir pinned-profile-packet\n");return 2;}
 char *end=NULL;errno=0;long fd=strtol(argv[1],&end,10);if(errno||!*argv[1]||*end||fd<0||fd>INT_MAX)return 2;
 unsigned width,height,profile;if(!strcmp(argv[2],"1280x800")){width=1280;height=800;profile=5;}else if(!strcmp(argv[2],"640x640")){width=height=640;profile=6;}else return 2;
 const char *dir=argv[3];uint64_t times[3]={ns(),0,0};
 struct remesa_linux_memory *memory=NULL;OK(remesa_linux_memory_begin(fd,&memory));
 struct remesa_device device={0};struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=memory,.context=remesa_linux_memory_context(memory),.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};
 cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 OK(remesa_persistent_open(&device,&cfg));OK(remesa_persistent_set_native_scene(&device,remesa_linux_memory_scene_lab_dispatch));void *c[11]={0},*bg=NULL,*aux=NULL,*region=NULL,*table=NULL,*parameter=NULL,*dpm=NULL,*event=NULL;
 struct remesa_shader_program vs=vertex(2),fs=fragment(1);struct remesa_shader_linkage link;OK(remesa_shader_linkage_prepare(&vs,&fs,&link));const uint32_t banks[2]={4,3};
 const float vertices[3][12]={{width*.25f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.75f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.25f,height*.75f,.125f,1,0,0,1,0,0,0,1,0}};
 const struct remesa_linked_geometry_input input={.vertices=vertices,.vertex_bytes=sizeof vertices,.vertex_count=3};
 OK(remesa_persistent_linked_program_prepare(&device,&link,banks,&c[0]));OK(remesa_persistent_linked_geometry_prepare(&device,&input,&c[1]));OK(remesa_persistent_linked_pds_prepare(&device,c[0],c[1],&c[2]));
 const struct remesa_persistent_linked_state_request state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}};
 OK(remesa_persistent_linked_state_prepare(&device,c[2],&state,4,&c[3]));OK(remesa_persistent_scene_close_prepare(&device,width,height,4,&c[4]));const struct remesa_linked_body_draw draw={c[3],0,3};OK(remesa_persistent_linked_body_prepare(&device,&draw,1,c[4],&c[5]));
 OK(remesa_persistent_initialized_prefix_prepare(&device,width,height,width==1280?1:.25f,4,3,4,&c[6]));OK(remesa_persistent_linked_ta_prepare(&device,c[6],c[5],&c[7]));
 OK(remesa_persistent_allocate_placed(&device,(size_t)width*height*4,REMESA_VA_SURFACE,&c[8]));OK(remesa_persistent_depth_allocate(&device,width,height,4,&c[9]));OK(remesa_persistent_finalized_targets_prepare(&device,c[7],c[8],c[9],width,&c[10]));
 OK(remesa_persistent_linked_background_prepare(&device,c[10],3,&bg));OK(remesa_persistent_linked_event_auxiliary_prepare(&device,c[10],3,&aux));OK(remesa_persistent_linked_region_allocate(&device,c[10],&region));
 OK(remesa_persistent_allocate_placed(&device,0x2000000,REMESA_VA_SURFACE,&table));OK(remesa_persistent_allocate_placed(&device,0x2000000,REMESA_VA_RASTER,&parameter));OK(remesa_persistent_linked_dpm_prepare(&device,table,parameter,&dpm));OK(remesa_persistent_linked_event_profile_prepare(&device,aux,&event));
 struct remesa_scene_capture capture;OK(remesa_persistent_scene_capture(&device,bg,event,region,dpm,&capture));struct remesa_scene_packet packet;OK(remesa_persistent_scene_packet_snapshot(&device,capture,&packet));expected_packet(argv[4],&packet);
 struct remesa_persistent_scene_footprint footprint;OK(remesa_persistent_scene_capture_snapshot(&device,capture,&footprint));
 struct {uint32_t version,count;struct {uint64_t address,bytes;uint32_t id,access;} bindings[32];} placement={.version=1,.count=footprint.allocation_count};
 _Static_assert(sizeof placement==776,"capture record");
 for(unsigned i=0;i<placement.count;i++){placement.bindings[i].address=footprint.allocations[i].gpu_va;placement.bindings[i].bytes=footprint.allocations[i].allocation_bytes;placement.bindings[i].id=footprint.allocations[i].handle;placement.bindings[i].access=footprint.allocations[i].access;}
 output(dir,"packet.bin",&packet,sizeof packet);output(dir,"placement.bin",&placement,sizeof placement);
 /* The retained copied body and CPU-only DPM plan are real GEM allocations,
  * even though the GPU references neither. Parent must account for both. */
 struct {uint64_t address,bytes;uint32_t id,access;} extras[2]={0};
 struct remesa_persistent_linked_body_backing body_meta;struct remesa_persistent_linked_dpm_backing dpm_meta;
 OK(remesa_persistent_linked_body_snapshot(&device,c[5],&body_meta));OK(remesa_persistent_linked_dpm_snapshot(&device,dpm,&dpm_meta));
 extras[0].address=body_meta.gpu_va;extras[0].bytes=body_meta.allocation_bytes;extras[0].id=body_meta.handle;
 extras[1].address=dpm_meta.gpu_va;extras[1].bytes=dpm_meta.allocation_bytes;extras[1].id=dpm_meta.handle;
 output(dir,"EXTRAS.bin",extras,sizeof extras);
 struct remesa_persistent_scene_controls controls;OK(remesa_persistent_scene_controls_snapshot(&device,capture,&controls));unsigned char control[264];memcpy(control,controls.raster,208);memcpy(control+208,controls.ta,56);output(dir,"CONTROLS.bin",control,sizeof control);
 uint32_t guard_ids[4]={0};uint64_t guard_va[4]={0};unsigned count=0;const uint64_t ranges[3][2]={{0x20000000,0x28000000},{0x30000000,0x40000000},{0x40000000,0xe0000000}};
 for(unsigned g=0;g<3;g++){
  uint64_t lo=UINT64_MAX,hi=0;for(unsigned i=0;i<placement.count;i++){uint64_t va=placement.bindings[i].address;if(va>=ranges[g][0]&&va<ranges[g][1]){if(va<lo)lo=va;if(va+placement.bindings[i].bytes>hi)hi=va+placement.bindings[i].bytes;}}
  for(unsigned i=0;i<2;i++){uint64_t va=extras[i].address;if(va>=ranges[g][0]&&va<ranges[g][1]){if(va<lo)lo=va;if(va+extras[i].bytes>hi)hi=va+extras[i].bytes;}}
  if(!hi)continue;const uint64_t candidates[2]={lo-4096,hi};for(unsigned k=0;k<2;k++){
   uint64_t va=candidates[k];if(va<ranges[g][0]||va+4096>ranges[g][1])continue;bool overlap=false;
   for(unsigned i=0;i<placement.count;i++)if(va<placement.bindings[i].address+placement.bindings[i].bytes&&placement.bindings[i].address<va+4096)overlap=true;
   for(unsigned i=0;i<2;i++)if(va<extras[i].address+extras[i].bytes&&extras[i].address<va+4096)overlap=true;
   for(unsigned i=0;i<count;i++)if(guard_va[i]==va)overlap=true;
   if(overlap)continue;assert(count<4);guard_va[count]=va;OK(cfg.ops->create(memory,4096,&guard_ids[count]));OK(cfg.ops->pin(memory,guard_ids[count]));OK(cfg.ops->gpu_map(memory,guard_ids[count],va,4096));unsigned char a5[4096];memset(a5,0xa5,sizeof a5);OK(remesa_linux_memory_upload_pages(memory,guard_ids[count],0,a5,sizeof a5));count++;
  }
 }
 assert(count==4);output(dir,"GUARD_IDS.bin",guard_ids,sizeof guard_ids);output(dir,"GUARD_VA.bin",guard_va,sizeof guard_va);
 struct remesa_memory_info info;OK(remesa_linux_memory_info(memory,0,&info));assert(info.objects==23&&info.pinned_objects==23&&!info.leases&&!info.held_jobs&&!info.quarantined);output(dir,"INFO-BEFORE.bin",&info,sizeof info);
 struct remesa_scene_probe_request request={.version=1,.profile=profile,.context=packet.owner,.budget_ms=1000},response={0};for(unsigned i=0;i<17;i++)request.ids[i]=packet.bindings[i].id;output(dir,"REQUEST.bin",&request,sizeof request);
 times[1]=ns();void *fence=NULL;int error=remesa_persistent_scene_capture_submit(&device,capture,&fence);times[2]=ns();int response_error=remesa_linux_memory_scene_lab_response(memory,&response);output(dir,"RESPONSE.bin",&response,sizeof response);output(dir,"TIMES.bin",times,sizeof times);
 if(response_error){fprintf(stderr,"no entered kernel response%d; no teardown/retry\n",response_error);return 1;}
 if(error){fprintf(stderr,"entered scene result%d; no teardown/retry\n",error);return 1;}
 assert(fence);OK(device.ops->wait(device.priv,fence,0,true));device.ops->destroy_fence(device.priv,fence);
 assert(remesa_persistent_scene_capture_release(&device,capture)==ESTALE);
 /* No GEM teardown: the supervising parent still owns the borrowed fd and
  * reads the real retired allocations, then explicitly releases all of them. */
 return 0;
}
