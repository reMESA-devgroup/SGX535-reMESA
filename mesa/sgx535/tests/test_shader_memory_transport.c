/* SPDX-License-Identifier: MIT */
/* Original CPU-candidate upload through the production memory ioctl adapter.
 * All syscalls are interposed; this does not open a device or alter GPU state. */
#include "remesa_linux_memory.h"
#include "remesa_linux.h"
#include "remesa_draw.h"
#include "remesa_persistent.h"
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "remesa/shader_request.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef REMESA_WITH_NIR
#include "nir_fixture.h"
#include "vertex_nir_fixture.h"
#endif

static unsigned checks,calls,action[256],opens,native_opens,native_closes,versions;
static bool live,pinned,leased,write_lease;
static unsigned char bytes[4096];
static bool geometry_live,geometry_pinned,geometry_leased,geometry_write_lease;
static unsigned char geometry_bytes[4096];
static uint64_t context;
static bool fail_write;
#define CHECK(c) do { assert(c);++checks; } while(0)
int __wrap_ioctl(int,unsigned long,...);
int __wrap_open(const char *,int,...);
int __wrap_open64(const char *,int,...);
int __wrap_close(int);
int __wrap_open(const char *path,int flags,...)
{(void)flags;CHECK(!strcmp(path,"INTERPOSED-NO-DEVICE"));++native_opens;return 100;}
int __wrap_open64(const char *path,int flags,...)
{return __wrap_open(path,flags);}
int __wrap_close(int fd){CHECK(fd==100);++native_closes;return 0;}
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list args;va_start(args,command);struct remesa_memory_request *r=va_arg(args,void *);va_end(args);
 if(command==DRM_IOCTL_VERSION) {
  CHECK(fd==100);struct drm_version *version=(void *)r;
  memcpy(version->name,"gma500",6);version->name_len=6;++versions;return 0;
 }
 CHECK(fd==99&&command==DRM_IOCTL_REMESA_MEMORY&&calls<256);
 CHECK(r->version==REMESA_MEMORY_VERSION&&r->reserved==0);
 action[calls++]=r->action;
 if(r->action==REMESA_MEM_OPEN) {
  CHECK(!r->context&&!live&&!leased);context=1234+opens++;r->context=context;return 0;
 }
 CHECK(r->context==context);
 bool *object_live=r->id==33?&geometry_live:&live;
 bool *object_pinned=r->id==33?&geometry_pinned:&pinned;
 bool *object_leased=r->id==33?&geometry_leased:&leased;
 bool *object_write=r->id==33?&geometry_write_lease:&write_lease;
 unsigned char *object_bytes=r->id==33?geometry_bytes:bytes;
 if(r->action!=REMESA_MEM_CREATE&&r->action!=REMESA_MEM_CLOSE)
  CHECK(r->id==32||r->id==33);
 switch(r->action) {
 case REMESA_MEM_CLOSE:CHECK(!live&&!leased&&!geometry_live&&!geometry_leased);return 0;
 case REMESA_MEM_CREATE:
  CHECK(!r->id&&r->bytes==4096&&!geometry_live);
  if(live){r->id=33;geometry_live=true;memset(geometry_bytes,0xa5,sizeof(geometry_bytes));}
  else {r->id=32;live=true;memset(bytes,0xa5,sizeof(bytes));}
  return 0;
 case REMESA_MEM_PIN:CHECK(*object_live&&!*object_pinned);*object_pinned=true;return 0;
 case REMESA_MEM_LEASE:
  CHECK(*object_live&&*object_pinned&&!*object_leased);
  *object_leased=true;*object_write=r->flags!=0;return 0;
 case REMESA_MEM_READ:
  CHECK(*object_leased&&r->offset==0&&r->length==4096);
  memcpy(r->data,object_bytes,sizeof(bytes));return 0;
 case REMESA_MEM_WRITE:
  CHECK(*object_leased&&*object_write&&r->offset==0&&r->length==4096);
  if(fail_write){errno=EIO;return -1;}
  memcpy(object_bytes,r->data,sizeof(bytes));return 0;
 case REMESA_MEM_UNLEASE:CHECK(*object_leased);*object_leased=false;*object_write=false;return 0;
 case REMESA_MEM_UNPIN:CHECK(*object_pinned&&!*object_leased);*object_pinned=false;return 0;
 case REMESA_MEM_DESTROY:CHECK(*object_live&&!*object_pinned&&!*object_leased);*object_live=false;return 0;
 default:assert(!"SGX VA mapping, synchronization or unrelated ioctl is forbidden");return -1;
 }
}
static struct remesa_shader_draw_programs programs(void)
{
 struct remesa_shader_program vs,fs;struct remesa_shader_draw_programs pair;
#ifdef REMESA_WITH_NIR
 /* Join the previously qualified NIR frontend to the new native backing
  * binding. Both source shaders die before any byte storage is prepared. */
 struct nir_shader *vertex=vertex_fixture(),*fragment=fixture(0xff00ff00);
 struct remesa_shader_ir vertex_ir,fragment_ir;
 CHECK(!remesa_shader_ir_lower(vertex,&vertex_ir));
 CHECK(!remesa_shader_ir_lower(fragment,&fragment_ir));
 CHECK(!remesa_shader_program_prepare(&vertex_ir,&vs));
 CHECK(!remesa_shader_program_prepare(&fragment_ir,&fs));
 ralloc_free(vertex);ralloc_free(fragment);
 CHECK(!remesa_shader_program_validate(&vs));CHECK(!remesa_shader_program_validate(&fs));
#else
 struct remesa_shader_ir ir={.schema=1,.stage=REMESA_IR_VERTEX,.count=2};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32};
 ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.components=4,
  .type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 CHECK(!remesa_shader_program_prepare(&ir,&vs));CHECK(!remesa_shader_program_prepare_color(0xff00ff00,&fs));
#endif
 CHECK(!remesa_shader_draw_programs_prepare(&vs,&fs,&pair));return pair;
}
static void open_storage(struct remesa_linux_memory **memory,struct remesa_device *d)
{
 CHECK(!remesa_linux_memory_begin(99,memory));
 struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=*memory,
  .context=remesa_linux_memory_context(*memory),.va_start=0x20000000,.va_end=0x20010000,
  .byte_budget=4096,.resource_only_backend=true};
 CHECK(!remesa_persistent_open(d,&cfg));
}
static void native_storage(struct remesa_linux_memory **memory,struct remesa_device *d)
{
 CHECK(!remesa_linux_memory_begin(99,memory));
 struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=*memory,
  .context=remesa_linux_memory_context(*memory),.va_start=0x20000000,.va_end=0x20010000,
  .byte_budget=8192,.resource_only_backend=true};
 CHECK(!remesa_linux_open(d,"INTERPOSED-NO-DEVICE"));
 unsigned before=calls;
 struct remesa_persistent_config invalid=cfg;invalid.offline_test_backend=true;
 CHECK(remesa_linux_enable_shader_staging(d,&invalid)==EINVAL&&calls==before);
 invalid=cfg;invalid.qualified_hardware_backend=true;
 CHECK(remesa_linux_enable_shader_staging(d,&invalid)==EINVAL&&calls==before);
 invalid=cfg;invalid.resource_only_backend=false;
 CHECK(remesa_linux_enable_shader_staging(d,&invalid)==EINVAL&&calls==before);
 invalid=cfg;invalid.byte_budget=0;
 CHECK(remesa_linux_enable_shader_staging(d,&invalid)==EINVAL&&calls==before);
 CHECK(!remesa_linux_enable_shader_staging(d,&cfg));
 CHECK(remesa_linux_enable_shader_staging(d,&cfg)==EBUSY);
}
static void record_preview_bounds(const struct remesa_shader_pds_links *source)
{
 struct remesa_pds_plan plans[2]={source->launch.stage[0],source->launch.stage[1]};
 const struct remesa_pds_binding programs[2]={source->use[0].program,source->use[1].program};
 const uint32_t banks[2]={source->use[0].entry.bank,source->use[1].entry.bank};
 const uint64_t offsets[2]={0,0};
 struct remesa_pds_use_link links[2];
 struct remesa_pds_record_preview out[2],saved[2];
 for(unsigned n=0;n<8;n++) {
  struct remesa_pds_request requests[2]={source->use[0].request,source->use[1].request};
  int expected=0;
  if(n==0)requests[1].code.offset=requests[1].data.offset+16;
  if(n==1)requests[1].code.offset=requests[1].data.offset+1008;
  if(n==2)requests[1].code.offset=requests[1].data.offset+1024,expected=ERANGE;
  if(n==3)requests[1].code.address=requests[1].data.address=0x100000000ULL,expected=ERANGE;
  if(n==4)requests[1].code.offset++,expected=EINVAL;
  if(n==5){requests[1].code.offset=0;requests[1].code.bytes=16;expected=EINVAL;}
  if(n==6){requests[1].code.allocation_cookie+=100;requests[1].code.address+=4096;expected=ENOTSUP;}
  if(n==7)requests[1].code.address=requests[1].data.address=0x30001000;
  for(unsigned i=0;i<2;i++)CHECK(!remesa_pds_prepare(&source->launch.stage[i].program,&requests[i],&plans[i]));
  CHECK(!remesa_pds_prepare_use_links(plans,programs,banks,offsets,links));
  memset(out,0xa5,sizeof(out));memcpy(saved,out,sizeof(out));
  CHECK(remesa_pds_preview_record_pair(plans,programs,links,out)==expected);
  if(expected)CHECK(!memcmp(out,saved,sizeof(out)));
  else {
   CHECK(out[1].prefix_bytes==requests[1].code.offset-requests[1].data.offset);
   CHECK(!(out[1].address_prefix_bits&~out[1].address_prefix_known_mask));
   CHECK(out[1].block_address==requests[1].data.address+requests[1].data.offset);
   if(n==7)CHECK(out[1].high_address_region==0x30000000&&out[1].address_prefix_bits==0x0c000101);
  }
 }
 /* Independent original Xpsb producer + public-kernel source-slice oracles:
  * sgx-physical-task/PRIMARY_REFERENCE_ORACLES.json. Higher address-region
  * bits never leak into the now-qualified primary FS bits25:24. */
 const uint64_t block[]={0x20000160,0x20000160,0x30000160,0x2ffff160};
 const uint64_t prefix[]={48,64,48,48};
 const uint32_t word[]={0x0c000016,0x10000016,0x0c000016,0x0cffff16};
 for(unsigned n=0;n<4;n++) {
  struct remesa_pds_request requests[2]={source->use[0].request,source->use[1].request};
  requests[0].code.address=requests[0].data.address=0x24000000;
  requests[1].code.address=requests[1].data.address=block[n]-requests[1].data.offset;
  requests[1].code.offset=requests[1].data.offset+prefix[n];
  for(unsigned i=0;i<2;i++)CHECK(!remesa_pds_prepare(&source->launch.stage[i].program,&requests[i],&plans[i]));
  CHECK(!remesa_pds_prepare_use_links(plans,programs,banks,offsets,links));
  CHECK(!remesa_pds_preview_record_pair(plans,programs,links,out));
  CHECK(out[1].address_prefix_bits==word[n]&&out[1].address_prefix_known_mask==UINT32_MAX);
  CHECK(!(out[1].address_prefix_bits&UINT32_C(0x03000000)));
  CHECK(out[1].high_address_region==(block[n]&UINT64_C(0xf0000000)));
  CHECK(out[0].address_prefix_known_mask==UINT32_C(0xfcffffff));
 }
 CHECK(remesa_pds_preview_record_pair(plans,programs,links,(void *)plans)==EINVAL);
 CHECK(remesa_pds_preview_record_pair(plans,programs,links,(void *)programs)==EINVAL);
 CHECK(remesa_pds_preview_record_pair(plans,programs,links,(void *)links)==EINVAL);
 CHECK(remesa_pds_preview_record_pair((void *)(UINTPTR_MAX-3),programs,links,out)==EINVAL);
 CHECK(remesa_pds_preview_record_pair(plans,(void *)(UINTPTR_MAX-3),links,out)==EINVAL);
 CHECK(remesa_pds_preview_record_pair(plans,programs,(void *)(UINTPTR_MAX-3),out)==EINVAL);
}
static uint32_t host_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void native_host_fixups(struct remesa_device *device,void *token,
 const struct remesa_pds_request pds[2],const struct remesa_shader_pds_links *links)
{
 unsigned char raw[2][32],saved[2][32];struct remesa_pds_host_data data[2];
 memset(raw[0],0xa5,32);memset(raw[1],0x5a,32);memcpy(saved,raw,sizeof(raw));
 for(unsigned i=0;i<2;i++)data[i]=(struct remesa_pds_host_data){.binding=pds[i].data,
  .bytes=raw[i]+1,.byte_length=16}; /* Unaligned CPU pointers are serialized explicitly. */
 unsigned before=calls;
 CHECK(!remesa_linux_shader_apply_host_pds_fixups(device,token,0x10000000,pds,data)&&calls==before);
 for(unsigned i=0;i<2;i++) {
  size_t offset=1+(size_t)links->use[i].data_byte_offset;
  uint32_t previous=host_word(saved[i]+offset),patched=host_word(raw[i]+offset);
  CHECK((patched&0x7ffff)==links->use[i].address_bits);
  CHECK((patched&~0x7ffffu)==(previous&~0x7ffffu));
  for(unsigned b=0;b<32;b++)if(b<offset||b>=offset+4)CHECK(raw[i][b]==saved[i][b]);
  struct remesa_use_entry_fixup_fields decoded;
  CHECK(!remesa_use_entry_decode_fixup(patched&0x7ffff,&decoded));
  CHECK(decoded.bank==links->use[i].entry.bank&&decoded.entry_byte_offset==links->use[i].entry.entry_byte_offset);
 }
 memcpy(saved,raw,sizeof(raw));
 CHECK(!remesa_linux_shader_apply_host_pds_fixups(device,token,0x10000000,pds,data)&&
       calls==before&&!memcmp(raw,saved,sizeof(raw)));
 for(unsigned n=0;n<8;n++) {
  struct remesa_pds_host_data bad[2]={data[0],data[1]};int error=EINVAL;
  if(n==0)bad[1].binding.mapping_generation++,error=ESTALE;
  if(n==1)bad[1].byte_length--;
  if(n==2)bad[1].bytes=bad[0].bytes;
  if(n==3)bad[1].bytes=(void *)(UINTPTR_MAX-3);
  if(n==4)bad[1].bytes=token;
  if(n==5)bad[1].bytes=(void *)pds;
  if(n==6)bad[1].bytes=(void *)bad;
  if(n==7)bad[1].bytes=NULL;
  CHECK(remesa_linux_shader_apply_host_pds_fixups(device,token,0x10000000,pds,bad)==error&&
        calls==before&&!memcmp(raw,saved,sizeof(raw)));
 }
 struct remesa_pds_request stale[2]={pds[0],pds[1]};stale[1].generation++;
 CHECK(remesa_linux_shader_apply_host_pds_fixups(device,token,0x10000000,stale,data)==ESTALE&&
       calls==before&&!memcmp(raw,saved,sizeof(raw)));
 CHECK(remesa_linux_shader_apply_host_pds_fixups(device,(void *)1,0x10000000,pds,data)==EINVAL&&
       calls==before&&!memcmp(raw,saved,sizeof(raw)));
 CHECK(remesa_linux_shader_apply_host_pds_fixups(device,token,0x10000000,pds,
       (void *)(UINTPTR_MAX-3))==EINVAL&&calls==before);
 struct remesa_pds_host_data_view view[2],saved_view[2];
 memset(view,0x6b,sizeof(view));memcpy(saved_view,view,sizeof(view));
 CHECK(remesa_linux_shader_host_pds_data(device,token,0x10000000,pds,view)==EAGAIN&&
       calls==before&&!memcmp(view,saved_view,sizeof(view)));
 struct remesa_pds_host_data bad[2]={data[0],data[1]};bad[1].binding.mapping_generation++;
 CHECK(remesa_linux_shader_retain_host_pds_data(device,token,0x10000000,pds,bad)==ESTALE&&
       calls==before&&!memcmp(raw,saved,sizeof(raw)));
 CHECK(remesa_linux_shader_host_pds_data(device,token,0x10000000,pds,view)==EAGAIN&&
       !memcmp(view,saved_view,sizeof(view)));
 CHECK(!remesa_linux_shader_retain_host_pds_data(device,token,0x10000000,pds,data)&&calls==before);
 CHECK(remesa_linux_shader_retain_host_pds_data(device,token,0x10000000,pds,data)==EBUSY&&calls==before);
 CHECK(!remesa_linux_shader_host_pds_data(device,token,0x10000000,pds,view)&&calls==before);
 for(unsigned i=0;i<2;i++) {
  CHECK(view[i].storage.format==REMESA_SHADER_STORAGE_HOST_PDS_DATA&&view[i].storage.byte_length==16);
  CHECK(view[i].storage.bytes!=data[i].bytes&&!memcmp(view[i].storage.bytes,data[i].bytes,16));
 }
 memset(raw,0,sizeof(raw));
 for(unsigned i=0;i<2;i++)CHECK(!memcmp(view[i].storage.bytes,saved[i]+1,16));
 memcpy(saved_view,view,sizeof(view));
 CHECK(remesa_linux_shader_host_pds_data(device,token,0x10000000,stale,view)==ESTALE&&
       !memcmp(view,saved_view,sizeof(view))&&calls==before);
 CHECK(remesa_linux_shader_host_pds_data(device,token,0x10000000,pds,(void *)view[1].storage.bytes)==EINVAL&&calls==before);
 bad[0]=data[0];bad[1]=data[1];bad[1].bytes=(void *)view[1].storage.bytes;
 CHECK(remesa_linux_shader_apply_host_pds_fixups(device,token,0x10000000,pds,bad)==EINVAL&&calls==before);
 CHECK(device->ops->allocate(device->priv,4096,(void *)view[1].storage.bytes)==EINVAL&&calls==before);
 CHECK(device->ops->map(device->priv,(void *)1,false,(void *)view[1].storage.bytes)==EINVAL&&calls==before);
 CHECK(device->ops->submit(device->priv,(void *)1,(void *)1,(void *)view[1].storage.bytes)==EINVAL&&calls==before);
 CHECK(device->ops->submit_shader_programs(device->priv,(void *)1,(void *)1,(void *)1,
       token,(void *)view[1].storage.bytes)==EINVAL&&calls==before);
 struct remesa_shader_draw_programs replacement=programs();before=calls;
 CHECK(device->ops->prepare_shader_programs(device->priv,&replacement,(void *)view[1].storage.bytes)==EINVAL&&calls==before);
 for(unsigned i=0;i<2;i++)CHECK(!memcmp(view[i].storage.bytes,saved[i]+1,16));
}
static void native_pds_links(struct remesa_device *device,void *token,
 struct remesa_pds_request retained_requests[2],struct remesa_shader_pds_links *retained)
{
 struct remesa_shader_request request;struct remesa_shader_bindings bindings;
 unsigned before=calls;
 CHECK(!remesa_linux_shader_bindings(device,token,&request,&bindings)&&calls==before);
 struct remesa_pds_request pds[2];
 for(unsigned i=0;i<2;i++) {
  pds[i]=(struct remesa_pds_request){.operation=7,.generation=request.generation,
   .owner_cookie=request.owner,.code={.allocation_cookie=64+i,.owner_cookie=request.owner,
    .mapping_generation=19+i,.address=0x20000000+4096*i,.offset=64,.bytes=32,.allocation_bytes=4096}};
  pds[i].data=pds[i].code;pds[i].data.offset=16;pds[i].data.bytes=16;
 }
 uint32_t banks[2]={3,4};uint64_t offsets[2]={0,12};
 struct remesa_shader_pds_links links,saved,changed;
 memset(&changed,0x6b,sizeof(changed));saved=changed;
 CHECK(remesa_linux_shader_retained_pds_links(device,token,0x10000000,pds,&changed)==EAGAIN&&
       !memcmp(&changed,&saved,sizeof(changed))&&calls==before);
 CHECK(!remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,pds,offsets,&links)&&
       calls==before);
 CHECK(links.use[0].entry.program_end==0x10000020&&links.use[1].entry.program_end==0x10000028);
 CHECK(links.use[0].program.allocation_cookie==bindings.resources[0].handle&&
       links.use[1].program.allocation_cookie==bindings.resources[1].handle&&
       links.use[0].program.bytes==32&&links.use[1].program.offset==32&&links.use[1].program.bytes==8);
 CHECK(links.use[0].data_address==0x20000010&&links.use[1].data_address==0x2000101c);
 CHECK(links.use[0].address_bits==3&&links.use[1].address_bits==0x204);
 CHECK(links.launch.semantic.fragment_argb==0xff00ff00&&links.launch.semantic.binary_error==ENOTSUP);
 CHECK(!remesa_linux_shader_revalidate_pds_links(device,token,0x10000000,pds,&links)&&calls==before);
 saved=links;
 for(unsigned n=0;n<10;n++) {
  struct remesa_pds_request bad[2]={pds[0],pds[1]};int error=ESTALE;
  if(n==0)bad[1].generation++;
  if(n==1)bad[1].owner_cookie++;
  if(n==2)bad[1].operation++;
  if(n==3)bad[1].code.mapping_generation++;
  if(n==4)bad[1].code.address=bad[1].data.address=bad[0].code.address,error=EINVAL;
  if(n==5)bad[0].code.address=bad[0].data.address=0x10000000,error=EINVAL;
  if(n==6)bad[1].data.bytes=3,error=EINVAL;
  if(n==7)bad[1].data.offset=17,error=EINVAL;
  if(n==8)bad[0].code.address=bad[0].data.address=UINT64_MAX,error=EOVERFLOW;
  if(n==9)bad[0].data.offset=64,error=EINVAL;
  CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,bad,offsets,&links)==error&&
        calls==before&&!memcmp(&links,&saved,sizeof(links)));
  CHECK(remesa_linux_shader_retain_pds_links(device,token,0x10000000,banks,bad,offsets)==error&&
        calls==before);
  CHECK(remesa_linux_shader_retained_pds_links(device,token,0x10000000,pds,&links)==EAGAIN&&
        calls==before&&!memcmp(&links,&saved,sizeof(links)));
 }
 banks[1]=3;
 CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,pds,offsets,&links)==EBUSY&&
       calls==before&&!memcmp(&links,&saved,sizeof(links)));banks[1]=4;
 offsets[1]=16;
 CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,pds,offsets,&links)==EINVAL&&
       calls==before&&!memcmp(&links,&saved,sizeof(links)));offsets[1]=12;
 for(unsigned n=0;n<7;n++) {
  changed=links;
  if(n==0)changed.use[1].address_bits|=1u<<19;
  if(n==1)changed.use[1].program.bytes++;
  if(n==2)changed.use[1].program.mapping_generation++;
  if(n==3)changed.use[1].data_address++;
  if(n==4)changed.launch.stage[1].relocations[0].address++;
  if(n==5)changed.use[1].entry.program_end++;
  if(n==6)changed.use[1]=changed.use[0];
  CHECK(remesa_linux_shader_revalidate_pds_links(device,token,0x10000000,pds,&changed)!=0&&calls==before);
 }
 CHECK(remesa_linux_shader_revalidate_pds_links(device,token,0x10000100,pds,&links)==ESTALE&&calls==before);
 struct remesa_pds_request fresh[2]={pds[0],pds[1]};
 fresh[1].code.mapping_generation++;fresh[1].data.mapping_generation++;
 CHECK(remesa_linux_shader_revalidate_pds_links(device,token,0x10000000,fresh,&links)==ESTALE&&calls==before);
 CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,pds,offsets,(void *)token)==EINVAL);
 CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,pds,offsets,(void *)device)==EINVAL);
 CHECK(remesa_linux_shader_preview_pds_links(device,(void *)1,0x10000000,banks,pds,offsets,&links)==EINVAL);
 CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,
       (void *)(UINTPTR_MAX-15),offsets,&links)==EINVAL);
 CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,pds,offsets,
       (void *)(UINTPTR_MAX-15))==EINVAL);
 CHECK(remesa_linux_shader_preview_pds_links(device,token,0x10000000,banks,NULL,offsets,&links)==EINVAL);
 CHECK(remesa_linux_shader_revalidate_pds_links(device,token,0x10000000,pds,NULL)==EINVAL);
 CHECK(calls==before&&!memcmp(&links,&saved,sizeof(links)));
 CHECK(!remesa_linux_shader_retain_pds_links(device,token,0x10000000,banks,pds,offsets)&&calls==before);
 CHECK(remesa_linux_shader_retain_pds_links(device,token,0x10000000,banks,pds,offsets)==EBUSY&&calls==before);
 CHECK(!remesa_linux_shader_retained_pds_links(device,token,0x10000000,pds,&changed)&&
       !memcmp(&changed,&saved,sizeof(changed))&&calls==before);
 CHECK(remesa_linux_shader_retained_pds_links(device,token,0x10000100,pds,&changed)==ESTALE&&
       !memcmp(&changed,&saved,sizeof(changed))&&calls==before);
 CHECK(remesa_linux_shader_retained_pds_links(device,token,0x10000000,fresh,&changed)==ESTALE&&
       !memcmp(&changed,&saved,sizeof(changed))&&calls==before);
 CHECK(remesa_linux_shader_retained_pds_links(device,token,0x10000000,pds,(void *)pds)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_retained_pds_links(device,token,0x10000000,pds,(void *)token)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_retained_pds_links(device,token,0x10000000,
       (void *)(UINTPTR_MAX-15),&changed)==EINVAL&&calls==before);
 native_host_fixups(device,token,pds,&links);
 record_preview_bounds(&links);
 /* Exact retained CPU projections: no task-control or executable PDS. */
 {
  struct remesa_pds_record_preview previews[2],prior[2];
  memset(previews,0x6b,sizeof(previews));memcpy(prior,previews,sizeof(prior));
  CHECK(!remesa_linux_shader_preview_pds_records(device,token,0x10000000,pds,previews)&&calls==before);
  for(unsigned i=0;i<2;i++) {
   CHECK(previews[i].block_address==0x20000010+4096*i&&previews[i].prefix_bytes==48);
   CHECK(previews[i].code_window_address==0x20000040+4096*i&&previews[i].code_bytes==32);
   CHECK(previews[i].address_prefix_bits==0x0c000001+256*i&&
         previews[i].address_prefix_known_mask==(i?UINT32_MAX:UINT32_C(0xfcffffff))&&previews[i].high_address_region==0x20000000);
   CHECK(previews[i].use_entry_address==0x10000000+32*i&&previews[i].use_program_bytes==(i?8:32));
   CHECK(previews[i].use_address_bits==links.use[i].address_bits&&
         previews[i].data_fixup_address==links.use[i].data_address);
  }
  memcpy(prior,previews,sizeof(prior));
  CHECK(remesa_linux_shader_preview_pds_records(device,token,0x10000000,fresh,previews)==ESTALE&&
        !memcmp(previews,prior,sizeof(prior))&&calls==before);
  CHECK(remesa_linux_shader_preview_pds_records(device,(void *)1,0x10000000,pds,previews)==EINVAL&&
        !memcmp(previews,prior,sizeof(prior))&&calls==before);
  CHECK(remesa_linux_shader_preview_pds_records(device,token,0x10000000,pds,(void *)token)==EINVAL);
  CHECK(remesa_linux_shader_preview_pds_records(device,token,0x10000000,pds,(void *)pds)==EINVAL);
  CHECK(remesa_linux_shader_preview_pds_records(device,token,0x10000000,pds,(void *)(UINTPTR_MAX-3))==EINVAL);
  CHECK(remesa_linux_shader_preview_pds_records(device,token,0x10000000,(void *)(UINTPTR_MAX-3),previews)==EINVAL);
  CHECK(!memcmp(previews,prior,sizeof(prior))&&calls==before);
 }
 /* Source storage can now be reused without changing the token's plan. */
 retained_requests[0]=pds[0];retained_requests[1]=pds[1];*retained=saved;
 memset(pds,0,sizeof(pds));memset(banks,0,sizeof(banks));memset(offsets,0,sizeof(offsets));
 CHECK(!remesa_linux_shader_retained_pds_links(device,token,0x10000000,retained_requests,&changed)&&
       !memcmp(&changed,retained,sizeof(changed))&&calls==before);
}
static void native_vertex_storage(void)
{
 struct remesa_linux_memory *memory=NULL;struct remesa_device device={0};void *token=NULL;
 struct remesa_shader_draw_programs pair=programs();
 struct remesa_usse_vertex_mapping map={0};
 for(unsigned c=0;c<4;c++) {
  map.input[c]=(struct remesa_usse_cpu_register){0,32+c};
  map.output[c]=(struct remesa_usse_cpu_register){2,64+c};
 }
 native_storage(&memory,&device);
 unsigned before=calls;
 struct remesa_usse_vertex_mapping bad=map;bad.output[1]=map.input[3];
 CHECK(remesa_linux_prepare_vertex_candidate(&device,&pair,&bad,&token)==EINVAL&&
  !token&&calls==before&&!live);
 CHECK(!remesa_linux_set_vertex_candidate_mapping(&device,&map)&&calls==before);
 CHECK(device.ops->prepare_shader_programs(device.priv,&pair,&token)==ENOTSUP&&token);
 struct remesa_shader_request request;struct remesa_shader_bindings bindings;
 CHECK(!remesa_linux_shader_bindings(&device,token,&request,&bindings));
 CHECK(request.byte_count==40&&request.resource_mask==3&&bindings.resource_mask==3);
 CHECK(bindings.resources[0].handle==32&&bindings.resources[1].handle==32&&
  bindings.resources[0].length==32&&bindings.resources[1].offset==32&&bindings.resources[1].length==8);
 CHECK(!memcmp(bytes,request.data,40));
 unsigned preview_calls=calls;
 struct remesa_use_entry_plan vertex_entry,fragment_entry,unchanged;
 CHECK(!remesa_linux_shader_preview_stage_entry(&device,token,REMESA_IR_VERTEX,
  0x10000000,3,&vertex_entry)&&calls==preview_calls);
 CHECK(vertex_entry.entry_address==0x10000000&&vertex_entry.program_end==0x10000020);
 CHECK(!remesa_linux_shader_preview_use_entry(&device,token,0x10000000,3,&fragment_entry)&&
  calls==preview_calls&&fragment_entry.entry_address==vertex_entry.program_end&&
  fragment_entry.program_end==0x10000028);
 uint32_t banks[2]={3,4};struct remesa_use_entry_plan entries[2],saved_entries[2];
 CHECK(!remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,entries)&&
       calls==preview_calls);
 CHECK(entries[0].entry_address==0x10000000&&entries[0].program_end==0x10000020&&
       entries[1].entry_address==0x10000020&&entries[1].program_end==0x10000028&&
       entries[0].bank==3&&entries[1].bank==4);
 memcpy(saved_entries,entries,sizeof(entries));
 banks[1]=3;
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,entries)==EBUSY&&
       calls==preview_calls&&!memcmp(entries,saved_entries,sizeof(entries)));
 banks[1]=16;
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,entries)==ERANGE&&
       calls==preview_calls&&!memcmp(entries,saved_entries,sizeof(entries)));
 banks[1]=4;
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000001,banks,entries)==EINVAL&&
       calls==preview_calls&&!memcmp(entries,saved_entries,sizeof(entries)));
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,UINT64_MAX,banks,entries)==ERANGE&&
       calls==preview_calls&&!memcmp(entries,saved_entries,sizeof(entries)));
 CHECK(remesa_linux_shader_preview_entry_pair(&device,(void *)1,0x10000000,banks,entries)==EINVAL&&
       calls==preview_calls&&!memcmp(entries,saved_entries,sizeof(entries)));
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,(void *)token)==EINVAL);
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,(void *)&device)==EINVAL);
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,
       (void *)(UINTPTR_MAX-15))==EINVAL);
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,
       (void *)(UINTPTR_MAX-3),entries)==EINVAL&&!memcmp(entries,saved_entries,sizeof(entries)));
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,NULL,entries)==EINVAL);
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,NULL)==EINVAL);
 CHECK(calls==preview_calls&&!memcmp(entries,saved_entries,sizeof(entries)));
 unchanged=vertex_entry;
 CHECK(remesa_linux_shader_preview_stage_entry(&device,token,99,0x10000000,3,&vertex_entry)
  ==ENOTSUP&&calls==preview_calls&&!memcmp(&vertex_entry,&unchanged,sizeof(vertex_entry)));
 CHECK(remesa_linux_shader_preview_stage_entry(&device,token,REMESA_IR_VERTEX,
  UINT64_MAX,3,&vertex_entry)!=0&&calls==preview_calls&&
  !memcmp(&vertex_entry,&unchanged,sizeof(vertex_entry)));
 CHECK(remesa_linux_shader_preview_stage_entry(&device,token,REMESA_IR_VERTEX,
  0x10000000,3,(void *)token)==EINVAL&&calls==preview_calls);
 struct remesa_usse_cpu_move move;
 for(unsigned c=0;c<4;c++) {
  CHECK(!remesa_usse_cpu_move_decode(request.data+c*8,8,&move));
  CHECK(move.source.class_id==0&&move.source.number==32+c&&
   move.destination.class_id==2&&move.destination.number==64+c&&!move.control11);
 }
 struct remesa_pds_request old_pds[2];struct remesa_shader_pds_links old_links,current_links;
 native_pds_links(&device,token,old_pds,&old_links);current_links=old_links;
 memset(&pair,0,sizeof(pair));memset(&map,0,sizeof(map));
 CHECK(!remesa_linux_shader_verify(&device,token));
 struct remesa_linux_receipt receipt;
 CHECK(!remesa_linux_last_receipt(&device,&receipt)&&!receipt.attempted);
 void *fence=(void *)1;before=calls;
 CHECK(device.ops->submit_shader_programs(device.priv,(void *)1,(void *)1,(void *)1,
  token,&fence)==ENOTSUP&&!fence&&calls==before);
 device.ops->destroy_shader_programs(device.priv,token);before=calls;
 unsigned char retired_data[2][16],retired_saved[2][16];
 memset(retired_data,0x6b,sizeof(retired_data));memcpy(retired_saved,retired_data,sizeof(retired_data));
 struct remesa_pds_host_data retired_windows[2];
 for(unsigned i=0;i<2;i++)retired_windows[i]=(struct remesa_pds_host_data){
  .binding=old_pds[i].data,.bytes=retired_data[i],.byte_length=16};
 CHECK(remesa_linux_shader_apply_host_pds_fixups(&device,token,0x10000000,old_pds,retired_windows)==EINVAL&&
       calls==before&&!memcmp(retired_data,retired_saved,sizeof(retired_data)));
 struct remesa_pds_host_data_view retired_views[2],retired_views_saved[2];
 memset(retired_views,0x6b,sizeof(retired_views));memcpy(retired_views_saved,retired_views,sizeof(retired_views));
 CHECK(remesa_linux_shader_host_pds_data(&device,token,0x10000000,old_pds,retired_views)==EINVAL&&
       calls==before&&!memcmp(retired_views,retired_views_saved,sizeof(retired_views)));
 CHECK(remesa_linux_shader_retain_host_pds_data(&device,token,0x10000000,old_pds,retired_windows)==EINVAL&&calls==before);
 struct remesa_pds_record_preview records[2],saved_records[2];
 memset(records,0x6b,sizeof(records));memcpy(saved_records,records,sizeof(records));
 CHECK(remesa_linux_shader_preview_pds_records(&device,token,0x10000000,old_pds,records)==EINVAL&&
       calls==before&&!memcmp(records,saved_records,sizeof(records)));
 CHECK(remesa_linux_shader_retained_pds_links(&device,token,0x10000000,old_pds,&current_links)==EINVAL&&
       calls==before&&!memcmp(&current_links,&old_links,sizeof(current_links)));
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,entries)==EINVAL&&
       calls==before&&!memcmp(entries,saved_entries,sizeof(entries)));
 /* The backing provider reuses handle32, but a retired token must never
  * identify or destroy this replacement immutable program pair. */
 void *retired=token;token=NULL;pair=programs();
 CHECK(device.ops->prepare_shader_programs(device.priv,&pair,&token)==ENOTSUP&&token!=retired);
 before=calls;
 CHECK(remesa_linux_shader_bindings(&device,retired,&request,&bindings)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_preview_entry_pair(&device,retired,0x10000000,banks,entries)==EINVAL&&calls==before);
 device.ops->destroy_shader_programs(device.priv,retired);CHECK(calls==before&&live&&pinned);
 CHECK(remesa_linux_shader_bindings(&device,token,(void *)retired,&bindings)==EINVAL&&calls==before);
 CHECK(device.ops->prepare_shader_programs(device.priv,&pair,(void *)retired)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_preview_entry_pair(&device,token,0x10000000,banks,(void *)retired)==EINVAL&&calls==before);
 unsigned char cookie_before[sizeof(void *)];memcpy(cookie_before,retired,sizeof(cookie_before));
 CHECK(remesa_linux_last_shader_request(&device,(void *)retired)==EINVAL&&calls==before);
 CHECK(remesa_linux_last_receipt(&device,(void *)retired)==EINVAL&&calls==before);
 CHECK(remesa_linux_response(&device,(void *)retired)==EAGAIN&&calls==before);
 CHECK(device.ops->submit_shader_programs(device.priv,(void *)1,(void *)1,(void *)1,
  token,(void *)retired)==EINVAL&&calls==before);
 CHECK(device.ops->allocate(device.priv,4096,(void *)retired)==EINVAL&&calls==before);
 CHECK(device.ops->map(device.priv,(void *)1,false,(void *)retired)==EINVAL&&calls==before);
 CHECK(device.ops->submit(device.priv,(void *)1,(void *)1,(void *)retired)==EINVAL&&calls==before);
 CHECK(!memcmp(cookie_before,retired,sizeof(cookie_before)));
 memcpy(cookie_before,token,sizeof(cookie_before));
 CHECK(remesa_linux_last_shader_request(&device,(void *)token)==EINVAL&&calls==before);
 CHECK(remesa_linux_last_receipt(&device,(void *)token)==EINVAL&&calls==before);
 CHECK(device.ops->submit_shader_programs(device.priv,(void *)1,(void *)1,(void *)1,
  token,(void *)token)==EINVAL&&calls==before);
 CHECK(!memcmp(cookie_before,token,sizeof(cookie_before)));
 CHECK(!remesa_linux_shader_bindings(&device,token,&request,&bindings)&&request.generation==2&&
       bindings.resources[0].handle==32&&calls==before);
 CHECK(remesa_linux_shader_revalidate_pds_links(&device,retired,0x10000000,old_pds,&old_links)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_revalidate_pds_links(&device,token,0x10000000,old_pds,&old_links)==ESTALE&&calls==before);
 CHECK(remesa_linux_shader_retained_pds_links(&device,token,0x10000000,old_pds,&current_links)==EAGAIN&&
       calls==before&&!memcmp(&current_links,&old_links,sizeof(current_links)));
 CHECK(remesa_linux_shader_preview_pds_records(&device,token,0x10000000,old_pds,records)==EAGAIN&&
       calls==before&&!memcmp(records,saved_records,sizeof(records)));
 struct remesa_pds_request new_pds[2]={old_pds[0],old_pds[1]};
 for(unsigned i=0;i<2;i++)new_pds[i].generation=request.generation;
 CHECK(remesa_linux_shader_revalidate_pds_links(&device,token,0x10000000,new_pds,&old_links)==ESTALE&&calls==before);
 uint64_t data_offsets[2]={0,12};
 CHECK(!remesa_linux_shader_preview_pds_links(&device,token,0x10000000,banks,new_pds,data_offsets,&current_links)&&
       calls==before);
 CHECK(!remesa_linux_shader_revalidate_pds_links(&device,token,0x10000000,new_pds,&current_links)&&calls==before);
 CHECK(!remesa_linux_shader_retain_pds_links(&device,token,0x10000000,banks,new_pds,data_offsets)&&calls==before);
 struct remesa_shader_pds_links new_saved=current_links;
 CHECK(!remesa_linux_shader_retained_pds_links(&device,token,0x10000000,new_pds,&current_links)&&
       calls==before&&!memcmp(&current_links,&new_saved,sizeof(current_links)));
 CHECK(remesa_linux_shader_retained_pds_links(&device,token,0x10000000,old_pds,&current_links)==ESTALE&&
       calls==before&&!memcmp(&current_links,&new_saved,sizeof(current_links)));
 CHECK(remesa_linux_shader_preview_pds_records(&device,token,0x10000000,old_pds,records)==ESTALE&&
       calls==before&&!memcmp(records,saved_records,sizeof(records)));
 CHECK(!remesa_linux_shader_preview_pds_records(&device,token,0x10000000,new_pds,records)&&
       records[0].generation==2&&records[1].generation==2&&calls==before);
 CHECK(!remesa_linux_shader_verify(&device,token));
 device.ops->destroy_shader_programs(device.priv,token);
 CHECK(!remesa_linux_close_checked(&device)&&!live&&!pinned&&!leased);
 CHECK(!remesa_linux_memory_detach(memory));
}
int main(void)
{
 struct remesa_linux_memory *memory=NULL;struct remesa_device d={0};
 struct remesa_shader_draw_programs pair=programs();struct remesa_fragment_constant body;
 CHECK(!remesa_fragment_constant_prepare(&pair.fragment.usse,&body));open_storage(&memory,&d);
 void *token=NULL;
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
 CHECK(live&&pinned&&!leased);
 CHECK(!memcmp(bytes,body.record,sizeof(body.record)));
 bool zero=true;for(unsigned i=sizeof(body.record);i<4096;i++)zero=zero&&!bytes[i];CHECK(zero);
 struct remesa_shader_request request;struct remesa_persistent_shader_backing backing;
 CHECK(!remesa_persistent_shader_snapshot(&d,token,&request,&backing));
 CHECK(backing.owner==context&&backing.handle==32&&backing.allocation_bytes==4096&&backing.staged);
 CHECK(!memcmp(request.data,body.record,sizeof(body.record)));
 struct remesa_shader_bindings bindings;
 CHECK(!remesa_persistent_shader_bindings(&d,token,&request,&bindings));
 CHECK(bindings.owner==context&&bindings.generation==1&&bindings.resource_mask==2);
 CHECK(bindings.resources[1].handle==32&&bindings.resources[1].length==8&&
  bindings.resources[1].offset==0&&bindings.resources[1].allocation_bytes==4096);
 CHECK(!remesa_persistent_shader_verify(&d,token)&&!leased);
 unsigned before=calls;void *blocked=NULL;
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&blocked)==ENOSPC&&!blocked&&calls==before);
 void *fence=(void *)1;
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence&&calls==before);
 CHECK(remesa_persistent_close(&d)==EBUSY);
 d.ops->destroy_shader_programs(d.priv,token);CHECK(!live&&!pinned&&!leased);
 CHECK(!remesa_persistent_close(&d));CHECK(!remesa_linux_memory_detach(memory));memory=NULL;
 const unsigned expected[]={REMESA_MEM_OPEN,REMESA_MEM_CREATE,REMESA_MEM_PIN,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_WRITE,REMESA_MEM_UNLEASE,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_UNLEASE,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_UNLEASE,
  REMESA_MEM_UNPIN,REMESA_MEM_DESTROY,REMESA_MEM_CLOSE};
 CHECK(calls==sizeof(expected)/sizeof(expected[0])&&!memcmp(action,expected,sizeof(expected)));
 /* Production native preparation delegates the SAME owned bytes to the
  * production per-file memory adapter; enabling it does not open a session,
  * map the BO into SGX VA, consume diagnostic dispatch or enable execution. */
 d=(struct remesa_device){0};unsigned native_start=calls;
 native_storage(&memory,&d);token=NULL;
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
 CHECK(live&&pinned&&!leased&&!memcmp(bytes,body.record,sizeof(body.record)));
 CHECK(!remesa_linux_last_shader_request(&d,&request));
 CHECK(request.owner==context&&request.generation==1&&!memcmp(request.data,body.record,sizeof(body.record)));
 before=calls;
 CHECK(!remesa_linux_shader_bindings(&d,token,&request,&bindings)&&calls==before);
 CHECK(bindings.owner==context&&bindings.resources[1].handle==32&&bindings.resources[1].length==8);
 uint32_t pair_banks[2]={3,4};struct remesa_use_entry_plan absent_pair[2],unchanged_pair[2];
 memset(absent_pair,0xa5,sizeof(absent_pair));memcpy(unchanged_pair,absent_pair,sizeof(absent_pair));
 CHECK(remesa_linux_shader_preview_entry_pair(&d,token,0x10000000,pair_banks,absent_pair)==ENOTSUP&&
       calls==before&&!memcmp(absent_pair,unchanged_pair,sizeof(absent_pair)));
 struct remesa_pds_request absent_requests[2]={0};uint64_t absent_offsets[2]={0};
 struct remesa_shader_pds_links absent_links,saved_absent;
 memset(&absent_links,0xa5,sizeof(absent_links));saved_absent=absent_links;
 CHECK(remesa_linux_shader_preview_pds_links(&d,token,0x10000000,pair_banks,
       absent_requests,absent_offsets,&absent_links)==ENOTSUP&&calls==before&&
       !memcmp(&absent_links,&saved_absent,sizeof(absent_links)));
 /* Another live token on the same endpoint is immutable too. Reject query
  * outputs into it without touching its provider backing or metadata. */
 void *peer=NULL;
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&peer)==ENOTSUP&&peer&&peer!=token);
 before=calls;
 struct remesa_use_entry_plan peer_entry;
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,peer)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_bindings(&d,token,peer,&bindings)==EINVAL&&calls==before);
 struct remesa_shader_request peer_request;struct remesa_shader_bindings peer_bindings;
 CHECK(!remesa_linux_shader_bindings(&d,peer,&peer_request,&peer_bindings)&&calls==before);
 CHECK(peer_request.generation==2&&peer_bindings.resources[1].handle==33);
 CHECK(!remesa_linux_shader_preview_use_entry(&d,peer,0x10000100,3,&peer_entry)&&calls==before);
 CHECK(peer_entry.entry_address==0x10000100&&peer_entry.program_end==0x10000108);
 d.ops->destroy_shader_programs(d.priv,peer);
 CHECK(live&&pinned&&!leased&&!geometry_live&&!geometry_pinned&&!geometry_leased);
 before=calls;
 /* Own actual raw geometry with the native shader token, not packet's
  * diagnostic projected/clipped vertices. No ioctl or execution admission. */
 float vertex_data[3][4]={{-.75f,-.5f,0,1},{.5f,-.5f,0,1},{0,.75f,0,1}};
 float expected_vertices[3][4];memcpy(expected_vertices,vertex_data,sizeof(vertex_data));
 uint16_t index_data[3]={0,1,2},expected_indices[3];
 memcpy(expected_indices,index_data,sizeof(expected_indices));
 struct remesa_shader_draw_inputs draw_inputs={
  .vertex_data=vertex_data,.vertex_bytes=sizeof(vertex_data),.vertex_stride=16,
  .index_data=index_data,.index_bytes=sizeof(index_data),.index_size=2,
  .count=3,.position_format=PIPE_FORMAT_R32G32B32A32_FLOAT,
  .color_format=PIPE_FORMAT_B8G8R8A8_UNORM,.color_width=32,.color_height=32,
  .color_stride=128,.color_bytes=4096,
  .viewport={.scale={16,16,.5f},.translate={16,16,.5f}}
 },draw_snapshot,saved_draw;
 struct remesa_draw_plan draw_plan,saved_draw_plan;
 struct remesa_persistent_draw_backing geometry,saved_geometry;
 memset(&geometry,0xa5,sizeof(geometry));saved_geometry=geometry;
 CHECK(remesa_linux_shader_draw_backing(&d,token,&geometry)==EAGAIN&&calls==before);
 CHECK(!memcmp(&geometry,&saved_geometry,sizeof(geometry)));
 memset(&draw_snapshot,0xa5,sizeof(draw_snapshot));saved_draw=draw_snapshot;
 CHECK(remesa_linux_shader_draw_snapshot(&d,token,&draw_snapshot,&draw_plan)==EAGAIN&&calls==before);
 CHECK(!memcmp(&draw_snapshot,&saved_draw,sizeof(draw_snapshot)));
 CHECK(!remesa_linux_shader_stage_draw(&d,token,&draw_inputs)&&calls==before+9);
 before=calls;
 CHECK(geometry_live&&geometry_pinned&&!geometry_leased);
 CHECK(!remesa_linux_shader_draw_backing(&d,token,&geometry)&&calls==before);
 CHECK(geometry.owner==request.owner&&geometry.generation==request.generation&&
       geometry.handle==33&&geometry.allocation_bytes==4096&&geometry.vertex_bytes==48&&
       geometry.index_offset==48&&geometry.index_bytes==6&&geometry.pinned&&geometry.staged);
 CHECK(!memcmp(geometry_bytes,expected_vertices,sizeof(expected_vertices)));
 CHECK(!memcmp(geometry_bytes+48,expected_indices,sizeof(expected_indices)));
 zero=true;for(unsigned i=54;i<4096;i++)zero=zero&&!geometry_bytes[i];CHECK(zero);
 saved_geometry=geometry;
 memset(vertex_data,0xcc,sizeof(vertex_data));
 memset(index_data,0xcc,sizeof(index_data));
 CHECK(!remesa_linux_shader_draw_snapshot(&d,token,&draw_snapshot,&draw_plan)&&calls==before);
 CHECK(draw_snapshot.vertex_data!=vertex_data&&draw_snapshot.vertex_bytes==sizeof(vertex_data));
 CHECK(!memcmp(draw_snapshot.vertex_data,expected_vertices,sizeof(expected_vertices)));
 CHECK(draw_snapshot.count==3&&draw_snapshot.index_data!=index_data&&draw_snapshot.vertex_stride==16);
 CHECK(!memcmp(draw_snapshot.index_data,expected_indices,sizeof(expected_indices)));
 CHECK(draw_plan.primitive_count==1&&draw_plan.vertex_end_byte==48);
 saved_draw=draw_snapshot;saved_draw_plan=draw_plan;
 /* Replacement is EBUSY only for valid source geometry. Restore the mutated
  * source indices after proving the owned snapshot retained the old bytes. */
 memcpy(index_data,expected_indices,sizeof(index_data));
 CHECK(remesa_linux_shader_stage_draw(&d,token,&draw_inputs)==EBUSY&&calls==before);
 CHECK(!remesa_linux_shader_draw_backing(&d,token,&geometry)&&calls==before);
 CHECK(!memcmp(&geometry,&saved_geometry,sizeof(geometry)));
 draw_inputs.count=4;
 CHECK(remesa_linux_shader_stage_draw(&d,token,&draw_inputs)==EINVAL&&calls==before);
 CHECK(!remesa_linux_shader_draw_snapshot(&d,token,&draw_snapshot,&draw_plan)&&calls==before);
 CHECK(!memcmp(&draw_snapshot,&saved_draw,sizeof(draw_snapshot))&&
       !memcmp(&draw_plan,&saved_draw_plan,sizeof(draw_plan)));
 draw_inputs.count=3;draw_inputs.preserve_color=true;
 CHECK(remesa_linux_shader_stage_draw(&d,token,&draw_inputs)==ENOTSUP&&calls==before);
 CHECK(remesa_linux_shader_stage_draw(&d,(void *)1,&draw_inputs)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_draw_snapshot(&d,token,(void *)token,&draw_plan)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_draw_snapshot(&d,token,&draw_snapshot,(void *)&draw_snapshot)==EINVAL);
 CHECK(!memcmp(&draw_snapshot,&saved_draw,sizeof(draw_snapshot)));
 CHECK(remesa_linux_shader_draw_backing(&d,token,(void *)token)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_draw_backing(&d,token,(void *)draw_snapshot.vertex_data)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_draw_backing(&d,(void *)1,&geometry)==EINVAL&&calls==before);
 CHECK(!memcmp(&geometry,&saved_geometry,sizeof(geometry)));
 struct remesa_shader_bindings original=bindings;
 CHECK(remesa_linux_shader_bindings(&d,token,&request,(void *)&request)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_bindings(&d,token,&request,(void *)token)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_bindings(&d,token,(void *)(UINTPTR_MAX-63),&bindings)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_bindings(&d,token,&request,(void *)(UINTPTR_MAX-63))==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_bindings(&d,(void *)1,&request,&bindings)==EINVAL&&calls==before);
 CHECK(!memcmp(&bindings,&original,sizeof(bindings)));
 struct remesa_use_entry_plan entry,saved_entry;
 memset(&entry,0xa5,sizeof(entry));
 CHECK(!remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,&entry)&&calls==before);
 CHECK(entry.entry_address==0x10000000&&entry.program_end==0x10000008&&
       entry.entry_byte_offset==0&&entry.register_word==0x2200000);
 saved_entry=entry;
 uint32_t fixup=UINT32_MAX;
 struct remesa_use_entry_fixup_fields decoded;
 CHECK(!remesa_use_entry_preview_fixup(&entry,&fixup)&&fixup==3&&calls==before);
 CHECK(!remesa_use_entry_decode_fixup(fixup,&decoded)&&
       decoded.bank==3&&decoded.entry_byte_offset==0);
 CHECK(!remesa_linux_shader_preview_use_entry(&d,token,0x10000140,3,&entry)&&calls==before);
 CHECK(!remesa_use_entry_preview_fixup(&entry,&fixup)&&fixup==0x1403&&calls==before);
 CHECK(!remesa_use_entry_decode_fixup(fixup,&decoded)&&
       entry.base_address+decoded.entry_byte_offset==entry.entry_address);
 struct remesa_use_entry_plan aligned_entry=entry;
 for(unsigned low=1;low<16;low++) {
  CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000140+low,3,&entry)==EINVAL&&calls==before);
  CHECK(!memcmp(&entry,&aligned_entry,sizeof(entry)));
 }
 /* Native preview remains symbolic; neither projection publishes instructions
  * or lets the same token execute through the legacy color ioctl. */
 entry=saved_entry;
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,UINT64_MAX,3,&entry)==ERANGE&&calls==before);
 CHECK(!memcmp(&entry,&saved_entry,sizeof(entry)));
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,16,&entry)==ERANGE);
 CHECK(!memcmp(&entry,&saved_entry,sizeof(entry)));
 CHECK(remesa_linux_shader_preview_use_entry(&d,(void *)1,0x10000000,3,&entry)==EINVAL);
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,(void *)token)==EINVAL);
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,(void *)&d)==EINVAL);
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,
       (void *)(UINTPTR_MAX-7))==EINVAL);
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,NULL)==EINVAL);
 CHECK(calls==before&&!leased&&live&&pinned);
 /* An entry plan must never overwrite the immutable raw VB retained by
  * stage_draw. This is the real production preview, with no provider call. */
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,
       (void *)draw_snapshot.vertex_data)==EINVAL&&calls==before);
 CHECK(!memcmp(draw_snapshot.vertex_data,expected_vertices,sizeof(expected_vertices)));
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,
       (void *)((const unsigned char *)draw_snapshot.vertex_data+sizeof(expected_vertices)-1))
       ==EINVAL&&calls==before);
 CHECK(!memcmp(draw_snapshot.vertex_data,expected_vertices,sizeof(expected_vertices)));
 CHECK(remesa_linux_shader_bindings(&d,token,(void *)draw_snapshot.vertex_data,&bindings)
       ==EINVAL&&calls==before);
 CHECK(!memcmp(&bindings,&original,sizeof(bindings)));
 CHECK(remesa_linux_shader_bindings(&d,token,&request,(void *)draw_snapshot.vertex_data)
       ==EINVAL&&calls==before);
 CHECK(!memcmp(draw_snapshot.vertex_data,expected_vertices,sizeof(expected_vertices)));
 CHECK(remesa_linux_shader_draw_snapshot(&d,token,(void *)draw_snapshot.vertex_data,&draw_plan)
       ==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_draw_snapshot(&d,token,&draw_snapshot,(void *)draw_snapshot.vertex_data)
       ==EINVAL&&calls==before);
 CHECK(!memcmp(&draw_plan,&saved_draw_plan,sizeof(draw_plan)));
 CHECK(!memcmp(draw_snapshot.vertex_data,expected_vertices,sizeof(expected_vertices)));
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,
       (void *)draw_snapshot.index_data)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_bindings(&d,token,(void *)draw_snapshot.index_data,&bindings)
       ==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_bindings(&d,token,&request,(void *)draw_snapshot.index_data)
       ==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_draw_snapshot(&d,token,&draw_snapshot,(void *)draw_snapshot.index_data)
       ==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_draw_backing(&d,token,(void *)draw_snapshot.index_data)
       ==EINVAL&&calls==before);
 CHECK(!memcmp(draw_snapshot.index_data,expected_indices,sizeof(expected_indices)));
 CHECK(!memcmp(&bindings,&original,sizeof(bindings)));
 CHECK(remesa_linux_shader_preview_entry_pair(&d,token,0x10000000,pair_banks,
       (void *)draw_snapshot.vertex_data)==EINVAL&&calls==before);
 CHECK(remesa_linux_shader_preview_entry_pair(&d,token,0x10000000,pair_banks,
       (void *)draw_snapshot.index_data)==EINVAL&&calls==before);
 CHECK(!memcmp(draw_snapshot.vertex_data,expected_vertices,sizeof(expected_vertices))&&
       !memcmp(draw_snapshot.index_data,expected_indices,sizeof(expected_indices)));
 CHECK(!remesa_linux_shader_verify(&d,token)&&calls==before+6&&!leased&&!geometry_leased);
 struct remesa_linux_receipt receipt;CHECK(!remesa_linux_last_receipt(&d,&receipt)&&!receipt.attempted);
 fence=(void *)1;before=calls;
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence&&calls==before);
 /* close owns the nested tokens and releases their storage before its fd. */
 CHECK(!remesa_linux_close_checked(&d)&&!d.ops&&!d.priv&&!live&&!pinned&&!leased&&
       !geometry_live&&!geometry_pinned&&!geometry_leased);
 CHECK(remesa_linux_shader_bindings(&d,token,&request,&bindings)==EINVAL);
 CHECK(remesa_linux_shader_stage_draw(&d,token,&draw_inputs)==EINVAL);
 CHECK(remesa_linux_shader_draw_backing(&d,token,&geometry)==EINVAL);
 CHECK(!memcmp(&geometry,&saved_geometry,sizeof(geometry)));
 CHECK(remesa_linux_shader_preview_use_entry(&d,token,0x10000000,3,&entry)==EINVAL);
 CHECK(!memcmp(&entry,&saved_entry,sizeof(entry)));
 CHECK(!memcmp(&bindings,&original,sizeof(bindings)));
 CHECK(!remesa_linux_memory_detach(memory));memory=NULL;
 const unsigned native_expected[]={REMESA_MEM_OPEN,REMESA_MEM_CREATE,REMESA_MEM_PIN,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_WRITE,REMESA_MEM_UNLEASE,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_UNLEASE,
  REMESA_MEM_CREATE,REMESA_MEM_PIN,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_WRITE,REMESA_MEM_UNLEASE,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_UNLEASE,
  REMESA_MEM_UNPIN,REMESA_MEM_DESTROY,
  REMESA_MEM_CREATE,REMESA_MEM_PIN,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_WRITE,REMESA_MEM_UNLEASE,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_UNLEASE,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_UNLEASE,
  REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_UNLEASE,
  REMESA_MEM_UNPIN,REMESA_MEM_DESTROY,REMESA_MEM_UNPIN,REMESA_MEM_DESTROY,REMESA_MEM_CLOSE};
 CHECK(calls-native_start==sizeof(native_expected)/sizeof(native_expected[0])&&
       !memcmp(action+native_start,native_expected,sizeof(native_expected)));
 CHECK(native_opens==1&&native_closes==1&&versions==1);
 native_vertex_storage();
 unsigned prior_closes=native_closes;
 /* A possible partial WRITE does not receive UNLEASE/UNPIN/DESTROY. It is
  * quarantined and endpoint ownership survives until separately qualified
  * recovery, including the internal lease whose map pointer wasn't returned. */
 static struct remesa_device retained;static struct remesa_linux_memory *retained_memory;
 native_storage(&retained_memory,&retained);fail_write=true;before=calls;
 CHECK(retained.ops->prepare_shader_programs(retained.priv,&pair,&blocked)==EOWNERDEAD&&!blocked);
 CHECK(live&&pinned&&leased&&calls==before+5&&action[calls-1]==REMESA_MEM_WRITE);
 CHECK(remesa_linux_close_checked(&retained)==EBUSY&&retained.ops&&retained.priv&&native_closes==prior_closes);
 remesa_linux_close(&retained);CHECK(retained.ops&&retained.priv&&native_closes==prior_closes);
 CHECK(remesa_linux_memory_detach(retained_memory)==EBUSY);
 printf("PASS: %u original-shader memory-transport checks; no MAP/SYNC/DRAW or GPU\n",checks);
 return 0;
}
