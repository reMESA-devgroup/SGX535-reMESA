/* SPDX-License-Identifier: GPL-2.0-only. Actual exported Mesa READ images and
 * metadata in the real retained core. Kernel primitives/MMU/IO are modeled. */
#include "../../../../tools/remesa/scene_fixture.h"
static void replay_read(const char *root,const char *name,void *out,size_t bytes)
{
 char path[4096];CHECK(snprintf(path,sizeof path,"%s/%s",root,name)>0);FILE *fp=fopen(path,"rb");CHECK(fp);
 CHECK(fread(out,1,bytes,fp)==bytes&&fgetc(fp)==EOF);CHECK(!fclose(fp));
}
static u32 replay_id(const struct remesa_scene_fixture *f,const u32 *ids,u32 old)
{for(unsigned i=0;i<f->count;i++)if(f->bindings[i].id==old)return ids[i];CHECK(0);return 0;}
static void scene_replay(const char *root)
{
 struct remesa_scene_fixture image;struct remesa_scene_packet packet;
 replay_read(root,"placement.bin",&image,sizeof image);replay_read(root,"packet.bin",&packet,sizeof packet);
 CHECK(image.version==1&&(image.count==17||image.count==18)&&packet.binding_count==image.count&&remesa_scene_packet_valid(&packet));
 struct test_mmu mmu={.has_clflush=true,.pd={.driver=&driver}};
 struct drm_psb_private priv={&mmu,(void*)1,{0x28000000,32,0x80000000}};
 struct drm_device dev={&priv};struct remesa_gem_memory *s=NULL;
 struct remesa_twod_reservation guard={.device=&priv,.held=true};
 CHECK(!remesa_gem_memory_open(&dev,&file,packet.owner,&s));expected_memory=s;expected_guard=&guard;
 u32 ids[32]={0};struct remesa_gem_private_rule rules[32]={0};
 for(unsigned i=0;i<image.count;i++){
  CHECK(image.bindings[i].id==packet.bindings[i].id&&image.bindings[i].access==packet.bindings[i].access);
  ids[i]=scene_storage(s,image.bindings[i].bytes,image.bindings[i].address);
  rules[i]=(struct remesa_gem_private_rule){ids[i],packet.bindings[i].access,1};
  if(packet.bindings[i].access==1){
   char name[64];CHECK(snprintf(name,sizeof name,"bo-%u.bin",i)>0);
   unsigned char *bytes=malloc(image.bindings[i].bytes);CHECK(bytes);replay_read(root,name,bytes,image.bindings[i].bytes);
   struct remesa_memory_request r={.version=1,.action=REMESA_MEM_LEASE,.context=packet.owner,.id=ids[i],.flags=1};
   CHECK(!remesa_gem_memory_request(s,&file,&r));
   for(u64 offset=0;offset<image.bindings[i].bytes;offset+=4096){
    r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_WRITE,.context=packet.owner,.id=ids[i],.offset=offset,.length=4096};
    memcpy(r.data,bytes+offset,4096);CHECK(!remesa_gem_memory_request(s,&file,&r));
   }
   r=(struct remesa_memory_request){.version=1,.action=REMESA_MEM_UNLEASE,.context=packet.owner,.id=ids[i]};
   CHECK(!remesa_gem_memory_request(s,&file,&r));free(bytes);
  }
 }
 /* File-local handle namespaces differ. Remap metadata IDs only. Every GPU
  * address and every source image byte remains the producer's exact value. */
 for(unsigned i=0;i<packet.binding_count;i++)packet.bindings[i].id=ids[i];
 packet.actions.region_id=replay_id(&image,ids,packet.actions.region_id);
 packet.actions.table_id=replay_id(&image,ids,packet.actions.table_id);
 packet.actions.parameter_id=replay_id(&image,ids,packet.actions.parameter_id);
 packet.controls.color_id=replay_id(&image,ids,packet.controls.color_id);
 packet.controls.depth_id=replay_id(&image,ids,packet.controls.depth_id);
 packet.controls.ta_id=replay_id(&image,ids,packet.controls.ta_id);
 packet.controls.background_id=replay_id(&image,ids,packet.controls.background_id);
 packet.controls.event_id=replay_id(&image,ids,packet.controls.event_id);
 for(unsigned i=0;i<packet.use_count;i++)packet.uses[i].id=replay_id(&image,ids,packet.uses[i].id);
 for(unsigned i=0;i<packet.window_count;i++){
  packet.windows[i].index_id=replay_id(&image,ids,packet.windows[i].index_id);
  packet.windows[i].vertex_id=replay_id(&image,ids,packet.windows[i].vertex_id);
 }
 CHECK(remesa_scene_packet_valid(&packet));dpm_model=true;capacity_gpu_count=image.count;replay_rules=rules;
 struct remesa_gem_scene_job *c=calloc(1,sizeof(*c));CHECK(c);
 int error=remesa_gem_scene_packet_prepare(s,&packet,&guard,c);
 printf("Mesa producer replay %ux%u: prepare=%d\n",packet.actions.width,packet.actions.height,error);fflush(stdout);CHECK(!error);
 execute_context=c;execute_phase=execute_stages=execute_samples=execute_flushes=0;
 struct sgx535_gma500_fixed_backend backend={0};struct remesa_stream_observation observation={0};
 test_pfn_hook=execute_default_pfn;test_flush_hook=execute_flush;
 error=remesa_gem_scene_job_execute(c,&backend,&observation,s,10,20);
 printf("Mesa producer replay %ux%u: execute=%d phase=%u ledger=%u stage=%u\n",packet.actions.width,packet.actions.height,error,c->session.phase,c->session.observed_events,execute_stages);fflush(stdout);
 CHECK(!error&&execute_phase==9&&execute_stages==19&&c->loan.execution_retired);
 test_pfn_hook=NULL;test_flush_hook=NULL;
 CHECK(!remesa_gem_scene_job_retire(c)&&!c->memory);free(c);
 CHECK(!remesa_gem_memory_close(s)&&!gems&&!pins&&!ptes&&!claimed&&!power);
 puts("PASS actual Mesa producer bytes through kernel closure, ordered executor and retirement; hardware callbacks modeled, GPU0");
}
