/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual memory/seal/patch implementation; prior retirement and a data-only
 * policy are injected assumptions. No source or GPU callback is permitted. */
#define main old_input_fixture
#include "test_private_inputs.c"
#undef main
static u32 scene_ids[19];
static struct remesa_memory_binding scene_uses[18];
static const u32 read_mask=0x1fff;
static struct remesa_memory_ticket scene_job(void)
{
 struct remesa_memory_ticket t={0};u32 command=0;
 CHECK(!remesa_gem_job_prepare(memory,scene_uses,18,&command,4,&t));
 CHECK(!remesa_gem_job_claim_zero_dpm(memory,t,scene_ids[16],scene_ids[15]));
 return t;
}
static int scene_seal(struct remesa_memory_ticket t,u32 mask)
{return remesa_private_session_seal_scene_inputs(memory,&token,t,mask);}
struct patch_policy {u32 id;};
static struct patch_policy policy;
static unsigned validations;
static int data_policy(const void *opaque,u32 id,u64 offset,const void *data,unsigned bytes)
{
 const struct patch_policy *p=opaque;const u8 *bits=data;validations++;
 if(id!=p->id||offset!=4||bytes!=144)return -EACCES;
 /* Test admission mirrors the existing retained finite/bounded float rule.
  * It is not registration of a new executable or general-session policy. */
 for(unsigned i=0;i<bytes;i+=4){
  u32 word=bits[i]|((u32)bits[i+1]<<8)|((u32)bits[i+2]<<16)|((u32)bits[i+3]<<24);
  if((word&0x7fffffffU)>0x44800000U)return -ERANGE;
 }
 return 0;
}
static unsigned char replacement[144];
static struct page **patch_pages;
static void patch_flush(struct page **pages,unsigned count)
{CHECK(pages==patch_pages&&count==1);}
static int patch(u32 id,u64 offset,const void *data,unsigned bytes)
{return remesa_private_session_patch_scene_input(memory,&token,42,id,offset,data,bytes,&policy,data_policy);}
static void patch_setup(void)
{
 CHECK(!remesa_gem_memory_open(&dev,&file,77,&memory));endpoint=memory;
 for(unsigned i=0;i<19;i++){
  u64 bytes=i==15||i==16?33554432:4096;
  u64 va=i==15?0x30100000:i==16?0x50000000:0x20001000+i*4096;
  struct remesa_memory_request r={.version=1,.action=REMESA_MEM_CREATE,.context=77,.bytes=bytes};
  CHECK(!remesa_gem_memory_request(memory,&file,&r));scene_ids[i]=r.id;
  CHECK(!request(REMESA_MEM_PIN,r.id,0,0,NULL,0));r.action=REMESA_MEM_MAP;r.va=va;r.bytes=0;
  CHECK(!remesa_gem_memory_request(memory,&file,&r));
  if(i<18)scene_uses[i]=(struct remesa_memory_binding){r.id,va,bytes};
 }
 struct remesa_gem_memory *out=NULL;
 CHECK(!remesa_private_session_acquire(&dev,&file,77,&token,&out));
 struct remesa_memory_ticket t=scene_job();
 CHECK(!remesa_private_session_seal_dpm(memory,&token,t,scene_ids[16],scene_ids[15]));
 CHECK(!scene_seal(t,read_mask)&&!remesa_gem_job_cancel(memory,t));
 /* Inject a previously acknowledged hardware epoch, not simulated GPU
  * execution. The physical source/executor chain is tested separately. */
 memory->private_source_held=true;memory->private_dpm_ready=true;
 memory->private_dpm_retired_operation=42;
 memory->bo[scene_ids[15]&31].zero_pristine=false;
 memory->bo[scene_ids[16]&31].zero_pristine=false;
 policy.id=scene_ids[10];
}
int main(void)
{
 CHECK(!old_input_fixture());patch_setup();
 struct backing *b=&memory->bo[policy.id&31];
 unsigned char before[4096],expected[4096];
 for(unsigned i=0;i<sizeof(before);i++)before[i]=(u8)(i*17u+3u);
 memcpy(b->gem.bytes,before,sizeof(before));memcpy(expected,before,sizeof(expected));
 for(unsigned i=0;i<36;i++){
  u32 bits=0x3f000000u+i;
  for(unsigned k=0;k<4;k++)replacement[i*4+k]=(u8)(bits>>(8*k));
 }
 memcpy(expected+4,replacement,sizeof(replacement));
 unsigned maps=vmap_calls,zeroed=test_zeroed_pages,flushed=flushes,wmb=test_wmb;
 CHECK(remesa_private_session_patch_scene_input(NULL,&token,42,policy.id,4,replacement,144,&policy,data_policy)==-EINVAL);
 CHECK(remesa_private_session_patch_scene_input((void*)1,&token,42,policy.id,4,replacement,144,&policy,data_policy)==-EACCES);
 CHECK(remesa_private_session_patch_scene_input(memory,&other,42,policy.id,4,replacement,144,&policy,data_policy)==-EACCES);
 CHECK(remesa_private_session_patch_scene_input(memory,&token,0,policy.id,4,replacement,144,&policy,data_policy)==-EINVAL);
 CHECK(remesa_private_session_patch_scene_input(memory,&token,41,policy.id,4,replacement,144,&policy,data_policy)==-ESTALE);
 CHECK(remesa_private_session_patch_scene_input(memory,&token,42,policy.id,4,replacement,144,NULL,data_policy)==-EINVAL);
 CHECK(remesa_private_session_patch_scene_input(memory,&token,42,policy.id,4,replacement,144,&policy,NULL)==-EINVAL);
 CHECK(patch(policy.id,4,NULL,144)==-EINVAL&&patch(policy.id,4,replacement,0)==-EINVAL);
 CHECK(patch(policy.id,4,replacement,516)==-EINVAL&&patch(policy.id,5,replacement,144)==-EINVAL);
 CHECK(patch(policy.id,4092,replacement,144)==-EINVAL);
 CHECK(patch(policy.id,UINT64_MAX-3,replacement,144)==-EINVAL);
 CHECK(patch(policy.id,4096,replacement,144)==-ERANGE);
 CHECK(patch(policy.id,0,replacement,144)==-EACCES); /* self-address prefix */
 CHECK(patch(policy.id,4,replacement,140)==-EACCES); /* partial typed payload */
 CHECK(patch(policy.id^32,4,replacement,144)==-EACCES);
 CHECK(patch(scene_ids[18],4,replacement,144)==-EACCES); /* not a READ input */
 CHECK(patch(scene_ids[0],4,replacement,144)==-EACCES); /* not this policy's DATA */
 replacement[3]=0x7f;replacement[2]=0x80;
 CHECK(patch(policy.id,4,replacement,144)==-ERANGE); /* infinity */
 replacement[3]=0x3f;replacement[2]=0;
 CHECK(vmap_calls==maps&&flushes==flushed&&test_wmb==wmb);
 for(unsigned fault=0;fault<19;fault++){
  struct backing saved=*b;u32 pool=memory->private_dpm_ids[0];
  switch(fault){
   case 0:memory->private_source_held=false;break;
   case 1:memory->private_dpm_ready=false;break;
   case 2:memory->private_source_poisoned=true;break;
   case 3:memory->private_device_lost=true;break;
   case 4:memory->file=NULL;break;
   case 5:b->generation++;break;case 6:b->lease=true;break;
   case 7:b->holds=1;break;case 8:b->quarantine=true;break;
   case 9:b->pinned=false;break;case 10:b->pages=NULL;break;
   case 11:b->inserted--;break;case 12:b->input_seal=0;break;
   case 13:atomic_set(&b->color_vmas,1);break;
   case 14:memory->private_dpm_ids[0]^=32;break;
   case 15:memory->stream_bound=true;break;
   case 16:memory->private_work_owned=true;break;
   case 17:memory->bo[scene_ids[18]&31].lease=true;break;
   case 18:memory->jobs[0].live=true;break;
  }
  CHECK(patch(policy.id,4,replacement,144)!=0);
  CHECK(!memcmp(b->gem.bytes,before,sizeof(before))&&vmap_calls==maps&&flushes==flushed);
  *b=saved;memory->private_source_held=true;memory->private_dpm_ready=true;
  memory->private_source_poisoned=false;memory->private_device_lost=false;memory->file=&file;
  memory->private_dpm_ids[0]=pool;memory->stream_bound=false;memory->private_work_owned=false;
  memory->bo[scene_ids[18]&31].lease=false;memory->jobs[0].live=false;
 }
 fail_vmap=1;CHECK(patch(policy.id,4,replacement,144)==-ENOMEM);fail_vmap=0;
 CHECK(!memcmp(b->gem.bytes,before,sizeof(before))&&flushes==flushed);
 patch_pages=b->pages;test_flush_hook=patch_flush;
 CHECK(!patch(policy.id,4,replacement,144));
 CHECK(!memcmp(b->gem.bytes,expected,sizeof(expected))&&b->input_seal==2);
 CHECK(flushes==flushed+1&&test_wmb==wmb+1&&test_zeroed_pages==zeroed);
 CHECK(memory->private_dpm_ready&&memory->private_dpm_retired_operation==42);
 CHECK(request(REMESA_MEM_LEASE,policy.id,1,0,NULL,0)==-EPERM);
 CHECK(request(REMESA_MEM_SYNC,policy.id,0,0,NULL,0)==-EPERM);
 /* Stable cached mappings require no additional vmap/vunmap. */
 b->cached_job_view=b->gem.bytes;maps=vmap_calls;unsigned unmaps=vunmap_calls;
 CHECK(!patch(policy.id,4,replacement,144)&&vmap_calls==maps&&vunmap_calls==unmaps);
 /* Actual typed adapter: producer metadata is an injected admission policy;
  * live context/ID/address/size are independently checked under real locks. */
 struct remesa_parameter_patch_policy typed={.window={.owner=77,.generation=19,.gpu_va=b->va.start,
  .id=policy.id,.allocation_bytes=4096,.coefficient_dwords=36,.effective_source_dwords=37,
  .transfer_dwords=40,.use_offset=160,.use_bytes=8,.pds_offset=192,.pds_bytes=60,
  .ta_offset=256,.ta_bytes=8},.maximum_abs_bits=0x44800000u};
 CHECK(remesa_parameter_window_valid(&typed.window));
 struct remesa_parameter_patch_policy saved=typed;
 for(unsigned fault=0;fault<6;fault++){
  switch(fault){
   case 0:typed.window.owner++;break;case 1:typed.window.gpu_va+=4096;break;
   case 2:typed.window.allocation_bytes=8192;break;case 3:typed.window.id^=32;break;
   case 4:typed.maximum_abs_bits=0x7f800000;break;case 5:typed.window.generation=0;break;
  }
  unsigned old_flush=flushes;
  CHECK(remesa_private_session_patch_scene_parameters(memory,&token,42,&typed,policy.id,4,replacement,144)!=0);
  CHECK(flushes==old_flush&&!memcmp(b->gem.bytes,expected,sizeof(expected)));typed=saved;
 }
 CHECK(remesa_private_session_patch_scene_parameters(memory,&token,42,&typed,policy.id,0,replacement,4)==-EACCES);
 CHECK(remesa_private_session_patch_scene_parameters(memory,&token,42,&typed,policy.id,148,replacement,4)==-EACCES);
 CHECK(!remesa_private_session_patch_scene_parameters(memory,&token,42,&typed,policy.id,4,replacement,144));
 CHECK(!memcmp(b->gem.bytes,expected,sizeof(expected)));
 /* A single coefficient can change without uploading prefix/padding/CODE. */
 CHECK(!remesa_private_session_patch_scene_parameters(memory,&token,42,&typed,policy.id,144,replacement,4));
 memcpy(expected+144,replacement,4);
 CHECK(!memcmp(b->gem.bytes,expected,sizeof(expected))&&b->input_seal==2);
 b->cached_job_view=NULL;test_flush_hook=NULL;
 memory->private_source_held=false;
 CHECK(!remesa_private_session_release(memory,&token));
 CHECK(!remesa_gem_memory_close(memory));endpoint=memory=NULL;
 CHECK(!gems&&!pins&&!ptes&&!device_refs&&!module_refs&&!power&&!claimed);
 printf("PASS %u checks, typed sealed READ patch/idle epoch/refusal/exact page flush; GPU0\n",checks);
 return 0;
}
