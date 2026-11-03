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
static bool replay_pds_reserved(unsigned kind,unsigned byte)
{
 unsigned w=byte/4;
 switch(kind){
 case 0:return byte>=12&&byte<48;
 case 1:return byte>=16&&byte<48;
 case 2:return w>=2&&w<12&&w!=4&&w!=8;
 case 3:return w>=2&&w<12&&w!=4&&w!=5&&w!=8&&w!=9;
 case 4:return w>=2&&w<12&&w!=8;
 case 5:return w>=2&&w<12&&w!=8&&w!=9;
 case 6:return (w>=4&&w<8)||w==11;
 case 7:return w==6||w==7||w==15;
 default:CHECK(0);return false;
 }
}
static void replay_pds_images(struct remesa_gem_scene_job *c)
{
 struct {u8 *bytes;unsigned length,kind;} spans[REMESA_GEM_SCENE_STATE_MAX+2*REMESA_GEM_SCENE_VERTEX_MAX+2];
 unsigned count=0,n,r,mutations=0,maps=vmap_calls,clears=test_zeroed_pages,flush=flushes;
 CHECK(!remesa_gem_scene_job_pds_images_check_locked(c));
#define ADD_PDS(id,offset,length_,kind_) do { \
  CHECK(count<sizeof spans/sizeof spans[0]); \
  spans[count].bytes=(u8 *)scene_pds_image_view(c,id,offset,length_);CHECK(spans[count].bytes); \
  spans[count].length=length_;spans[count++].kind=kind_; \
 } while(0)
 for(n=0;n<c->states.count;n++)ADD_PDS(c->states.uploads[n].pds_id,c->states.uploads[n].pds_offset,60,
  c->states.uploads[n].source_bytes==160?1u:0u);
 for(n=0;n<c->vertices.count;n++){
  for(r=0;r<c->ta.reference_count;r++)if(c->ta.references[r].draw==n&&
     c->ta.references[r].kind==(n?REMESA_TA_REF_DRAW_VERTEX:REMESA_TA_REF_PREFIX_VERTEX))break;
  CHECK(r<c->ta.reference_count);ADD_PDS(c->ta.references[r].id,c->ta.references[r].byte_offset,64,n?3u:2u);
 }
 for(n=0;n<c->fragments.count;n++)ADD_PDS(c->fragments.draws[n].primary_id,c->fragments.draws[n].primary_offset,
  c->fragments.draws[n].primary_bytes,n?5u:4u);
 ADD_PDS(c->background.primary_id,c->background.primary_offset,64,6u);
 ADD_PDS(c->event.event_id,c->event.event_offset,148,7u);
#undef ADD_PDS
 for(n=0;n<count;n++)for(unsigned byte=0;byte<spans[n].length;byte++)if(replay_pds_reserved(spans[n].kind,byte)){
  spans[n].bytes[byte]^=1;
  CHECK(remesa_gem_scene_job_pds_images_check_locked(c)==-EINVAL);
  if(spans[n].kind!=1||byte<40||byte>=44)
   CHECK(!remesa_gem_scene_event_check_locked(&c->loan,c->views,c->count,&c->actions,&c->controls,&c->use,c->sections.vertices,
    &c->ta,&c->states,&c->vertices,&c->fragments,&c->background,&c->event));
  spans[n].bytes[byte]^=1;mutations++;
 }
 CHECK(!remesa_gem_scene_job_pds_images_check_locked(c));
 c->snapshot.ticket.operation++;CHECK(remesa_gem_scene_job_pds_images_check_locked(c)==-EACCES);c->snapshot.ticket.operation--;
 c->loan.host_published=true;CHECK(remesa_gem_scene_job_pds_images_check_locked(c)==-EINVAL);c->loan.host_published=false;
 void *saved=c->views[0].cpu;c->views[0].cpu=NULL;
 CHECK(remesa_gem_scene_job_pds_images_check_locked(c)<0);c->views[0].cpu=saved;
 CHECK(!remesa_gem_scene_job_pds_images_check_locked(c));
 CHECK(!c->service.session&&!c->service.started&&!c->loan.execution_started&&!c->loan.host_published);
 CHECK(vmap_calls==maps&&test_zeroed_pages==clears&&flushes==flush);
 printf("PASS complete PDS images: %u actual producer objects, %u reserved-byte mutations, stale/published/view refusal, no map/clear/flush/issue; GPU0\n",count,mutations);
}
static void scene_replay(const char *root,bool check_pds)
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
 if(check_pds){mutex_lock(&s->lock);replay_pds_images(c);mutex_unlock(&s->lock);}
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
