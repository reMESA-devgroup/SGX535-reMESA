/* SPDX-License-Identifier: GPL-2.0-only. Held native action binding; no MMIO. */
#define REMESA_GEM_PRIVATE_TEST_MAIN prior_scene_action_fixture
#include "test_gem_private.c"
#include "../remesa_gem_scene_actions.h"
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
 struct {u32 prefix;struct remesa_gem_scene_action_command scene;u32 tail;} command={0};
 CHECK(!remesa_scene_layout_build(width,height,0,&layout));
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&s));expected_memory=s;expected_guard=&guard;
 u32 ids[17];
 ids[0]=scene_storage(s,4096,0x20080000);ids[1]=scene_storage(s,33554432,0x40001000);
 ids[2]=scene_storage(s,33554432,0x30100000);ids[3]=scene_storage(s,(u64)layout.allocation_pages*4096,0x43000000);
 for(unsigned i=4;i<17;i++)ids[i]=scene_storage(s,4096,0x20090000+(i-4)*4096);
 for(unsigned i=0;i<17;i++){struct backing *b=&s->bo[ids[i]&31];uses[i]=(struct remesa_memory_binding){ids[i],b->va.start,b->gem.size};}
 if(fault==1){
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_LEASE,.context=77,.id=ids[3],.flags=1};
  CHECK(!remesa_gem_memory_request(s,&file,&r));r.action=REMESA_MEM_UNLEASE;r.flags=0;CHECK(!remesa_gem_memory_request(s,&file,&r));
 }
 command.scene=(struct remesa_gem_scene_action_command){REMESA_GEM_SCENE_ACTION_SCHEMA,sizeof(command.scene),width,height,ids[3],ids[1],ids[2],0};
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
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,17,9)==-EACCES);
 CHECK(remesa_gem_job_snapshot_selected(s,ticket,&snapshot,views,17,0x1fff1)==17&&vmap_calls==maps_before+14);
 CHECK(!views[1].cpu&&!views[2].cpu&&!views[3].cpu);
 struct remesa_gem_private_rule rules[17];
 scene_readonly_role=fault>=2&&fault<=4?(fault==2?3:fault==3?1:2):0;scene_uncached_role=fault==5?3:0;
 for(unsigned i=0;i<17;i++)rules[i]=(struct remesa_gem_private_rule){ids[i],i&&i<=3&&i!=scene_readonly_role?3:1,i!=0&&i!=scene_uncached_role};
 dpm_model=true;capacity_gpu_count=17;mode=0;
 CHECK(!remesa_gem_private_prepare(s,ticket,rules,17,&guard,&loan));
 mutex_lock(&s->lock);
 if(fault>=2){CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&plan)==-EACCES);goto unlock;}
 CHECK(!remesa_gem_scene_actions_check_locked(&loan,views,17,&plan));
 bad=plan;bad.ta[30].value=0;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bad=plan;bad.load[22].value=7;bad.load[22].mask=7;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bad=plan;bad.raster[19].offset=0;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bad=plan;bad.ticket.operation++;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&bad)==-EACCES);
 bound_error=-EIO;CHECK(remesa_gem_scene_actions_check_locked(&loan,views,17,&plan)==-EIO);bound_error=0;
 CHECK(!remesa_gem_private_publish_locked(&loan,views,17));
 CHECK(!s->bo[ids[3]&31].zero_pristine&&!remesa_gem_scene_actions_check_locked(&loan,views,17,&plan));
 unlock:mutex_unlock(&s->lock);CHECK(remesa_gem_job_issue(s,ticket)==-EOPNOTSUPP);CHECK(!remesa_gem_private_release(&loan));
 dpm_model=false;capacity_gpu_count=0;scene_readonly_role=scene_uncached_role=0;
 cleanup:CHECK(!remesa_gem_job_cancel(s,ticket));CHECK(!remesa_gem_memory_close(s));
 CHECK(!gems&&!pins&&!ptes&&!claimed&&!power);expected_memory=NULL;expected_guard=NULL;
}
int main(void)
{
 action_case(1280,800,0);action_case(640,640,0);action_case(641,479,0);action_case(1600,768,0);
 for(unsigned fault=1;fault<=5;fault++)action_case(640,640,fault);
 puts("PASS bound scene actions:17 held resources/four dimensions, original29/31/20 groups, immutable command section, cold scratch/no CPU aliases, strict wait15, tail tamper/RW/cache/MMU refusal, publication and no native issue; GPU0");return 0;
}
