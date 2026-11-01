/* SPDX-License-Identifier: GPL-2.0-only. Held native action binding; no MMIO. */
#define REMESA_GEM_PRIVATE_TEST_MAIN prior_scene_action_fixture
#include "test_gem_private.c"
#include "../remesa_gem_scene_actions.h"
#include "../remesa_gem_scene_controls.h"
#include "../remesa_gem_scene_use.h"
#include "../remesa_gem_scene_ta.h"
#include "../remesa_gem_scene_states.h"
#include "../remesa_gem_scene_vertices.h"
#include "../remesa_gem_scene_fragments.h"
#include "../remesa_gem_scene_background.h"
#include "../remesa_gem_scene_event.h"
#include "../remesa_gem_scene_job.h"
#include "remesa_render_target.h"
#include "remesa_ta_index.h"
#include "remesa_scene_layout.h"
#include "scene_service_model.inc"
#include "scene_backend_model.inc"
static void state_word(u8 *p,u32 word)
{for(unsigned i=0;i<4;i++)p[i]=word>>(8*i);}
/* Modeled inputs only. Separate tests compare validators against unchanged
 * actual Mesa encoders and independent preserved state-PDS carriers. */
static void state_fixture(u8 *p,u32 source,unsigned count,u32 fixup,u8 *aux)
{
 memset(p,0xa5,60);state_word(p,source);state_word(p+4,0x80000000u|((count-1)<<21)|(count-1));
 state_word(p+8,0x200000u|fixup);state_word(p+48,0x07030223);state_word(p+52,0x07042345);state_word(p+56,0xaf000000);
 state_word(aux,0xa0000000);state_word(aux+4,0x28a10001u|((count-1)<<12));
 state_word(aux+8,0xa0200000u|(count<<7));state_word(aux+12,0xfb274000);
}
static void vertex_fixture(u8 *p,u32 source,unsigned stride,u32 fixup,bool compiled)
{
 memset(p,0xa5,64);state_word(p,source);state_word(p+4,0x80000000u|((stride-1)<<21)|(stride-1));
 state_word(p+16,0x200000u|fixup);state_word(p+32,stride*4);
 if(compiled){state_word(p+20,0xe0000000);state_word(p+36,12);}
 state_word(p+48,0x67800070);state_word(p+52,0x2f030343);state_word(p+56,0x030803e5);state_word(p+60,0xaf000000);
}
static void fragment_fixture(u8 *p,u32 fixup,bool color)
{
 memset(p,0xa5,color?60:56);state_word(p,(color?0x80000u:0)|fixup);state_word(p+4,0);state_word(p+32,0x20);
 state_word(p+48,0x07000345);
 if(color){state_word(p+36,0x0fc0a00f);state_word(p+52,0x070418a2);state_word(p+56,0xaf000000);}
 else state_word(p+52,0xaf000000);
}
static void scene_images(struct remesa_memory_view views[17],const struct remesa_memory_binding uses[17],
 const struct remesa_gem_scene_use *use,unsigned width,unsigned height,unsigned pitch)
{
 u8 *stream=(u8 *)views[6].cpu+4,stream_saved[68];
 CHECK(!remesa_ta_state_reference_encode(uses[9].va+352,11,false,stream));
 struct remesa_ta_index_request index={.index_bo_gpu_va=uses[9].va,.index_bo_bytes=4096,.index_byte_offset=128,
  .pds_bo_gpu_va=uses[9].va,.pds_bo_bytes=4096,.pds_byte_offset=160,.pds_object_bytes=64,
  .index_count=3,.pds_data_dwords=12,.descriptor_a_field10=4,.descriptor_b_field38=4};
 CHECK(!remesa_ta_index_triangles_encode(&index,stream+8));
 CHECK(!remesa_ta_state_reference_encode(uses[10].va+96,14,false,stream+28));
 index.index_bo_gpu_va=uses[11].va;index.index_byte_offset=144;index.pds_bo_gpu_va=uses[12].va;
 index.pds_byte_offset=0;index.descriptor_a_field10=12;index.descriptor_b_field38=32;
 CHECK(!remesa_ta_index_compiled_position_color_triangles_encode(&index,stream+36));
 struct remesa_ta_scene_close_request end={uses[13].va,4096,256,60};
 CHECK(!remesa_ta_scene_close_encode(&end,stream+56));memcpy(stream_saved,stream,68);
 u8 *prefix=views[9].cpu,*draw=views[10].cpu,*close=views[13].cpu;
 state_fixture(prefix+352,uses[9].va+304,11,use->lower19[0],prefix+32);state_word(prefix+304,0x54c5);
 state_fixture(draw+96,uses[10].va,14,use->lower19[2],draw+64);state_word(draw,0xf41);
 state_fixture(close+256,uses[13].va,2,use->lower19[3],close+16);state_word(close,0x2000);
 state_word(close+4,(((width+15)>>4)<<16)|((height+15)>>4));
 vertex_fixture(prefix+160,uses[9].va+64,4,use->lower19[4],false);
 vertex_fixture(views[12].cpu,uses[11].va,12,use->lower19[5],true);
 u8 *indices=views[11].cpu;for(unsigned k=0;k<3;k++){prefix[128+k*2]=k;prefix[129+k*2]=0;indices[144+k*2]=k;indices[145+k*2]=0;}
 fragment_fixture(prefix+224,use->lower19[1],false);state_word(prefix+288,0xaf000000);
 fragment_fixture((u8 *)views[12].cpu+192,use->lower19[6],true);state_word((u8 *)views[12].cpu+256,0xaf000000);
 state_word(prefix+316,((uses[9].va+288)>>4)&0xffffff);state_word(prefix+320,0x30000);state_word(prefix+324,0x0c000000|(((uses[9].va+224)>>4)&0xffffff));
 state_word(draw+8,((uses[12].va+256)>>4)&0xffffff);state_word(draw+12,0x30030001);state_word(draw+16,0x0c000000|(((uses[12].va+192)>>4)&0xffffff));
 u8 *bg=(u8 *)views[7].cpu+16,*bp=(u8 *)views[16].cpu+32;
 memset(bg,0,68);state_word(bg,((uses[16].va+96)>>4)&0xffffff);state_word(bg+4,0x30001);
 state_word(bg+8,0x0c000000|(((uses[16].va+32)>>4)&0xffffff));
 state_word(bg+20,0x3f800000);state_word(bg+32,0x3f800000);state_word(bg+36,2);
 state_word(bg+44,(1024u<<20)|(1024u<<4));state_word(bg+52,((1024u+width)<<20)|(1024u<<4));
 state_word(bg+60,(1024u<<20)|((1024u+height)<<4));
 state_word(bg+48,0x3f800000);state_word(bg+56,0x3f800000);state_word(bg+64,0x3f800000);
 memset(bp,0xa5,64);state_word(bp,0x100000u|use->lower19[7]);state_word(bp+4,0);state_word(bp+8,0x001e0092);
 state_word(bp+12,0x6c000000u|((width-1)<<12)|(height-1));state_word(bp+32,0x20);state_word(bp+36,0xf800);
 state_word(bp+40,uses[4].va);state_word(bp+48,0x07000345);state_word(bp+52,0x070418a2);
 state_word(bp+56,0x07042364);state_word(bp+60,0xaf000000);state_word(bp+64,0xaf000000);
 state_word(views[16].cpu,0xa0000000);state_word((u8 *)views[16].cpu+4,0x28851001);
 u8 *ep=(u8 *)views[8].cpu+32,*target=views[15].cpu,*transfer=(u8 *)views[14].cpu+96,*end_aux=(u8 *)views[14].cpu+128;
 memset(ep,0xa5,148);state_word(ep,use->lower19[9]);state_word(ep+4,0);state_word(ep+8,uses[15].va);
 state_word(ep+12,0x80000003);state_word(ep+16,0x200000u|use->lower19[8]);state_word(ep+20,0);
 const u32 event_tail[7]={8,0,2,0x10000000,0x7f800,0x07f00000,0x08000000};
 const u32 event_code[21]={0xcf820030,0x87600000,0x90000005,0x070003e5,0xaf000000,0xcf820830,0x87600000,0x90000014,
  0x07042363,0x170086e0,0xf7800b31,0xcf621031,0xc762c070,0xf7800b32,0xcf641432,0xc764c070,0xf7811933,0xcf661833,0xc766c070,0x070b0345,0xaf000000};
 for(unsigned k=0;k<7;k++)state_word(ep+32+k*4,event_tail[k]);
 for(unsigned k=0;k<21;k++)state_word(ep+64+k*4,event_code[k]);
 state_word(target,(((pitch>>1)-1)<<16)|0x8000);state_word(target+4,uses[4].va);state_word(target+8,0);state_word(target+12,((height-1)<<12)|(pitch-1));
 state_word(transfer,0x84208180);state_word(transfer+4,0xfb200004);state_word(transfer+8,0x81200080);state_word(transfer+12,0xfb240044);
 state_word(end_aux,0);state_word(end_aux+4,0xf8348000);
 const u32 compiled_body[16]={0xa0000000,0x28a13001,0xa0400208,0x10a02001,0x01000001,0xfa100000,0x60a00200,0x28a12001,
  0x01010100,0xfa100000,0x50800230,0x18a13201,0x50800234,0x18a13001,0xa0200000,0xfb275000};
 state_word(prefix,compiled_body[0]);state_word(prefix+4,compiled_body[1]);state_word(prefix+8,compiled_body[14]);state_word(prefix+12,compiled_body[15]);
 state_word(prefix+16,0xff00);state_word(prefix+20,0xfca7f181);
 for(unsigned k=0;k<16;k++)state_word((u8 *)views[14].cpu+k*4,compiled_body[k]);
 state_word((u8 *)views[14].cpu+64,0xa0000000);state_word((u8 *)views[14].cpu+68,0x28851001);
}
static u32 scene_storage(struct remesa_gem_memory *s,u64 size,u64 address)
{
 struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=size};
 CHECK(!remesa_gem_memory_request(s,&file,&r));u32 id=r.id;
 r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_PIN,.context=77,.id=id};
 CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_MAP;r.va=address;
 CHECK(!remesa_gem_memory_request(s,&file,&r));return id;
}
static void action_case(unsigned width,unsigned height,unsigned fault)
{
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 struct remesa_memory_binding uses[17];struct remesa_memory_view views[17];
 struct remesa_memory_snapshot snapshot;struct remesa_memory_ticket ticket,bad_ticket;
 struct remesa_gem_private loan={0};struct remesa_scene_layout layout;
 struct remesa_gem_scene_actions plan,saved,bad;
 struct remesa_gem_scene_controls controls,control_saved,control_bad;
 struct remesa_gem_scene_use use,use_saved,use_bad;
 struct remesa_gem_scene_ta ta_refs,ta_saved;
 struct remesa_gem_scene_states states,states_saved;
 struct remesa_gem_scene_vertices vertices,vertices_saved;
 struct remesa_gem_scene_fragments fragments,fragments_saved;
 struct remesa_gem_scene_background background,background_saved;
 struct remesa_gem_scene_event event,event_saved;
 struct {u32 prefix;struct remesa_gem_scene_action_command scene;
  struct remesa_gem_scene_control_command controls;
  struct remesa_gem_scene_use_command use;struct remesa_gem_scene_use_entry entries[REMESA_SCENE_USE_MAX];
  struct remesa_gem_scene_vertex_command vertex;struct remesa_gem_scene_vertex_window windows[REMESA_GEM_SCENE_VERTEX_MAX];u32 tail;} command={0};
 const unsigned control_offset=(u8 *)&command.controls-(u8 *)&command;
 const unsigned use_offset=(u8 *)&command.use-(u8 *)&command;
 const unsigned vertex_offset=(u8 *)&command.vertex-(u8 *)&command;
 u32 pitch=(width+31)&~31u;
 CHECK(!remesa_scene_layout_build(width,height,0,&layout));
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));expected_memory=s;expected_guard=&guard;
 u32 ids[17];
 ids[0]=scene_storage(s,4096,0x20080000);ids[1]=scene_storage(s,33554432,0x40001000);
 ids[2]=scene_storage(s,33554432,0x30100000);ids[3]=scene_storage(s,(u64)layout.allocation_pages*4096,0x43000000);
 ids[4]=scene_storage(s,((u64)pitch*height*4+4095)&~4095ULL,0x44000000);
 ids[5]=scene_storage(s,((u64)pitch*((height+31)&~31u)*4+4095)&~4095ULL,0x45000000);
 for(unsigned i=6;i<17;i++)ids[i]=scene_storage(s,4096,i==7?0x34000000:0x20090000+(i-4)*4096);
 for(unsigned i=0;i<17;i++){struct backing *b=&s->bo[ids[i]&31];uses[i]=(struct remesa_memory_binding){ids[i],b->va.start,b->gem.size};}
 if(fault==1){
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_LEASE,.context=77,.id=ids[3],.flags=1};
  CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_UNLEASE;r.flags=0;CHECK(!remesa_gem_memory_request(s,&file,&r));
 }
 command.scene=(struct remesa_gem_scene_action_command){REMESA_GEM_SCENE_ACTION_SCHEMA,sizeof(command.scene),width,height,ids[3],ids[1],ids[2],0};
 command.controls=(struct remesa_gem_scene_control_command){
  .schema=1,.bytes=sizeof(command.controls),.width=width,.height=height,
  .color_pitch=pitch,.depth_pitch=pitch,.depth_cpp=4,.clear_depth_bits=0x3e800000,
  .color_id=ids[4],.depth_id=ids[5],.ta_id=ids[6],.ta_offset=4,.ta_bytes=68,
  .background_id=ids[7],.background_offset=16,.background_bytes=68,
  .event_id=ids[8],.event_offset=32,.event_bytes=148};
 command.use=(struct remesa_gem_scene_use_command){1,sizeof(command.use)+10*sizeof(command.entries[0]),10,0};
 command.entries[0]=(struct remesa_gem_scene_use_entry){ids[9],32,16,4,0,0};
 command.entries[1]=(struct remesa_gem_scene_use_entry){ids[9],16,8,3,1,0};
 command.entries[2]=(struct remesa_gem_scene_use_entry){ids[10],64,16,4,0,0};
 command.entries[3]=(struct remesa_gem_scene_use_entry){ids[13],16,16,4,0,0};
 command.entries[4]=(struct remesa_gem_scene_use_entry){ids[9],0,16,4,0,0};
 command.entries[5]=(struct remesa_gem_scene_use_entry){ids[14],0,64,4,0,0};
 command.entries[6]=(struct remesa_gem_scene_use_entry){ids[14],64,8,3,1,0};
 command.entries[7]=(struct remesa_gem_scene_use_entry){ids[16],0,8,3,1,0};
 command.entries[8]=(struct remesa_gem_scene_use_entry){ids[14],96,16,3,1,0};
 command.entries[9]=(struct remesa_gem_scene_use_entry){ids[14],128,8,3,1,0};
 command.vertex=(struct remesa_gem_scene_vertex_command){1,sizeof(command.vertex)+2*sizeof(command.windows[0]),2,0};
 command.windows[0]=(struct remesa_gem_scene_vertex_window){ids[9],128,6,ids[9],64,48,3,0};
 command.windows[1]=(struct remesa_gem_scene_vertex_window){ids[11],144,6,ids[11],0,144,3,0};
 if(fault==14){
  struct remesa_gem_scene_job *context=calloc(1,sizeof(*context));CHECK(context);
  struct remesa_gem_scene_sections sections={4,control_offset,use_offset,vertex_offset},bad_sections=sections;
  struct remesa_gem_private_rule prepare_rules[17];
  for(unsigned k=0;k<17;k++)prepare_rules[k]=(struct remesa_gem_private_rule){ids[k],k&&k<=5?3:1,k!=0};
  dpm_model=true;scene_output_roles=true;capacity_gpu_count=17;mode=0;
  unsigned before_zero=test_zeroed_pages;int before_claimed=claimed;
  bad_sections.controls++;
  CHECK(remesa_gem_scene_job_prepare(s,uses,17,&command,sizeof(command),&bad_sections,prepare_rules,&guard,context)==-EINVAL);
  CHECK(!context->memory&&context->state==REMESA_SCENE_JOB_EMPTY&&claimed==before_claimed);
  bad_sections=sections;bad_sections.use++;
  CHECK(remesa_gem_scene_job_prepare(s,uses,17,&command,sizeof(command),&bad_sections,prepare_rules,&guard,context)==-EINVAL);
  CHECK(!context->memory&&claimed==before_claimed);
  for(unsigned iteration=0;iteration<2;iteration++){
   CHECK(!remesa_gem_scene_job_prepare(s,uses,17,&command,sizeof(command),&sections,prepare_rules,&guard,context));
   CHECK(context->state==REMESA_SCENE_JOB_PREPARED&&context->count==17&&context->loan.live&&!context->loan.host_published);
   CHECK(context->actions.load_count==29&&context->actions.ta_count==31&&context->actions.raster_count==20&&context->use.entry_count==10);
   CHECK(!context->views[1].cpu&&!context->views[2].cpu&&!context->views[3].cpu&&!context->views[4].cpu&&!context->views[5].cpu&&context->views[9].cpu);
   CHECK(remesa_gem_scene_job_prepare(s,uses,17,&command,sizeof(command),&sections,prepare_rules,&guard,context)==-EBUSY);
   scene_images(context->views,uses,&context->use,width,height,pitch);
   mutex_lock(&s->lock);
   CHECK(!remesa_gem_scene_job_bind_service_locked(context));
   CHECK(context->service_plan.binding_count==17&&context->service.plan==&context->service_plan&&
    context->service.session==&context->session&&context->session.phase==SGX535_PHASE_HOST_IMAGE_FINAL);
   CHECK(remesa_gem_scene_job_bind_service_locked(context)==-EALREADY);
   mutex_unlock(&s->lock);
   scene_service_cases(context);
   scene_backend_cases(context);
   context->service.started=1;CHECK(remesa_gem_scene_job_cancel(context)==-EBUSY&&context->loan.live);
   context->service.started=0; /* Model only: no actual service ran. */
   context->loan.execution_attempted=true;
   CHECK(remesa_gem_scene_job_cancel(context)==-EBUSY&&context->loan.live&&context->memory==s);
   context->loan.execution_attempted=false; /* Model only: no execution occurred. */
   CHECK(!remesa_gem_scene_job_cancel(context)&&!context->memory&&claimed==before_claimed);
  }
  CHECK(test_zeroed_pages==before_zero);free(context);
  dpm_model=false;scene_output_roles=false;capacity_gpu_count=0;
  CHECK(!remesa_gem_memory_close(s));CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);expected_memory=NULL;expected_guard=NULL;return;
 }
 CHECK(!remesa_gem_job_prepare(s,uses,17,&command,sizeof(command),&ticket));struct retained_job *j=ticket_job(s,ticket);
 command.scene.width=0; /* Only the copied kernel section is authoritative. */
 command.entries[0].bank=99;
 memset(&plan,0xa5,sizeof plan);saved=plan;
 CHECK(remesa_gem_scene_actions_prepare(s,ticket,4,&plan)==-EACCES&&!memcmp(&plan,&saved,sizeof plan)&&!j->region_mask);
 CHECK(!remesa_gem_job_claim_zero_dpm(s,ticket,ids[1],ids[2]));
 bad_ticket=ticket;bad_ticket.generation++;
 CHECK(remesa_gem_scene_actions_prepare(s,bad_ticket,4,&plan)==-ENOENT&&!memcmp(&plan,&saved,sizeof plan));
 CHECK(remesa_gem_scene_actions_prepare(s,ticket,1,&plan)==-EINVAL&&!memcmp(&plan,&saved,sizeof plan));
 unsigned maps_before=vmap_calls,flush_before=flushes,zero_before=test_zeroed_pages;
 int error=remesa_gem_scene_actions_prepare(s,ticket,4,&plan);
 CHECK(vmap_calls==maps_before&&flushes==flush_before&&test_zeroed_pages==zero_before);
 if(fault==1){CHECK(error==-ENODATA&&!memcmp(&plan,&saved,sizeof plan)&&!j->region_mask);goto cleanup;}
 CHECK(!error&&plan.binding_count==17&&plan.command.width==width&&plan.command.height==height&&j->region_mask==8);
 CHECK(plan.load_count==29&&plan.ta_count==31&&plan.raster_count==20);
 CHECK(plan.load[22].offset==0x118&&plan.load[22].value==15&&plan.load[22].mask==15);
 CHECK(plan.ta[0].offset==0x220&&plan.ta[0].value==0x43000000+layout.cookie[8]);
 CHECK(plan.ta[30].offset==0x200&&plan.ta[30].value==1);
 CHECK(plan.raster[4].offset==0x408&&plan.raster[4].value==0x43000000+layout.cookie[7]);
 CHECK(plan.raster[19].offset==0x428&&plan.raster[19].value==1);
 saved=plan;CHECK(!remesa_gem_scene_actions_prepare(s,ticket,4,&plan)&&!memcmp(&plan,&saved,sizeof plan));
 memset(&controls,0xa5,sizeof controls);control_saved=controls;
 CHECK(remesa_gem_scene_controls_prepare(s,ticket,4,control_offset+1,&controls)==-EINVAL&&!memcmp(&controls,&control_saved,sizeof controls));
 CHECK(!remesa_gem_scene_controls_prepare(s,ticket,4,control_offset,&controls));
 CHECK(vmap_calls==maps_before&&flushes==flush_before&&test_zeroed_pages==zero_before);
 struct remesa_raster_control_request raster={.color={width,height,pitch,0x44000000,s->bo[ids[4]&31].gem.size},
  .has_depth=true,.depth={.depth={width,height,pitch,4,REMESA_DEPTH_SURFACE_UNDEFINED,0x45000000,s->bo[ids[5]&31].gem.size},
   .color_width=width,.color_height=height,.clear_depth=0.25f},
  .background={0x34000000,4096,16,68},.event={uses[8].va,4096,32,148},.event_field0c=16};
 u8 raster_bytes[208],ta_bytes[56];
 struct remesa_raster_object_reference ta={uses[6].va,4096,4,80};
 CHECK(!remesa_raster_control_encode(&raster,raster_bytes)&&!memcmp(raster_bytes,controls.raster,208));
 CHECK(!remesa_ta_control_encode(&ta,ta_bytes)&&!memcmp(ta_bytes,controls.ta,56));
 struct remesa_gem_scene_control_command *copy=(void *)((u8 *)j->commands+control_offset),copy_saved=*copy;
 control_saved=controls;
 for(unsigned negative=0;negative<12;negative++){
  *copy=copy_saved;
  if(negative==0)copy->clear_depth_bits=0x7fc00000;
  if(negative==1)copy->color_pitch=width-1;
  if(negative==2)copy->height++;
  if(negative==3)copy->background_id=ids[3];
  if(negative==4)copy->event_bytes=4096;
  if(negative==5)copy->background_offset=1;
  if(negative==6)copy->depth_id=copy->color_id;
  if(negative==7)copy->clear_depth_bits=0x7f800000;
  if(negative==8)copy->clear_depth_bits=0xbe800000;
  if(negative==9)copy->clear_depth_bits=0x3f800001;
  if(negative==10)copy->depth_cpp=3;
  if(negative==11)copy->color_pitch=2048;
  CHECK(remesa_gem_scene_controls_prepare(s,ticket,4,control_offset,&controls)<0&&!memcmp(&controls,&control_saved,sizeof controls));
 }
 const u32 clear_cases[]={0,0x80000000U,1,0x3f800000};
 for(unsigned n=0;n<4;n++){
  *copy=copy_saved;copy->clear_depth_bits=clear_cases[n];copy->depth_cpp=(n&1)?2:4;
  raster.depth.depth.cpp=copy->depth_cpp;memcpy(&raster.depth.clear_depth,&copy->clear_depth_bits,4);
  CHECK(!remesa_gem_scene_controls_prepare(s,ticket,4,control_offset,&controls));
  CHECK(!remesa_raster_control_encode(&raster,raster_bytes)&&!memcmp(raster_bytes,controls.raster,208));
 }
 *copy=copy_saved;
 CHECK(!remesa_gem_scene_controls_prepare(s,ticket,4,control_offset,&controls)&&!memcmp(&controls,&control_saved,sizeof controls));
 memset(&use,0xa5,sizeof use);use_saved=use;
 CHECK(remesa_gem_scene_use_prepare(s,ticket,4,control_offset,use_offset+1,&use)==-EINVAL&&!memcmp(&use,&use_saved,sizeof use));
 CHECK(!remesa_gem_scene_use_prepare(s,ticket,4,control_offset,use_offset,&use)&&use.entry_count==10&&use.bank_mask==24);
 CHECK(vmap_calls==maps_before&&flushes==flush_before&&test_zeroed_pages==zero_before);
 use_saved=use;
 struct remesa_gem_scene_use_command *use_copy=(void *)((u8 *)j->commands+use_offset),use_header=*use_copy;
 struct remesa_gem_scene_use_entry *entries=(void *)(use_copy+1),entry_saved=entries[0];
 for(unsigned negative=0;negative<8;negative++){
  entries[0]=entry_saved;
  if(negative==0)entries[0].bank=16;
  if(negative==1)entries[0].data_master=3;
  if(negative==2)entries[0].id=ids[3];
  if(negative==3)entries[0].byte_offset=1;
  if(negative==4)entries[0].program_bytes=4097;
  if(negative==5)entries[0].data_master=1; /* Same bank4 cannot retain VS and PX. */
  if(negative==6)entries[0].reserved=1;
  if(negative==7)entries[0].id+=32; /* Stale generation, not the next live BO. */
  CHECK(remesa_gem_scene_use_prepare(s,ticket,4,control_offset,use_offset,&use)<0&&!memcmp(&use,&use_saved,sizeof use));
 }
 entries[0]=entry_saved;
 use_copy->entry_count=REMESA_SCENE_USE_MAX;use_copy->bytes=sizeof(*use_copy)+use_copy->entry_count*sizeof(*entries);
 for(unsigned i=10;i<REMESA_SCENE_USE_MAX;i++)entries[i]=entry_saved;
 CHECK(!remesa_gem_scene_use_prepare(s,ticket,4,control_offset,use_offset,&use)&&use.entry_count==REMESA_SCENE_USE_MAX);
 use_copy->entry_count++;use_copy->bytes+=sizeof(*entries);use_bad=use;
 CHECK(remesa_gem_scene_use_prepare(s,ticket,4,control_offset,use_offset,&use)==-EINVAL&&!memcmp(&use,&use_bad,sizeof use));
 *use_copy=use_header;
 CHECK(!remesa_gem_scene_use_prepare(s,ticket,4,control_offset,use_offset,&use)&&!memcmp(&use,&use_saved,sizeof use));
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,17,9)==-EACCES);
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,17,0x1ffc1)==17&&vmap_calls==maps_before+12);
 CHECK(!views[1].cpu&&!views[2].cpu&&!views[3].cpu);
 scene_images(views,uses,&use,width,height,pitch);
 u8 *stream=(u8 *)views[6].cpu+4,stream_saved[68];memcpy(stream_saved,stream,68);
 u8 *prefix=views[9].cpu,*draw=views[10].cpu,*close=views[13].cpu,*indices=views[11].cpu;
 u8 *bg=(u8 *)views[7].cpu+16,*bp=(u8 *)views[16].cpu+32,*ep=(u8 *)views[8].cpu+32,*target=views[15].cpu;
 u8 *transfer=(u8 *)views[14].cpu+96,*end_aux=(u8 *)views[14].cpu+128;
 struct remesa_gem_private_rule rules[17];
 scene_readonly_role=fault>=2&&fault<=4?(fault==2?3:fault==3?1:2):fault==6?4:fault==7?5:0;
 scene_uncached_role=fault==5?3:fault==8?6:fault==10?9:0;scene_write_role=fault==9?9:(fault==11||fault==12)?15:0;
 for(unsigned i=0;i<17;i++)rules[i]=(struct remesa_gem_private_rule){ids[i],(i&&i<=5&&i!=scene_readonly_role)||(scene_write_role&&i==scene_write_role)?3:1,i!=0&&i!=scene_uncached_role};
 dpm_model=true;scene_output_roles=true;capacity_gpu_count=17;mode=0;
 CHECK(!remesa_gem_private_prepare(s,ticket,rules,17,&guard,&loan));
 mutex_lock(&s->lock);
 if(fault>=2&&fault<=5){CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&plan)==-EACCES);goto unlock;}
 CHECK(!remesa_gem_scene_actions_check_locked(&loan,views,17,&plan));
 if(fault>=6&&fault<=8){CHECK(remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&controls)==-EACCES);goto unlock;}
 CHECK(!remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&controls));
 if(fault>=9&&fault<=10){CHECK(remesa_gem_scene_use_check_locked(&loan,views,17,&plan,&controls,&use)==-EACCES);goto unlock;}
 CHECK(!remesa_gem_scene_use_check_locked(&loan,views,17,&plan,&controls,&use));
 CHECK(!remesa_gem_scene_ta_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs));
 CHECK(ta_refs.draw_count==1&&ta_refs.reference_count==7&&ta_refs.ta_id==ids[6]&&ta_refs.ta_bytes==68);
 CHECK(ta_refs.references[0].id==ids[9]&&ta_refs.references[0].byte_offset==352&&
  ta_refs.references[4].id==ids[11]&&ta_refs.references[4].minimum_bytes==6&&ta_refs.references[6].id==ids[13]);
 ta_saved=ta_refs;
 CHECK(!remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)&&states.count==3);
 CHECK(states.uploads[0].source_id==ids[9]&&states.uploads[0].source_offset==304&&states.uploads[0].source_bytes==44&&
  states.uploads[0].use_id==ids[9]&&states.uploads[0].use_offset==32&&states.uploads[1].source_bytes==56&&states.uploads[2].source_bytes==8);
 states_saved=states;
 CHECK(!remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)&&vertices.count==2);
 CHECK(vertices.draws[0].vertex_bytes==48&&vertices.draws[0].stride_bytes==16&&vertices.draws[0].maximum_index==2&&vertices.draws[0].use_bytes==16&&
  vertices.draws[1].vertex_bytes==144&&vertices.draws[1].stride_bytes==48&&vertices.draws[1].minimum_index==0&&vertices.draws[1].maximum_index==2&&vertices.draws[1].use_bytes==64);
 vertices_saved=vertices;
 if(fault==11){
  state_word(draw+16,0x0c000000u|(((uses[15].va+192)>>4)&0xffffff));
  CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-EACCES);goto unlock;
 }
 CHECK(!remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)&&fragments.count==2&&fragments.expected_pds_window==0x20000000);
 CHECK(fragments.draws[0].primary_id==ids[9]&&fragments.draws[0].primary_offset==224&&fragments.draws[0].primary_bytes==56&&fragments.draws[0].use_id==ids[9]&&fragments.draws[0].use_offset==16&&
  fragments.draws[1].primary_id==ids[12]&&fragments.draws[1].primary_bytes==60&&fragments.draws[1].use_id==ids[14]&&fragments.draws[1].use_offset==64);
 fragments_saved=fragments;
 for(unsigned k=0;k<16;k++){
  prefix[k]^=1;CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-EINVAL&&!memcmp(&vertices,&vertices_saved,sizeof vertices));prefix[k]^=1;
 }
 for(unsigned k=0;k<64;k++){
  ((u8 *)views[14].cpu)[k]^=1;CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-EINVAL&&!memcmp(&vertices,&vertices_saved,sizeof vertices));((u8 *)views[14].cpu)[k]^=1;
 }
 for(unsigned k=0;k<8;k++){
  prefix[16+k]^=1;CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-EINVAL&&!memcmp(&fragments,&fragments_saved,sizeof fragments));prefix[16+k]^=1;
  ((u8 *)views[14].cpu)[64+k]^=1;CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-EINVAL&&!memcmp(&fragments,&fragments_saved,sizeof fragments));((u8 *)views[14].cpu)[64+k]^=1;
 }

 if(fault==12){
  state_word(bg+8,0x0c000000u|((uses[15].va>>4)&0xffffff));
  CHECK(remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background)==-EACCES);goto unlock;
 }
 CHECK(!remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background));
 CHECK(background.background_id==ids[7]&&background.background_offset==16&&background.primary_id==ids[16]&&background.primary_offset==32&&
       background.secondary_id==ids[16]&&background.secondary_offset==96&&background.color_id==ids[4]&&background.use_id==ids[16]&&background.use_offset==0);
 background_saved=background;
 if(fault==13){state_word(ep+8,uses[4].va);CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-EACCES);goto unlock;}
 CHECK(!remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event));
 CHECK(event.event_id==ids[8]&&event.event_offset==32&&event.target_id==ids[15]&&event.target_offset==0&&
       event.transfer_id==ids[14]&&event.transfer_offset==96&&event.end_id==ids[14]&&event.end_offset==128);event_saved=event;
 for(unsigned k=0;k<8;k++){
  ((u8 *)views[16].cpu)[k]^=1;
  CHECK(remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background)==-EINVAL&&!memcmp(&background,&background_saved,sizeof background));((u8 *)views[16].cpu)[k]^=1;
 }
 for(unsigned k=0;k<16;k++){
  transfer[k]^=1;CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-EINVAL&&!memcmp(&event,&event_saved,sizeof event));transfer[k]^=1;
 }
 for(unsigned k=0;k<8;k++){
  end_aux[k]^=1;CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-EINVAL&&!memcmp(&event,&event_saved,sizeof event));end_aux[k]^=1;
 }
 state_word(ep+8,uses[8].va+32+64);CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-EINVAL&&!memcmp(&event,&event_saved,sizeof event));state_word(ep+8,uses[15].va);
 state_word(ep+8,uses[15].va+4096-12);CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-ENOENT&&!memcmp(&event,&event_saved,sizeof event));state_word(ep+8,uses[15].va);
 ep[147]^=1;CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-EINVAL&&!memcmp(&event,&event_saved,sizeof event));ep[147]^=1;
 target[12]^=1;CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-EINVAL&&!memcmp(&event,&event_saved,sizeof event));target[12]^=1;
 state_word(target+4,uses[5].va);CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-EINVAL&&!memcmp(&event,&event_saved,sizeof event));state_word(target+4,uses[4].va);
 state_word(ep,use.lower19[0]);CHECK(remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)==-ENOENT&&!memcmp(&event,&event_saved,sizeof event));state_word(ep,use.lower19[9]);
 ep[24]^=1;CHECK(!remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)&&!memcmp(&event,&event_saved,sizeof event));ep[24]^=1;

 for(unsigned negative=0;negative<6;negative++){
  unsigned at=negative==0?4:negative==1?20:negative==2?44:negative==3?64:negative==4?32+48:32+64;
  u8 *target=negative<4?bg:(u8 *)views[16].cpu;target[at]^=1;
  CHECK(remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background)==-EINVAL&&!memcmp(&background,&background_saved,sizeof background));target[at]^=1;
 }
 state_word(bp+40,uses[5].va);
 CHECK(remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background)==-EACCES&&!memcmp(&background,&background_saved,sizeof background));state_word(bp+40,uses[4].va);
 state_word(bp,0x100000u|use.lower19[0]);
 CHECK(remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background)==-ENOENT&&!memcmp(&background,&background_saved,sizeof background));state_word(bp,0x100000u|use.lower19[7]);
 bp[16]^=1;CHECK(!remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background)&&!memcmp(&background,&background_saved,sizeof background));bp[16]^=1;

 prefix[272]^=1;CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-EINVAL&&!memcmp(&fragments,&fragments_saved,sizeof fragments));prefix[272]^=1;
 prefix[288]^=1;CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-EINVAL&&!memcmp(&fragments,&fragments_saved,sizeof fragments));prefix[288]^=1;
 state_word(draw+16,0x0c000000u|((uses[4].va>>4)&0xffffff));
 CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-ENOENT&&!memcmp(&fragments,&fragments_saved,sizeof fragments));state_word(draw+16,0x0c000000u|(((uses[12].va+192)>>4)&0xffffff));
 state_word(prefix+224,use.lower19[0]);CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-ENOENT&&!memcmp(&fragments,&fragments_saved,sizeof fragments));state_word(prefix+224,use.lower19[1]);
 ((u8 *)views[12].cpu)[228]^=1;CHECK(remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)==-EINVAL&&!memcmp(&fragments,&fragments_saved,sizeof fragments));((u8 *)views[12].cpu)[228]^=1;
 prefix[232]^=1;CHECK(!remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)&&!memcmp(&fragments,&fragments_saved,sizeof fragments));prefix[232]^=1;
 struct remesa_gem_scene_vertex_window *prefix_window=(void *)((u8 *)j->commands+vertex_offset+sizeof(command.vertex));
 prefix_window->vertex_count=4;prefix_window->vertex_bytes=64;
 CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-EINVAL&&!memcmp(&vertices,&vertices_saved,sizeof vertices));
 prefix_window->vertex_count=3;prefix_window->vertex_bytes=48;
 indices[144]=3;CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-ERANGE&&!memcmp(&vertices,&vertices_saved,sizeof vertices));indices[144]=0;
 prefix[208]^=1;CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-EINVAL&&!memcmp(&vertices,&vertices_saved,sizeof vertices));prefix[208]^=1;
 struct remesa_gem_scene_vertex_window *windows=(void *)((u8 *)j->commands+vertex_offset+sizeof(command.vertex)),window_saved=windows[1];
 windows[1].vertex_bytes=4096;CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-ERANGE&&!memcmp(&vertices,&vertices_saved,sizeof vertices));windows[1]=window_saved;
 windows[1].vertex_id=ids[4];CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-EACCES&&!memcmp(&vertices,&vertices_saved,sizeof vertices));windows[1]=window_saved;
 state_word(views[12].cpu,uses[11].va+4);CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-ERANGE&&!memcmp(&vertices,&vertices_saved,sizeof vertices));state_word(views[12].cpu,uses[11].va);
 ((u8 *)views[12].cpu)[20]^=1;CHECK(remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)==-EINVAL&&!memcmp(&vertices,&vertices_saved,sizeof vertices));((u8 *)views[12].cpu)[20]^=1;
 ((u8 *)views[12].cpu)[8]^=1;CHECK(!remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)&&!memcmp(&vertices,&vertices_saved,sizeof vertices));((u8 *)views[12].cpu)[8]^=1;
 prefix[400]^=1;CHECK(remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)==-EINVAL&&!memcmp(&states,&states_saved,sizeof states));prefix[400]^=1;
 state_word(prefix+352,uses[4].va);CHECK(remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)==-EACCES&&!memcmp(&states,&states_saved,sizeof states));state_word(prefix+352,uses[9].va+304);
 state_word(prefix+360,0x27ffff);CHECK(remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)==-ENOENT&&!memcmp(&states,&states_saved,sizeof states));state_word(prefix+360,0x200000u|use.lower19[0]);
 prefix[32]^=1;CHECK(remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)==-EINVAL&&!memcmp(&states,&states_saved,sizeof states));prefix[32]^=1;
 close[4]^=1;CHECK(remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)==-EINVAL&&!memcmp(&states,&states_saved,sizeof states));close[4]^=1;
 prefix[364]^=1;CHECK(!remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)&&!memcmp(&states,&states_saved,sizeof states));prefix[364]^=1;
 stream[67]=0;CHECK(remesa_gem_scene_ta_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs)==-EINVAL&&!memcmp(&ta_refs,&ta_saved,sizeof ta_refs));memcpy(stream,stream_saved,68);
 u32 forbidden=0x40000000u|(uses[4].va>>4);memcpy(stream,&forbidden,4);
 CHECK(remesa_gem_scene_ta_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs)==-EACCES&&!memcmp(&ta_refs,&ta_saved,sizeof ta_refs));memcpy(stream,stream_saved,68);
 forbidden=0x45000000u;memcpy(stream,&forbidden,4);
 CHECK(remesa_gem_scene_ta_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs)==-ENOENT&&!memcmp(&ta_refs,&ta_saved,sizeof ta_refs));memcpy(stream,stream_saved,68);
 struct remesa_memory_view missing[17];memcpy(missing,views,sizeof missing);missing[6].cpu=NULL;
 CHECK(remesa_gem_scene_ta_check_locked(&loan,missing,17,&plan,&controls,&use,&ta_refs)==-EACCES&&!memcmp(&ta_refs,&ta_saved,sizeof ta_refs));
 use_bad=use;use_bad.lower19[0]^=1;CHECK(remesa_gem_scene_use_check_locked(&loan,views,17,&plan,&controls,&use_bad)==-EACCES);
 use_bad=use;use_bad.banks[4].word^=1;CHECK(remesa_gem_scene_use_check_locked(&loan,views,17,&plan,&controls,&use_bad)==-EACCES);
 use_bad=use;use_bad.control_offset+=4;CHECK(remesa_gem_scene_use_check_locked(&loan,views,17,&plan,&controls,&use_bad)==-EACCES);
 control_bad=controls;control_bad.raster[39]=0x3f800000;CHECK(remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&control_bad)==-EACCES);
 control_bad=controls;control_bad.ta[13]+=4;CHECK(remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&control_bad)==-EACCES);
 control_bad=controls;control_bad.action_offset+=4;CHECK(remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&control_bad)==-EACCES);
 bad=plan;bad.ta[30].value=0;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bad=plan;bad.load[22].value=7;bad.load[22].mask=7;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bad=plan;bad.raster[19].offset=0;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bad=plan;bad.ticket.operation++;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bound_error=-EIO;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&plan)==-EIO);bound_error=0;
 CHECK(!remesa_gem_private_publish_locked(&loan,views,17));
 CHECK(!s->bo[ids[3]&31].zero_pristine&&!remesa_gem_scene_actions_check_locked(&loan,views,17,&plan));
 CHECK(!remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&controls));
 CHECK(!remesa_gem_scene_use_check_locked(&loan,views,17,&plan,&controls,&use));
 CHECK(!remesa_gem_scene_ta_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs)&&!memcmp(&ta_refs,&ta_saved,sizeof ta_refs));
 CHECK(!remesa_gem_scene_states_check_locked(&loan,views,17,&plan,&controls,&use,&ta_refs,&states)&&!memcmp(&states,&states_saved,sizeof states));
 CHECK(!remesa_gem_scene_vertices_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices)&&!memcmp(&vertices,&vertices_saved,sizeof vertices));
 CHECK(!remesa_gem_scene_fragments_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments)&&!memcmp(&fragments,&fragments_saved,sizeof fragments));
 CHECK(!remesa_gem_scene_background_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background)&&!memcmp(&background,&background_saved,sizeof background));
 CHECK(!remesa_gem_scene_event_check_locked(&loan,views,17,&plan,&controls,&use,vertex_offset,&ta_refs,&states,&vertices,&fragments,&background,&event)&&!memcmp(&event,&event_saved,sizeof event));
 unlock:mutex_unlock(&s->lock);CHECK(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP);CHECK(!remesa_gem_private_release(&loan));
 dpm_model=false;scene_output_roles=false;capacity_gpu_count=0;scene_readonly_role=scene_uncached_role=scene_write_role=0;
 cleanup:CHECK(!remesa_gem_job_cancel(s,ticket));CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);expected_memory=NULL;expected_guard=NULL;
}
int main(void)
{
 action_case(1280,800,0);action_case(640,640,0);action_case(641,479,0);action_case(1600,768,0);
 action_case(1280,800,14);action_case(641,479,14);action_case(1600,768,14);
 for(unsigned fault=1;fault<=14;fault++)action_case(640,640,fault);
 CHECK(scene_service_model_runs==208&&backend_model_runs==144);
 puts("PASS bound scene actions+controls:208 general-service protocol cases;17 held resources/four dimensions, original29/31/20 groups and raster208/TA56 match Mesa, immutable sections, cold scratch/no CPU aliases, strict wait15, clear IEEE/depth2+4, bounds/tamper/RW/cache/MMU refusal, publication and no native issue; GPU0");return 0;
}
