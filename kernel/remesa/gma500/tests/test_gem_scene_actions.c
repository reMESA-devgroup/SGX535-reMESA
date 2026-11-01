/* SPDX-License-Identifier: GPL-2.0-only. Held native action binding; no MMIO. */
#define REMESA_GEM_PRIVATE_TEST_MAIN prior_scene_action_fixture
#include "test_gem_private.c"
#include "../remesa_gem_scene_actions.h"
#include "../remesa_gem_scene_controls.h"
#include "remesa_render_target.h"
#include "remesa_scene_layout.h"
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
 struct {u32 prefix;struct remesa_gem_scene_action_command scene;
  struct remesa_gem_scene_control_command controls;u32 tail;} command={0};
 const unsigned control_offset=(u8 *)&command.controls-(u8 *)&command;
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
  .color_id=ids[4],.depth_id=ids[5],.ta_id=ids[6],.ta_offset=4,.ta_bytes=80,
  .background_id=ids[7],.background_offset=16,.background_bytes=68,
  .event_id=ids[8],.event_offset=32,.event_bytes=148};
 CHECK(!remesa_gem_job_prepare(s,uses,17,&command,sizeof(command),&ticket));struct retained_job *j=ticket_job(s,ticket);
 command.scene.width=0; /* Only the copied kernel section is authoritative. */
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
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,17,9)==-EACCES);
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,17,0x1ffc1)==17&&vmap_calls==maps_before+12);
 CHECK(!views[1].cpu&&!views[2].cpu&&!views[3].cpu);
 struct remesa_gem_private_rule rules[17];
 scene_readonly_role=fault>=2&&fault<=4?(fault==2?3:fault==3?1:2):fault==6?4:fault==7?5:0;
 scene_uncached_role=fault==5?3:fault==8?6:0;
 for(unsigned i=0;i<17;i++)rules[i]=(struct remesa_gem_private_rule){ids[i],i&&i<=5&&i!=scene_readonly_role?3:1,i!=0&&i!=scene_uncached_role};
 dpm_model=true;scene_output_roles=true;capacity_gpu_count=17;mode=0;
 CHECK(!remesa_gem_private_prepare(s,ticket,rules,17,&guard,&loan));
 mutex_lock(&s->lock);
 if(fault>=2&&fault<=5){CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&plan)==-EACCES);goto unlock;}
 CHECK(!remesa_gem_scene_actions_check_locked(&loan,views,17,&plan));
 if(fault>=6){CHECK(remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&controls)==-EACCES);goto unlock;}
 CHECK(!remesa_gem_scene_controls_check_locked(&loan,views,17,&plan,&controls));
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
 unlock:mutex_unlock(&s->lock);CHECK(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP);CHECK(!remesa_gem_private_release(&loan));
 dpm_model=false;scene_output_roles=false;capacity_gpu_count=0;scene_readonly_role=scene_uncached_role=0;
 cleanup:CHECK(!remesa_gem_job_cancel(s,ticket));CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);expected_memory=NULL;expected_guard=NULL;
}
int main(void)
{
 action_case(1280,800,0);action_case(640,640,0);action_case(641,479,0);action_case(1600,768,0);
 for(unsigned fault=1;fault<=8;fault++)action_case(640,640,fault);
 puts("PASS bound scene actions+controls:17 held resources/four dimensions, original29/31/20 groups and raster208/TA56 match Mesa, immutable sections, cold scratch/no CPU aliases, strict wait15, clear IEEE/depth2+4, bounds/tamper/RW/cache/MMU refusal, publication and no native issue; GPU0");return 0;
}
