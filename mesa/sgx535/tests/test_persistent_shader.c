/* SPDX-License-Identifier: MIT */
/* Owned original CPU candidates and explicit GPU-VA promotion in an injected
 * provider. No ioctl, instruction fetch, execution or hardware evidence. */
#include "remesa_persistent.h"
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "remesa_draw.h"
#include "remesa_pds_structure.h"
#include "remesa_ta_scene.h"
#include "remesa/shader_request.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { assert(c);++checks; } while(0)
enum failure {
 FAIL_NONE,FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_CREATE_UNCERTAIN,
 FAIL_PIN,FAIL_PIN_UNCERTAIN,FAIL_MAP,FAIL_MAP_UNCERTAIN,
 FAIL_MAP_NULL,FAIL_MAP_NONNULL_ERROR,FAIL_UNMAP_UNCERTAIN,
 FAIL_UNPIN,FAIL_DESTROY,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,
 FAIL_DUPLICATE_CREATE,FAIL_DUPLICATE_CREATE_ERROR,FAIL_CREATE_ZERO,FAIL_READ_GAP,
 FAIL_DUPLICATE_CREATE_UNCERTAIN
};
struct backing { unsigned char *bytes;size_t length;bool live,pinned,lease; };
struct fixture {
 struct backing backing[64];enum failure fail;
 unsigned creates,pins,cpu_maps,cpu_unmaps,reads,writes,unpins,destroys,forbidden;
 uint32_t duplicate_id,fail_map_handle;
 size_t corrupt_offset;
 bool allow_gpu_map,allow_sync;
 unsigned gpu_maps;
};
static struct backing *get(struct fixture *f,uint32_t id)
{ assert(id&&id<=64&&f->backing[id-1].live);return &f->backing[id-1]; }
static int create(void *data,size_t length,uint32_t *id)
{
 struct fixture *f=data;CHECK(length&&length%4096==0&&length<=REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES);*id=0;
 if(f->fail==FAIL_DUPLICATE_CREATE||f->fail==FAIL_DUPLICATE_CREATE_ERROR||
    f->fail==FAIL_DUPLICATE_CREATE_UNCERTAIN) {
  *id=f->duplicate_id?f->duplicate_id:1;CHECK(get(f,*id)->live);
  if(f->fail==FAIL_DUPLICATE_CREATE_UNCERTAIN)return EOWNERDEAD;
  return f->fail==FAIL_DUPLICATE_CREATE_ERROR?ENOMEM:0;
 }
 if(f->fail==FAIL_CREATE_ZERO)return 0;
 if(f->fail==FAIL_CREATE)return ENOMEM;
 CHECK(f->creates<64);struct backing *b=&f->backing[f->creates];
 b->bytes=malloc(length);CHECK(b->bytes);memset(b->bytes,0xa5,length);
 b->length=length;b->live=true;*id=++f->creates;
 if(f->fail==FAIL_CREATE_WITH_HANDLE)return ENOMEM;
 if(f->fail==FAIL_CREATE_UNCERTAIN)return EOWNERDEAD;
 return 0;
}
static int pin(void *data,uint32_t id)
{
 struct fixture *f=data;struct backing *b=get(f,id);CHECK(!b->pinned);
 if(f->fail==FAIL_PIN)return ENOMEM;
 b->pinned=true;++f->pins;
 return f->fail==FAIL_PIN_UNCERTAIN?EOWNERDEAD:0;
}
static int gpu_map(void *data,uint32_t id,uint64_t va,size_t n)
{ struct fixture *f=data;(void)id;(void)va;(void)n;if(f->allow_gpu_map){++f->gpu_maps;return 0;}++f->forbidden;return ENOTSUP; }
static int gpu_unmap(void *data,uint32_t id,uint64_t va,size_t n)
{ return gpu_map(data,id,va,n); }
static int unpin(void *data,uint32_t id)
{
 struct fixture *f=data;struct backing *b=get(f,id);CHECK(b->pinned&&!b->lease);
 if(f->fail==FAIL_UNPIN)return EOWNERDEAD;
 b->pinned=false;++f->unpins;return 0;
}
static int destroy(void *data,uint32_t id)
{
 struct fixture *f=data;struct backing *b=get(f,id);CHECK(!b->pinned&&!b->lease);
 if(f->fail==FAIL_DESTROY)return EOWNERDEAD;
 free(b->bytes);memset(b,0,sizeof(*b));++f->destroys;return 0;
}
static int cpu_map(void *data,uint32_t id,bool write,void **out)
{
 struct fixture *f=data;struct backing *b=get(f,id);CHECK(b->pinned&&!b->lease);
 enum failure selected=f->fail_map_handle&&f->fail_map_handle!=id?FAIL_NONE:f->fail;
 *out=NULL;++f->cpu_maps;
 if(write)++f->writes;else ++f->reads;
 if(!write&&selected==FAIL_READ_MAP)return ENOMEM;
 if(selected==FAIL_MAP)return ENOMEM;
 b->lease=true;
 if(selected==FAIL_MAP_UNCERTAIN)return EOWNERDEAD;
 if(selected==FAIL_MAP_NULL)return 0;
 *out=b->bytes;
 if(!write&&selected==FAIL_READ_BODY)b->bytes[0]^=1;
 if(!write&&selected==FAIL_READ_TAIL)b->bytes[4095]=1;
 if(!write&&selected==FAIL_READ_GAP){CHECK(f->corrupt_offset<b->length);b->bytes[f->corrupt_offset]^=1;}
 return selected==FAIL_MAP_NONNULL_ERROR?ENOMEM:0;
}
static int cpu_unmap(void *data,uint32_t id,bool write)
{
 struct fixture *f=data;struct backing *b=get(f,id);(void)write;CHECK(b->lease);++f->cpu_unmaps;
 if(f->fail==FAIL_UNMAP_UNCERTAIN)return EOWNERDEAD;
 b->lease=false;return 0;
}
static int sync_device(void *data,uint32_t id)
{struct fixture *f=data;(void)id;if(f->allow_sync)return 0;++f->forbidden;return ENOTSUP;}
static enum remesa_dispatch dispatch(void *data,const struct remesa_gpu_submit *s)
{struct fixture *f=data;(void)s;++f->forbidden;return REMESA_NOT_ISSUED;}
static int poll(void *data,uint64_t c,uint64_t op,uint64_t n,struct remesa_gpu_receipt *r)
{struct fixture *f=data;(void)c;(void)op;(void)n;(void)r;++f->forbidden;return ENOTSUP;}
static int release(void *data,uint64_t c,uint64_t op)
{struct fixture *f=data;(void)c;(void)op;++f->forbidden;return ENOTSUP;}
static const struct remesa_persistent_ops ops={
 create,pin,gpu_map,gpu_unmap,unpin,destroy,cpu_map,cpu_unmap,sync_device,dispatch,poll,release
};
static void open_device(struct fixture *f,struct remesa_device *d,uint64_t context)
{
 struct remesa_persistent_config c={.ops=&ops,.priv=f,.context=context,
  .va_start=0x100000,.va_end=0x200000,.byte_budget=65536,.resource_only_backend=true};
 CHECK(!remesa_persistent_open(d,&c));
 CHECK(d->ops->prepare_shader_programs&&d->ops->submit_shader_programs&&d->ops->destroy_shader_programs);
}
static struct remesa_shader_draw_programs programs(uint32_t color)
{
 struct remesa_shader_ir ir={.schema=1,.stage=REMESA_IR_VERTEX,.count=2};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32};
 ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.components=4,
  .type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 struct remesa_shader_program vs,fs;struct remesa_shader_draw_programs pair;
 CHECK(!remesa_shader_program_prepare(&ir,&vs));CHECK(!remesa_shader_program_prepare_color(color,&fs));
 CHECK(!remesa_shader_draw_programs_prepare(&vs,&fs,&pair));return pair;
}

static void staged(void)
{
 struct fixture f={0},other_f={0};struct remesa_device d={0},other={0};void *token=NULL,*second=NULL;
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 struct remesa_fragment_constant expected;CHECK(!remesa_fragment_constant_prepare(&pair.fragment.usse,&expected));
 open_device(&f,&d,101);open_device(&other_f,&other,102);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
 CHECK(f.creates==1&&f.pins==1&&f.cpu_maps==2&&f.cpu_unmaps==2&&f.reads==1&&f.writes==1&&!f.forbidden);
 CHECK(f.backing[0].live&&f.backing[0].pinned&&!f.backing[0].lease);
 CHECK(!memcmp(f.backing[0].bytes,expected.record,sizeof(expected.record)));
 bool zero_tail=true;for(unsigned i=sizeof(expected.record);i<4096;i++)zero_tail=zero_tail&&!f.backing[0].bytes[i];CHECK(zero_tail);
 struct remesa_shader_request request={0},saved;
 struct remesa_persistent_shader_backing binding={0},binding_saved;
 CHECK(!remesa_persistent_shader_snapshot(&d,token,&request,&binding));
 CHECK(binding.owner==101&&binding.generation==1&&binding.handle==1&&binding.allocation_bytes==4096);
 CHECK(binding.pinned&&binding.staged&&!binding.quarantined);
 CHECK(request.owner==101&&request.generation==1&&request.byte_count==sizeof(expected.record)&&request.resource_mask==2);
 CHECK(remesa_shader_request_validate(&request,101,1)==REMESA_SHADER_REQUEST_OK);
 CHECK(!memcmp(request.data,expected.record,sizeof(expected.record)));saved=request;binding_saved=binding;
 struct remesa_shader_bindings resources={0},resources_saved;
 CHECK(!remesa_persistent_shader_bindings(&d,token,&request,&resources));resources_saved=resources;
 CHECK(resources.owner==101&&resources.generation==1&&resources.resource_mask==2);
 CHECK(resources.resources[1].handle==1&&resources.resources[1].offset==0&&
  resources.resources[1].length==8&&resources.resources[1].allocation_bytes==4096);
 CHECK(remesa_shader_bindings_validate(&request,&resources,101,1)==REMESA_SHADER_REQUEST_OK);
 CHECK(remesa_persistent_shader_bindings(&d,token,&request,(void *)&request)==EINVAL);
 CHECK(!memcmp(&request,&saved,sizeof(request)));
 CHECK(remesa_persistent_shader_bindings(&d,token,&request,(void *)token)==EINVAL);
 CHECK(remesa_persistent_shader_bindings(&d,token,(void *)(UINTPTR_MAX-63),&resources)==EINVAL);
 CHECK(remesa_persistent_shader_bindings(&d,token,&request,(void *)(UINTPTR_MAX-63))==EINVAL);
 CHECK(remesa_persistent_shader_bindings(&other,token,&request,&resources)==EINVAL);
 CHECK(!memcmp(&resources,&resources_saved,sizeof(resources))&&!other_f.cpu_maps);
 unsigned reads=f.reads,maps=f.cpu_maps;
 CHECK(!remesa_persistent_shader_verify(&d,token));
 CHECK(f.reads==reads+1&&f.cpu_maps==maps+1&&!f.forbidden&&!f.backing[0].lease);
 CHECK(remesa_persistent_shader_snapshot(&d,token,(void *)token,&binding)==EINVAL);
 CHECK(remesa_persistent_shader_snapshot(&d,token,&request,(void *)token)==EINVAL);
 memset(&pair,0,sizeof(pair));
 CHECK(!remesa_persistent_shader_snapshot(&d,token,&request,&binding));
 CHECK(!memcmp(&request,&saved,sizeof(request))&&!memcmp(&binding,&binding_saved,sizeof(binding)));
 CHECK(remesa_persistent_shader_snapshot(&other,token,&request,&binding)==EINVAL);
 CHECK(!memcmp(&request,&saved,sizeof(request))&&!memcmp(&binding,&binding_saved,sizeof(binding)));
 other.ops->destroy_shader_programs(other.priv,token);CHECK(!other_f.creates&&!other_f.forbidden);
 CHECK(remesa_persistent_close(&d)==EBUSY);
 void *fence=(void *)1;
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence&&!f.forbidden);
 fence=(void *)1;
 CHECK(other.ops->submit_shader_programs(other.priv,(void *)1,(void *)1,(void *)1,token,&fence)==EINVAL&&!fence);
 pair=programs(0xffff00ff);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&second)==ENOTSUP&&second&&second!=token);
 CHECK(!remesa_persistent_shader_snapshot(&d,second,&request,&binding));
 CHECK(request.generation==2&&binding.generation==2&&binding.handle==2);
 CHECK(!remesa_persistent_shader_bindings(&d,second,&request,&resources));
 CHECK(resources.generation==2&&resources.resources[1].handle==2);
 CHECK(memcmp(request.data,saved.data,sizeof(expected.record)));
 d.ops->destroy_shader_programs(d.priv,token);
 CHECK(remesa_persistent_shader_snapshot(&d,token,&request,&binding)==EINVAL);
 CHECK(remesa_persistent_shader_verify(&d,token)==EINVAL);
 CHECK(remesa_persistent_shader_bindings(&d,token,&request,&resources)==EINVAL);
 d.ops->destroy_shader_programs(d.priv,token); /* Not dereferenced or freed twice. */
 token=NULL;d.ops->destroy_shader_programs(d.priv,second);second=NULL;
 CHECK(f.unpins==2&&f.destroys==2&&!f.forbidden);
 CHECK(!remesa_persistent_close(&d));CHECK(!remesa_persistent_close(&other));
}
static void ordinary_failures(void)
{
 const enum failure failures[]={FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_PIN,
  FAIL_MAP,FAIL_MAP_NULL,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL};
 const int expected_errors[]={ENOMEM,ENOMEM,ENOMEM,ENOMEM,EPROTO,ENOMEM,EPROTO,EPROTO};
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 for(unsigned n=0;n<sizeof(failures)/sizeof(failures[0]);n++) {
  struct fixture f={.fail=failures[n]};struct remesa_device d={0};void *token=(void *)1;
  open_device(&f,&d,200+n);
  CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==expected_errors[n]&&!token);
  CHECK(f.creates==f.destroys&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
}
/* Quarantine has no qualified recovery operation. Retain both endpoint and
 * provider for the process lifetime instead of pretending retirement. */
static struct remesa_device retained[14];
static struct fixture retained_fixture[14];
static void uncertain_failures(void)
{
 const enum failure failures[]={FAIL_CREATE_UNCERTAIN,FAIL_PIN_UNCERTAIN,
  FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR,FAIL_UNMAP_UNCERTAIN};
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 for(unsigned n=0;n<sizeof(failures)/sizeof(failures[0]);n++) {
  struct fixture *f=&retained_fixture[n];struct remesa_device *d=&retained[n];
  f->fail=failures[n];open_device(f,d,300+n);void *token=(void *)1;
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==EOWNERDEAD&&!token);
  CHECK(!f->destroys&&!f->unpins&&!f->forbidden);
  CHECK(remesa_persistent_close(d)==EBUSY);
  unsigned creates=f->creates;
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==EIO&&!token&&f->creates==creates);
 }
 for(unsigned n=5;n<7;n++) {
  struct fixture *f=&retained_fixture[n];struct remesa_device *d=&retained[n];void *token=NULL;
  open_device(f,d,300+n);
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  f->fail=n==5?FAIL_UNPIN:FAIL_DESTROY;
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(!f->destroys&&!f->forbidden);
  CHECK(remesa_persistent_close(d)==EBUSY);
 }
 for(unsigned n=7;n<9;n++) {
  struct fixture *f=&retained_fixture[n];struct remesa_device *d=&retained[n];
  void *token=NULL,*bad=NULL;open_device(f,d,300+n);
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  f->fail=n==7?FAIL_DUPLICATE_CREATE:FAIL_DUPLICATE_CREATE_ERROR;
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&bad)==EOWNERDEAD&&!bad);
  CHECK(!f->destroys&&!f->unpins&&!f->forbidden&&f->backing[0].pinned);
  CHECK(remesa_persistent_close(d)==EBUSY);
  /* Live original token remains held: corrupt duplicate identity does not
   * provide permission to release either disputed backing ownership. */
 }
 for(unsigned n=9;n<11;n++) {
  struct fixture *f=&retained_fixture[n];struct remesa_device *d=&retained[n];
  void *token=NULL,*ordinary=(void *)1;open_device(f,d,300+n);
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  f->fail=n==9?FAIL_DUPLICATE_CREATE:FAIL_DUPLICATE_CREATE_ERROR;
  unsigned pins=f->pins,maps=f->cpu_maps;
  /* Ordinary framebuffer/resource allocation must not pin or GPU-map an ID
   * already belonging to an immutable CPU candidate, even when CREATE also
   * reports an ordinary allocation error. Its ownership is disputed. */
  CHECK(d->ops->allocate(d->priv,4096,&ordinary)==EOWNERDEAD&&!ordinary);
  CHECK(f->pins==pins&&f->cpu_maps==maps&&!f->forbidden);
  CHECK(!f->unpins&&!f->destroys&&f->backing[0].pinned);
  CHECK(remesa_persistent_close(d)==EBUSY);
  CHECK(d->ops->allocate(d->priv,4096,&ordinary)==EIO&&!ordinary&&!f->forbidden);
 }
 for(unsigned n=11;n<13;n++) {
  struct fixture *f=&retained_fixture[n];struct remesa_device *d=&retained[n];void *token=NULL;
  open_device(f,d,300+n);
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  /* Candidate identity must fail if the actual body or zero tail changes
   * after the initial successful staging readback. Metadata alone is stale. */
  f->backing[0].bytes[n==11?0:4095]^=1;
  CHECK(remesa_persistent_shader_verify(d,token)==EOWNERDEAD);
  CHECK(!f->backing[0].lease&&!f->forbidden&&!f->unpins&&!f->destroys);
  struct remesa_shader_request request;
  struct remesa_shader_bindings bindings;
  CHECK(remesa_persistent_shader_bindings(d,token,&request,&bindings)==EOWNERDEAD);
  void *fence=(void *)1;
  CHECK(d->ops->submit_shader_programs(d->priv,(void *)1,(void *)1,(void *)1,token,&fence)==EIO&&!fence);
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(remesa_persistent_close(d)==EBUSY&&!f->unpins&&!f->destroys);
 }
 {
  struct fixture *f=&retained_fixture[13];struct remesa_device *d=&retained[13];
  void *token=NULL,*bad=NULL;open_device(f,d,313);
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  f->fail=FAIL_DUPLICATE_CREATE_UNCERTAIN;
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&bad)==EOWNERDEAD&&!bad);
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(remesa_persistent_close(d)==EBUSY&&!f->unpins&&!f->destroys&&!f->forbidden);
 }
}

static struct remesa_shader_draw_inputs geometry_inputs(void *vertices,size_t bytes,
 const void *indices)
{
 struct remesa_shader_draw_inputs in={
  .vertex_data=vertices,.vertex_bytes=bytes,.count=3,
  .position_format=PIPE_FORMAT_R32G32B32A32_FLOAT,
  .vertex_stride=16,.color_format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .color_width=32,.color_height=32,.color_stride=128,.color_bytes=4096,
  .viewport={.scale={16,16,.5f},.translate={16,16,.5f}}
 };
 if(indices) {
  in.index_data=indices;in.index_bytes=8;in.index_size=2;in.start=1;
  in.vertex_offset=1;in.element_offset=2;in.vertex_stride=20;
 }
 return in;
}

static void geometry_staged(void)
{
 struct fixture f={0},foreign_f={0};struct remesa_device d={0},foreign={0};void *token=NULL;
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 unsigned char vertices[64];for(unsigned i=0;i<sizeof(vertices);i++)vertices[i]=(unsigned char)i;
 uint16_t indices[4]={99,0,2,1};
 struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),indices);
 struct remesa_persistent_draw_backing backing,saved;
 memset(&backing,0xa5,sizeof(backing));saved=backing;
 open_device(&f,&d,400);open_device(&foreign_f,&foreign,401);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
 CHECK(remesa_persistent_shader_draw_backing(&d,token,&backing)==EAGAIN);
 CHECK(!memcmp(&backing,&saved,sizeof(backing)));
 CHECK(remesa_persistent_shader_stage_draw(&foreign,token,&inputs)==EINVAL&&!foreign_f.creates);
 CHECK(remesa_persistent_shader_stage_draw(&d,token,NULL)==EINVAL&&f.creates==1);
 inputs.position_input_slot=1;
 CHECK(remesa_persistent_shader_stage_draw(&d,token,&inputs)==EINVAL&&f.creates==1);
 inputs.position_input_slot=0;inputs.preserve_color=true;
 CHECK(remesa_persistent_shader_stage_draw(&d,token,&inputs)==ENOTSUP&&f.creates==1);
 inputs.preserve_color=false;
 CHECK(!remesa_persistent_shader_stage_draw(&d,token,&inputs));
 CHECK(f.creates==2&&f.pins==2&&f.cpu_maps==4&&f.cpu_unmaps==4&&!f.forbidden&&!f.gpu_maps);
 CHECK(!remesa_persistent_shader_draw_backing(&d,token,&backing));
 CHECK(backing.owner==400&&backing.generation==1&&backing.handle==2);
 CHECK(backing.allocation_bytes==4096&&backing.vertex_bytes==59&&backing.index_offset==60&&backing.index_bytes==8);
 CHECK(backing.pinned&&backing.staged);saved=backing;
 CHECK(!memcmp(f.backing[1].bytes,vertices,59));
 CHECK(!memcmp(f.backing[1].bytes+60,indices,8)&&!f.backing[1].bytes[59]);
 bool tail=true;for(unsigned i=68;i<4096;i++)tail=tail&&!f.backing[1].bytes[i];CHECK(tail);
 memset(vertices,0xff,sizeof(vertices));memset(indices,0xff,sizeof(indices));
 CHECK(f.backing[1].bytes[0]==0&&f.backing[1].bytes[58]==58&&f.backing[1].bytes[60]==99);
 unsigned reads=f.reads;
 CHECK(!remesa_persistent_shader_verify(&d,token)&&f.reads==reads+2);
 CHECK(remesa_persistent_shader_stage_draw(&d,token,&inputs)==EBUSY&&f.creates==2);
 CHECK(!remesa_persistent_shader_draw_backing(&d,token,&backing)&&!memcmp(&backing,&saved,sizeof(backing)));
 CHECK(remesa_persistent_shader_draw_backing(&d,token,(void *)token)==EINVAL);
 /* The overflow address must use this ABI's actual output extent: the
  * metadata occupies 64 bytes on the host and 40 bytes on i386. */
 CHECK(remesa_persistent_shader_draw_backing(&d,token,
       (void *)(UINTPTR_MAX-sizeof(struct remesa_persistent_draw_backing)+1))==EINVAL);
 CHECK(remesa_persistent_shader_draw_backing(&foreign,token,&backing)==EINVAL);
 CHECK(!memcmp(&backing,&saved,sizeof(backing)));
 void *fence=(void *)1;
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,&inputs,token,&fence)==ENOTSUP&&!fence);
 CHECK(!f.forbidden&&!f.gpu_maps);
 d.ops->destroy_shader_programs(d.priv,token);
 CHECK(f.destroys==2&&f.unpins==2&&!f.backing[0].live&&!f.backing[1].live);
 CHECK(remesa_persistent_shader_draw_backing(&d,token,&backing)==EINVAL);
 CHECK(!remesa_persistent_close(&d)&&!remesa_persistent_close(&foreign));

 memset(&f,0,sizeof(f));memset(&d,0,sizeof(d));token=NULL;open_device(&f,&d,402);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
 unsigned char large[12288]={0};inputs=geometry_inputs(large,sizeof(large),NULL);
 inputs.vertex_stride=4096;
 CHECK(!remesa_persistent_shader_stage_draw(&d,token,&inputs));
 CHECK(!remesa_persistent_shader_draw_backing(&d,token,&backing));
 CHECK(backing.vertex_bytes==8208&&backing.allocation_bytes==12288&&!backing.index_bytes&&!backing.index_offset);
 CHECK(!remesa_persistent_shader_verify(&d,token)&&!f.forbidden&&!f.gpu_maps);
 d.ops->destroy_shader_programs(d.priv,token);
 CHECK(!remesa_persistent_close(&d)&&f.destroys==2&&f.unpins==2);
}

static void geometry_ordinary_failures(void)
{
 const enum failure failures[]={FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_CREATE_ZERO,
  FAIL_PIN,FAIL_MAP,FAIL_MAP_NULL,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL,FAIL_READ_GAP};
 const int errors[]={ENOMEM,ENOMEM,EPROTO,ENOMEM,ENOMEM,EPROTO,ENOMEM,EPROTO,EPROTO,EPROTO};
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 unsigned char vertices[64]={0};uint16_t indices[4]={99,0,2,1};
 struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),indices);
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
  struct fixture f={0};struct remesa_device d={0};void *token=NULL;
  open_device(&f,&d,410+i);
  CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
  f.fail=failures[i];f.corrupt_offset=59;
  CHECK(remesa_persistent_shader_stage_draw(&d,token,&inputs)==errors[i]);
  CHECK(f.backing[0].live&&f.backing[0].pinned&&!f.backing[0].lease&&!f.forbidden&&!f.gpu_maps);
  struct remesa_persistent_draw_backing backing;
  CHECK(remesa_persistent_shader_draw_backing(&d,token,&backing)==EAGAIN);
  f.fail=FAIL_NONE;CHECK(!remesa_persistent_shader_verify(&d,token));
  CHECK(!remesa_persistent_shader_stage_draw(&d,token,&inputs));
  CHECK(!remesa_persistent_shader_draw_backing(&d,token,&backing)&&backing.owner==410+i);
  d.ops->destroy_shader_programs(d.priv,token);
  CHECK(f.creates==f.destroys&&!f.forbidden&&!f.gpu_maps);
  CHECK(!remesa_persistent_close(&d));
 }
 /* Cap and budget rejection precede provider allocation; token remains usable. */
 struct fixture f={0};struct remesa_device d={0};void *token=NULL;
 struct remesa_persistent_config cfg={.ops=&ops,.priv=&f,.context=430,
  .va_start=0x100000,.va_end=0x200000,.byte_budget=4096,.resource_only_backend=true};
 CHECK(!remesa_persistent_open(&d,&cfg));
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
 CHECK(remesa_persistent_shader_stage_draw(&d,token,&inputs)==ENOSPC&&f.creates==1);
 CHECK(!remesa_persistent_shader_verify(&d,token));
 size_t length=REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES+16;
 unsigned char *large=calloc(1,length);CHECK(large);
 struct remesa_shader_draw_inputs too_large=geometry_inputs(large,length,NULL);
 too_large.vertex_stride=REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES/2;
 CHECK(remesa_persistent_shader_stage_draw(&d,token,&too_large)==E2BIG&&f.creates==1);
 free(large);d.ops->destroy_shader_programs(d.priv,token);
 CHECK(!remesa_persistent_close(&d));
}

static struct remesa_device geometry_retained[24];
static struct fixture geometry_retained_fixture[24];
static void geometry_uncertain_failures(void)
{
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 unsigned char vertices[64]={0};uint16_t indices[4]={99,0,2,1};
 struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),indices);
 const enum failure uncertain[]={FAIL_CREATE_UNCERTAIN,FAIL_PIN_UNCERTAIN,
  FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR,FAIL_UNMAP_UNCERTAIN};
 for(unsigned i=0;i<sizeof(uncertain)/sizeof(uncertain[0]);i++) {
  struct fixture *f=&geometry_retained_fixture[i];struct remesa_device *d=&geometry_retained[i];void *token=NULL;
  open_device(f,d,440+i);CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  f->fail=uncertain[i];CHECK(remesa_persistent_shader_stage_draw(d,token,&inputs)==EOWNERDEAD);
  CHECK(remesa_persistent_shader_stage_draw(d,token,&inputs)==EOWNERDEAD);
  CHECK(!f->unpins&&!f->destroys&&!f->forbidden&&!f->gpu_maps);
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(remesa_persistent_close(d)==EBUSY&&!f->unpins&&!f->destroys);
 }
 for(unsigned i=5;i<7;i++) {
  struct fixture *f=&geometry_retained_fixture[i];struct remesa_device *d=&geometry_retained[i];void *token=NULL;
  open_device(f,d,440+i);CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  CHECK(!remesa_persistent_shader_stage_draw(d,token,&inputs));
  struct remesa_shader_draw_inputs saved;struct remesa_draw_plan plan;
  CHECK(!remesa_persistent_shader_draw_snapshot(d,token,&saved,&plan));
  f->fail=i==5?FAIL_UNPIN:FAIL_DESTROY;
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(remesa_persistent_close(d)==EBUSY&&!f->destroys&&!f->forbidden&&!f->gpu_maps);
  CHECK(f->backing[0].live&&f->backing[0].pinned); /* FS not freed behind failed geometry cleanup. */
  struct remesa_shader_request diagnostic;
  CHECK(!remesa_persistent_shader_output_check(d,&diagnostic,sizeof(diagnostic)));
  CHECK(remesa_persistent_shader_output_check(d,saved.vertex_data,sizeof(diagnostic))==EINVAL);
  CHECK(remesa_persistent_shader_output_check(d,token,sizeof(void *))==EINVAL);
 }
 /* Recycled identity: geometry -> FS; geometry -> another geometry; new FS
  * and regular allocations -> geometry. Destruction must retain BOTH owners. */
 for(unsigned i=7;i<15;i++) {
  struct fixture *f=&geometry_retained_fixture[i];struct remesa_device *d=&geometry_retained[i];void *token=NULL,*second=NULL;
  open_device(f,d,440+i);CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  if(i>=9)CHECK(!remesa_persistent_shader_stage_draw(d,token,&inputs));
  if(i==9||i==10)CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&second)==ENOTSUP&&second);
  f->duplicate_id=i<9?1:2;
  f->fail=i%2?FAIL_DUPLICATE_CREATE:FAIL_DUPLICATE_CREATE_ERROR;
  unsigned pins=f->pins,maps=f->cpu_maps,unpins=f->unpins,destroys=f->destroys;
  if(i<9)CHECK(remesa_persistent_shader_stage_draw(d,token,&inputs)==EOWNERDEAD);
  else if(i<11)CHECK(remesa_persistent_shader_stage_draw(d,second,&inputs)==EOWNERDEAD);
  else if(i<13)CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&second)==EOWNERDEAD&&!second);
  else {void *ordinary=NULL;CHECK(d->ops->allocate(d->priv,4096,&ordinary)==EOWNERDEAD&&!ordinary);}
  CHECK(f->pins==pins&&f->cpu_maps==maps&&!f->forbidden&&!f->gpu_maps);
  d->ops->destroy_shader_programs(d->priv,token);
  if(second)d->ops->destroy_shader_programs(d->priv,second);
  CHECK(f->unpins==unpins&&f->destroys==destroys&&remesa_persistent_close(d)==EBUSY);
 }
 /* Geometry CREATE cannot alias a live ordinary framebuffer allocation. */
 for(unsigned i=15;i<17;i++) {
  struct fixture *f=&geometry_retained_fixture[i];struct remesa_device *d=&geometry_retained[i];void *token=NULL,*ordinary=NULL;
  f->allow_gpu_map=true;open_device(f,d,440+i);
  CHECK(!d->ops->allocate(d->priv,4096,&ordinary)&&ordinary);
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  unsigned maps=f->gpu_maps;f->duplicate_id=1;f->fail=i==15?FAIL_DUPLICATE_CREATE:FAIL_DUPLICATE_CREATE_ERROR;
  CHECK(remesa_persistent_shader_stage_draw(d,token,&inputs)==EOWNERDEAD&&f->gpu_maps==maps);
  d->ops->destroy_bo(d->priv,ordinary);d->ops->destroy_shader_programs(d->priv,token);
  CHECK(!f->destroys&&!f->unpins&&!f->forbidden&&f->gpu_maps==maps&&remesa_persistent_close(d)==EBUSY);
 }
 const size_t corrupt[]={0,59,61,4095};
 for(unsigned i=17;i<21;i++) {
  struct fixture *f=&geometry_retained_fixture[i];struct remesa_device *d=&geometry_retained[i];void *token=NULL;
  open_device(f,d,440+i);CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  CHECK(!remesa_persistent_shader_stage_draw(d,token,&inputs));f->backing[1].bytes[corrupt[i-17]]^=1;
  CHECK(remesa_persistent_shader_verify(d,token)==EOWNERDEAD);
  struct remesa_persistent_draw_backing backing;
  CHECK(remesa_persistent_shader_draw_backing(d,token,&backing)==EOWNERDEAD);
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(!f->unpins&&!f->destroys&&!f->forbidden&&!f->gpu_maps&&remesa_persistent_close(d)==EBUSY);
 }
 {
  struct fixture *f=&geometry_retained_fixture[21];struct remesa_device *d=&geometry_retained[21];void *token=NULL;
  open_device(f,d,461);CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  f->fail=FAIL_DUPLICATE_CREATE_UNCERTAIN;
  CHECK(remesa_persistent_shader_stage_draw(d,token,&inputs)==EOWNERDEAD);
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(remesa_persistent_close(d)==EBUSY&&!f->unpins&&!f->destroys&&!f->forbidden);
 }
}
static void vertex_staged(void)
{
 struct fixture f={0};struct remesa_device d={0};void *token=NULL;
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 struct remesa_usse_vertex_mapping map={0};
 for(unsigned c=0;c<4;c++) {
  map.input[c]=(struct remesa_usse_cpu_register){0,12+c};
  map.output[c]=(struct remesa_usse_cpu_register){2,64+c};
 }
 unsigned char expected[32];size_t length;
 CHECK(!remesa_usse_vertex_candidate(&pair.vertex.usse,&map,expected,32,&length));
 open_device(&f,&d,700);
 struct remesa_usse_vertex_mapping bad=map;bad.output[1]=map.input[3];
 CHECK(remesa_persistent_prepare_vertex_candidate(&d,&pair,&bad,&token)==EINVAL&&!token&&!f.creates);
 CHECK(remesa_persistent_prepare_vertex_candidate(&d,&pair,&map,&token)==ENOTSUP&&token);
 struct remesa_shader_request request;struct remesa_shader_bindings binding;
 CHECK(!remesa_persistent_shader_bindings(&d,token,&request,&binding));
 CHECK(request.resource_mask==3&&request.byte_count==40&&binding.resource_mask==3);
 CHECK(binding.resources[0].handle==binding.resources[1].handle&&
  binding.resources[0].length==32&&binding.resources[1].offset==32&&binding.resources[1].length==8);
 CHECK(!memcmp(f.backing[0].bytes,expected,32)&&!memcmp(f.backing[0].bytes,request.data,40));
 CHECK(remesa_shader_bindings_validate(&request,&binding,700,1)==REMESA_SHADER_REQUEST_OK);
 CHECK(remesa_persistent_prepare_vertex_candidate(&d,&pair,&map,&token)==EBUSY);
 CHECK(remesa_persistent_prepare_vertex_candidate(&d,&pair,&map,(void *)token)==EINVAL);
 memset(&pair,0,sizeof(pair));memset(&map,0,sizeof(map));
 CHECK(!remesa_persistent_shader_verify(&d,token)&&!f.gpu_maps&&!f.forbidden);
 void *fence=(void *)1;
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence);
 d.ops->destroy_shader_programs(d.priv,token);
 CHECK(f.unpins==1&&f.destroys==1&&!f.forbidden&&!f.gpu_maps);
 CHECK(!remesa_persistent_close(&d));
 /* A mutation in either owned program invalidates the entire pair. */
 static struct fixture corrupt[2];static struct remesa_device retained[2];
 for(unsigned n=0;n<2;n++) {
  pair=programs(0xff00ff00);
  for(unsigned c=0;c<4;c++) {
   map.input[c]=(struct remesa_usse_cpu_register){0,12+c};
   map.output[c]=(struct remesa_usse_cpu_register){2,64+c};
  }
  token=NULL;open_device(&corrupt[n],&retained[n],710+n);
  CHECK(remesa_persistent_prepare_vertex_candidate(&retained[n],&pair,&map,&token)==ENOTSUP&&token);
  corrupt[n].backing[0].bytes[n?32:0]^=1;
  CHECK(remesa_persistent_shader_verify(&retained[n],token)==EOWNERDEAD);
  retained[n].ops->destroy_shader_programs(retained[n].priv,token);
  CHECK(!corrupt[n].unpins&&!corrupt[n].destroys&&!corrupt[n].forbidden&&
   !corrupt[n].gpu_maps&&remesa_persistent_close(&retained[n])==EBUSY);
 }
}
static void cookie_lifetime_and_cross_token_outputs(void)
{
 struct fixture f={0};struct remesa_device d={0};void *old=NULL,*current=NULL,*second=NULL;
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 open_device(&f,&d,750);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&old)==ENOTSUP&&old);
 d.ops->destroy_shader_programs(d.priv,old);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&current)==ENOTSUP&&current!=old);
 struct remesa_shader_request request,saved;
 struct remesa_persistent_shader_backing backing;
 struct remesa_shader_bindings bindings;
 CHECK(!remesa_persistent_shader_snapshot(&d,current,&request,&backing));saved=request;
 unsigned maps=f.cpu_maps,destroys=f.destroys;
 CHECK(!remesa_persistent_shader_output_check(&d,&request,sizeof(request)));
 CHECK(remesa_persistent_shader_output_check(&d,NULL,sizeof(request))==EINVAL);
 CHECK(remesa_persistent_shader_output_check(&d,&request,0)==EINVAL);
 CHECK(remesa_persistent_shader_output_check(&d,&d,sizeof(request))==EINVAL);
 CHECK(remesa_persistent_shader_output_check(&d,(void *)(UINTPTR_MAX-1),sizeof(request))==EINVAL);
 CHECK(remesa_persistent_shader_snapshot(&d,old,&request,&backing)==EINVAL);
 CHECK(!memcmp(&request,&saved,sizeof(request)));
 d.ops->destroy_shader_programs(d.priv,old);
 CHECK(f.destroys==destroys&&f.cpu_maps==maps);
 CHECK(remesa_persistent_shader_snapshot(&d,current,(void *)old,&backing)==EINVAL);
 CHECK(remesa_persistent_shader_bindings(&d,current,&request,(void *)old)==EINVAL);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,(void *)old)==EINVAL&&f.cpu_maps==maps);
 unsigned char cookie_before[sizeof(void *)];memcpy(cookie_before,old,sizeof(cookie_before));
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,current,
  (void *)old)==EINVAL&&f.cpu_maps==maps);
 CHECK(d.ops->allocate(d.priv,4096,(void *)old)==EINVAL&&f.cpu_maps==maps);
 CHECK(d.ops->map(d.priv,(void *)1,false,(void *)old)==EINVAL&&f.cpu_maps==maps);
 CHECK(d.ops->submit(d.priv,(void *)1,(void *)1,(void *)old)==EINVAL&&f.cpu_maps==maps);
 CHECK(!memcmp(cookie_before,old,sizeof(cookie_before)));
 memcpy(cookie_before,current,sizeof(cookie_before));
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,current,
  (void *)current)==EINVAL&&f.cpu_maps==maps);
 CHECK(!memcmp(cookie_before,current,sizeof(cookie_before)));
 CHECK(!remesa_persistent_shader_verify(&d,current));
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&second)==ENOTSUP&&second!=old&&second!=current);
 unsigned char vertices[64]={0};uint16_t indices[4]={99,0,2,1};
 struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),indices);
 struct remesa_shader_draw_inputs a,b,unchanged;struct remesa_draw_plan plan,plan_saved;
 memset(&a,0xa5,sizeof(a));unchanged=a;memset(&plan,0xa5,sizeof(plan));plan_saved=plan;
 CHECK(remesa_persistent_shader_draw_snapshot(&d,current,&a,&plan)==EAGAIN);
 CHECK(!memcmp(&a,&unchanged,sizeof(a))&&!memcmp(&plan,&plan_saved,sizeof(plan)));
 CHECK(!remesa_persistent_shader_stage_draw(&d,current,&inputs));
 vertices[0]=17;
 CHECK(!remesa_persistent_shader_stage_draw(&d,second,&inputs));
 CHECK(!remesa_persistent_shader_draw_snapshot(&d,current,&a,&plan));
 CHECK(!remesa_persistent_shader_draw_snapshot(&d,second,&b,&plan));
 CHECK(a.vertex_data!=b.vertex_data&&((const unsigned char *)a.vertex_data)[0]==0&&
       ((const unsigned char *)b.vertex_data)[0]==17);
 unsigned char original[59];memcpy(original,b.vertex_data,sizeof(original));
 maps=f.cpu_maps;
 CHECK(remesa_persistent_shader_snapshot(&d,current,(void *)b.vertex_data,&backing)==EINVAL);
 CHECK(remesa_persistent_shader_snapshot(&d,current,&request,(void *)b.index_data)==EINVAL);
 CHECK(remesa_persistent_shader_bindings(&d,current,&request,(void *)b.vertex_data)==EINVAL);
 CHECK(remesa_persistent_shader_draw_backing(&d,current,(void *)b.vertex_data)==EINVAL);
 CHECK(remesa_persistent_shader_draw_snapshot(&d,current,(void *)b.vertex_data,&plan)==EINVAL);
 CHECK(remesa_persistent_shader_draw_snapshot(&d,current,&a,(void *)b.index_data)==EINVAL);
 CHECK(remesa_persistent_shader_draw_snapshot(&d,current,&a,(void *)&a)==EINVAL);
 CHECK(!memcmp(original,b.vertex_data,sizeof(original))&&f.cpu_maps==maps);
 CHECK(!remesa_persistent_shader_bindings(&d,second,&request,&bindings));
 CHECK(!remesa_persistent_shader_verify(&d,current)&&!remesa_persistent_shader_verify(&d,second));
 CHECK(!f.forbidden&&!f.gpu_maps);
 d.ops->destroy_shader_programs(d.priv,current);d.ops->destroy_shader_programs(d.priv,second);
 CHECK(!remesa_persistent_close(&d)&&f.creates==f.destroys&&f.pins==f.unpins);
}
static void geometry_gpu_owned(void)
{
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 for(unsigned indexed=0;indexed<2;indexed++) {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *token=NULL;open_device(&f,&d,900+indexed);
  CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
  uint8_t vertices[64];for(unsigned i=0;i<64;i++)vertices[i]=(uint8_t)i;
  uint16_t indices[4]={99,1,3,2};
  struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),indexed?indices:NULL);
  if(indexed)inputs.index_bias=-1;
  struct remesa_persistent_draw_gpu_backing result,saved;
  memset(&result,0xa5,sizeof(result));saved=result;
  CHECK(remesa_persistent_shader_draw_gpu_backing(&d,token,&result)==EAGAIN);
  CHECK(!memcmp(&result,&saved,sizeof(result)));
  CHECK(!remesa_persistent_shader_stage_draw(&d,token,&inputs)&&!f.gpu_maps);
  memset(vertices,255,sizeof(vertices));memset(indices,255,sizeof(indices));
  CHECK(!remesa_persistent_shader_prepare_draw_gpu(&d,token));
  CHECK(!remesa_persistent_shader_draw_gpu_backing(&d,token,&result));
  CHECK(result.owner==900+indexed&&result.generation==1&&result.handle==3);
  CHECK(result.gpu_va==0x100000&&result.allocation_bytes==4096);
  CHECK(result.vertex_bytes==(indexed?59:48)&&result.index_offset==(indexed?60:0));
  CHECK(result.index_bytes==(indexed?8:0)&&result.min_vertex==0&&result.max_vertex==2);
  CHECK(!memcmp(f.backing[1].bytes,f.backing[2].bytes,4096));
  CHECK(remesa_persistent_shader_prepare_draw_gpu(&d,token)==EBUSY&&f.creates==3);
  CHECK(remesa_persistent_shader_draw_gpu_backing(&d,token,(void *)token)==EINVAL);
  CHECK(remesa_persistent_close(&d)==EBUSY);
  d.ops->destroy_shader_programs(d.priv,token);
  CHECK(f.destroys==3&&f.unpins==3&&f.gpu_maps==2&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 const enum failure errors[]={FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_PIN,FAIL_MAP,
                             FAIL_MAP_NULL,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL};
 for(unsigned i=0;i<sizeof(errors)/sizeof(errors[0]);i++) {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *token=NULL;open_device(&f,&d,950+i);
  CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
  uint8_t vertices[64]={0};struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,64,NULL);
  CHECK(!remesa_persistent_shader_stage_draw(&d,token,&inputs));
  /* Fail the promotion rather than re-verification of the CPU original. */
  f.fail_map_handle=3;f.fail=errors[i];CHECK(remesa_persistent_shader_prepare_draw_gpu(&d,token)!=0);
  struct remesa_persistent_draw_gpu_backing absent;memset(&absent,0xa5,sizeof(absent));
  CHECK(remesa_persistent_shader_draw_gpu_backing(&d,token,&absent)==EAGAIN);
  f.fail=FAIL_NONE;
  CHECK(!remesa_persistent_shader_prepare_draw_gpu(&d,token));
  d.ops->destroy_shader_programs(d.priv,token);CHECK(!remesa_persistent_close(&d));
 }
}
static void geometry_gpu_uncertain(void)
{
 const enum failure failures[]={FAIL_CREATE_UNCERTAIN,FAIL_PIN_UNCERTAIN,
  FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
  struct fixture *f=calloc(1,sizeof(*f));struct remesa_device *d=calloc(1,sizeof(*d));
  CHECK(f&&d);f->allow_gpu_map=true;f->allow_sync=true;open_device(f,d,980+i);
  struct remesa_shader_draw_programs pair=programs(0xff00ff00);void *token=NULL;
  CHECK(d->ops->prepare_shader_programs(d->priv,&pair,&token)==ENOTSUP&&token);
  uint8_t vertices[48]={0};struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,48,NULL);
  CHECK(!remesa_persistent_shader_stage_draw(d,token,&inputs));
  f->fail_map_handle=3;f->fail=failures[i];
  CHECK(remesa_persistent_shader_prepare_draw_gpu(d,token)==EOWNERDEAD);
  CHECK(remesa_persistent_shader_prepare_draw_gpu(d,token)==EOWNERDEAD);
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(remesa_persistent_close(d)==EBUSY);
  /* Provider uncertainty intentionally retains the entire endpoint, including
   * raw snapshot and possible acquired GPU/CPU leases. Never force-free it. */
 }
}
static void shader_gpu_owned(void)
{
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned c=0;c<4;c++) {
  mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
  mapping.output[c]=(struct remesa_usse_cpu_register){1,c};
 }
 const uint32_t banks[2]={2,3},same_bank[2]={2,2},invalid_bank[2]={16,3};
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
 void *token=NULL;open_device(&f,&d,1100);
 CHECK(remesa_persistent_prepare_vertex_candidate(&d,&pair,&mapping,&token)==ENOTSUP&&token);
 struct remesa_persistent_shader_gpu_backing result,saved;
 memset(&result,0xa5,sizeof(result));saved=result;
 CHECK(remesa_persistent_shader_gpu_backing(&d,token,&result)==EAGAIN);
 CHECK(!memcmp(&result,&saved,sizeof(result)));
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,NULL)==EINVAL);
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,(const uint32_t *)(UINTPTR_MAX-3))==EINVAL);
 uint8_t unaligned[sizeof(banks)+1];memcpy(unaligned+1,banks,sizeof(banks));
 const uint32_t *bad_alignment=(const uint32_t *)((uintptr_t)unaligned|1u);
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,bad_alignment)==EINVAL);
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,same_bank)==EBUSY);
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,invalid_bank)==ERANGE);
 CHECK(f.creates==1&&!f.gpu_maps);
 void *other=NULL,*cpu=NULL;
 CHECK(!d.ops->allocate(d.priv,4096,&other));CHECK(!d.ops->map(d.priv,other,true,&cpu)&&cpu);
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,banks)==EBUSY);
 d.ops->unmap(d.priv,other,true);d.ops->destroy_bo(d.priv,other);
 CHECK(!remesa_persistent_shader_prepare_gpu(&d,token,banks));
 CHECK(!remesa_persistent_shader_gpu_backing(&d,token,&result));
 CHECK(result.owner==1100&&result.generation==1&&result.handle==3);
 CHECK(result.gpu_va==0x100000&&result.allocation_bytes==4096);
 CHECK(result.program_offset[0]==0&&result.program_bytes[0]==16);
 CHECK(result.program_offset[1]==16&&result.program_bytes[1]==8);
 CHECK(result.entry[0].entry_address==result.gpu_va&&result.entry[1].entry_address==result.gpu_va+16);
 CHECK(result.entry[0].bank==2&&result.entry[1].bank==3);
 CHECK(!memcmp(f.backing[0].bytes,f.backing[2].bytes,4096));
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,banks)==EBUSY&&f.creates==3);
 CHECK(remesa_persistent_shader_gpu_backing(&d,token,(void *)token)==EINVAL);
 CHECK(!remesa_persistent_shader_verify(&d,token));
 /* Logical join: both addresses come from the same owned endpoint, not a
  * synthetic VA. This does not prove physical vertex fetch or DATA preload. */
 uint8_t vertices[48]={0},pds[64],known[64];size_t written=0;
 struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),NULL);
 CHECK(!remesa_persistent_shader_stage_draw(&d,token,&inputs));
 CHECK(!remesa_persistent_shader_prepare_draw_gpu(&d,token));
 struct remesa_persistent_draw_gpu_backing raw;
 CHECK(!remesa_persistent_shader_draw_gpu_backing(&d,token,&raw));
 uint32_t use_lower19;
 CHECK(!remesa_use_entry_preview_fixup(&result.entry[0],&use_lower19));
 const struct remesa_pds_vertex_image_request vertex_image={
  .source_allocation_address=raw.gpu_va,.source_allocation_bytes=raw.vertex_bytes,
  .source_offset=inputs.vertex_offset+inputs.element_offset,
  .vertex_stride_dwords=inputs.vertex_stride/4,.use_lower19=use_lower19};
 CHECK(!remesa_pds_structure_encode_vertex_image(&vertex_image,pds,known,sizeof(pds),&written));
 CHECK(written==64&&raw.owner==result.owner&&raw.generation==result.generation);
 uint32_t encoded_source=0,encoded_use=0;
 for(unsigned i=0;i<4;i++){encoded_source|=(uint32_t)pds[i]<<(8*i);encoded_use|=(uint32_t)pds[16+i]<<(8*i);}
 CHECK(encoded_source==raw.gpu_va&&encoded_use==(UINT32_C(0x200000)|use_lower19));
 CHECK(use_lower19==2&&known[0]==255&&known[16]==255&&known[8]==0);

 CHECK(!d.ops->allocate(d.priv,4096,&other));CHECK(!d.ops->map(d.priv,other,true,&cpu));
 saved=result;CHECK(remesa_persistent_shader_gpu_backing(&d,token,&result)==EBUSY);
 CHECK(!memcmp(&result,&saved,sizeof(result)));
 d.ops->unmap(d.priv,other,true);d.ops->destroy_bo(d.priv,other);
 void *fence=(void *)1;
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence);
 CHECK(remesa_persistent_close(&d)==EBUSY);
 d.ops->destroy_shader_programs(d.priv,token);
 CHECK(!remesa_persistent_close(&d)&&f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
 /* FS-only CPU staging remains unpromotable and no extra allocation occurs. */
 memset(&f,0,sizeof(f));memset(&d,0,sizeof(d));token=NULL;
 open_device(&f,&d,1101);
 CHECK(d.ops->prepare_shader_programs(d.priv,&pair,&token)==ENOTSUP&&token);
 CHECK(remesa_persistent_shader_prepare_gpu(&d,token,banks)==ENOTSUP&&f.creates==1);
 d.ops->destroy_shader_programs(d.priv,token);CHECK(!remesa_persistent_close(&d));
 const enum failure failures[]={FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_PIN,FAIL_MAP,
  FAIL_MAP_NULL,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
  memset(&f,0,sizeof(f));memset(&d,0,sizeof(d));token=NULL;
  f.allow_gpu_map=f.allow_sync=true;open_device(&f,&d,1120+i);
  CHECK(remesa_persistent_prepare_vertex_candidate(&d,&pair,&mapping,&token)==ENOTSUP&&token);
  f.fail_map_handle=2;f.fail=failures[i];
  CHECK(remesa_persistent_shader_prepare_gpu(&d,token,banks)!=0);
  memset(&result,0xa5,sizeof(result));saved=result;
  CHECK(remesa_persistent_shader_gpu_backing(&d,token,&result)==EAGAIN);
  CHECK(!memcmp(&result,&saved,sizeof(result)));
  f.fail=FAIL_NONE;CHECK(!remesa_persistent_shader_prepare_gpu(&d,token,banks));
  CHECK(!remesa_persistent_shader_gpu_backing(&d,token,&result));
  d.ops->destroy_shader_programs(d.priv,token);CHECK(!remesa_persistent_close(&d));
 }
}
static void shader_gpu_uncertain(void)
{
 const enum failure failures[]={FAIL_CREATE_UNCERTAIN,FAIL_PIN_UNCERTAIN,
  FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR,FAIL_UNMAP_UNCERTAIN,
  FAIL_DUPLICATE_CREATE,FAIL_DUPLICATE_CREATE_ERROR};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
  struct fixture *f=calloc(1,sizeof(*f));struct remesa_device *d=calloc(1,sizeof(*d));
  CHECK(f&&d);f->allow_gpu_map=f->allow_sync=true;open_device(f,d,1150+i);
  struct remesa_shader_draw_programs pair=programs(0xff00ff00);void *token=NULL;
  struct remesa_usse_vertex_mapping mapping={0};
  for(unsigned c=0;c<4;c++) {
   mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
   mapping.output[c]=(struct remesa_usse_cpu_register){1,c};
  }
  CHECK(remesa_persistent_prepare_vertex_candidate(d,&pair,&mapping,&token)==ENOTSUP&&token);
  const uint32_t banks[2]={2,3};f->fail_map_handle=2;f->fail=failures[i];
  CHECK(remesa_persistent_shader_prepare_gpu(d,token,banks)==EOWNERDEAD);
  CHECK(remesa_persistent_shader_prepare_gpu(d,token,banks)==EOWNERDEAD);
  d->ops->destroy_shader_programs(d->priv,token);CHECK(remesa_persistent_close(d)==EBUSY);
  /* Retain uncertain endpoint exactly like the existing geometry tests. */
 }
}
static void prepare_pds_owners(struct fixture *f,struct remesa_device *d,void **token,
 uint64_t context,const struct remesa_shader_draw_inputs *inputs)
{
 f->allow_gpu_map=f->allow_sync=true;open_device(f,d,context);
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned c=0;c<4;c++) {
  mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
  mapping.output[c]=(struct remesa_usse_cpu_register){1,c};
 }
 const uint32_t banks[2]={2,3};
 CHECK(remesa_persistent_prepare_vertex_candidate(d,&pair,&mapping,token)==ENOTSUP&&*token);
 CHECK(!remesa_persistent_shader_prepare_gpu(d,*token,banks));
 CHECK(!remesa_persistent_shader_stage_draw(d,*token,inputs));
 CHECK(!remesa_persistent_shader_prepare_draw_gpu(d,*token));
}
static uint32_t shader_pds_le32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void pds_gpu_owned(void)
{
 for(unsigned indexed=0;indexed<2;indexed++) {
  struct fixture f={0};struct remesa_device d={0};void *token=NULL;
  uint8_t vertices[64]={0};uint16_t indices[4]={99,2,0,1};
  struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),indexed?indices:NULL);
  inputs.vertex_offset=inputs.element_offset=0;inputs.vertex_stride=16;
  /* Nonindexed start1 must remain an index, not become a shifted source base. */
  inputs.start=1;
  prepare_pds_owners(&f,&d,&token,1200+indexed,&inputs);
  struct remesa_persistent_shader_pds_gpu_backing pds,saved;
  memset(&pds,0xa5,sizeof(pds));saved=pds;
  CHECK(remesa_persistent_shader_pds_gpu_backing(&d,token,&pds)==EAGAIN);
  CHECK(!memcmp(&pds,&saved,sizeof(pds)));
  CHECK(!remesa_persistent_shader_prepare_pds_gpu(&d,token));
  CHECK(!remesa_persistent_shader_pds_gpu_backing(&d,token,&pds));
  struct remesa_persistent_shader_gpu_backing code;
  struct remesa_persistent_draw_gpu_backing raw;
  CHECK(!remesa_persistent_shader_gpu_backing(&d,token,&code));
  CHECK(!remesa_persistent_shader_draw_gpu_backing(&d,token,&raw));
  CHECK(pds.owner==code.owner&&pds.generation==code.generation&&pds.owner==raw.owner);
  CHECK(pds.handle==5&&pds.gpu_va==0x102000&&pds.allocation_bytes==4096);
  CHECK(pds.object_offset[0]==0&&pds.object_offset[1]==64&&pds.object_offset[2]==128);
  CHECK(pds.object_bytes[0]==64&&pds.object_bytes[1]==56&&pds.object_bytes[2]==4);
  CHECK(pds.data_bytes[0]==48&&pds.data_bytes[1]==48&&pds.data_bytes[2]==0);
  CHECK(pds.code_bytes[0]==16&&pds.code_bytes[1]==8&&pds.code_bytes[2]==4);
  CHECK(pds.min_vertex==(indexed?0:1)&&pds.max_vertex==(indexed?2:3)&&pds.vertex_stride==16);
  const uint8_t *bytes=f.backing[4].bytes;
  CHECK(shader_pds_le32(bytes)==raw.gpu_va); /* No extra start*stride. */
  uint32_t fixup[2];
  for(unsigned n=0;n<2;n++)CHECK(!remesa_use_entry_preview_fixup(&code.entry[n],&fixup[n]));
  CHECK(shader_pds_le32(bytes+16)==(UINT32_C(0x200000)|fixup[0]));
  CHECK(shader_pds_le32(bytes+64)==fixup[1]&&fixup[1]==0x103);
  CHECK(pds.use_lower19[0]==fixup[0]&&pds.use_lower19[1]==fixup[1]);
  unsigned known_bytes=0;
  for(unsigned n=0;n<132;n++){CHECK(pds.known_mask[n]==0||pds.known_mask[n]==255);known_bytes+=pds.known_mask[n]!=0;}
  CHECK(known_bytes==52&&pds.known_mask[64]==0&&pds.known_mask[128]==255);
  for(unsigned n=120;n<128;n++)CHECK(bytes[n]==0&&pds.known_mask[n]==0);
  bool zero_tail=true;for(unsigned n=132;n<4096;n++)if(bytes[n]){zero_tail=false;break;}
  CHECK(zero_tail);
  const struct remesa_pds_code_instruction secondary={REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,
   REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};
  uint8_t secondary_code[4];size_t length=0;
  CHECK(!remesa_pds_structure_encode_code(&secondary,1,secondary_code,sizeof(secondary_code),&length));
  CHECK(length==4&&!memcmp(bytes+128,secondary_code,4));
  struct remesa_ta_fragment_binding_request binding={.address_window_base=pds.gpu_va&~UINT64_C(0x0fffffff)};
  binding.primary=(struct remesa_ta_fragment_pds_reference){pds.gpu_va,pds.allocation_bytes,
   pds.object_offset[1],pds.object_bytes[1],12,0,1};
  binding.secondary=(struct remesa_ta_fragment_pds_reference){pds.gpu_va,pds.allocation_bytes,
   pds.object_offset[2],pds.object_bytes[2],0,0,0};
  uint8_t group40[16];CHECK(!remesa_ta_fragment_binding_encode(&binding,group40));
  CHECK(shader_pds_le32(group40)==0x40);
  CHECK(shader_pds_le32(group40+12)==(UINT32_C(0x0c000000)|((pds.gpu_va+64)>>4)));
  CHECK(!remesa_persistent_shader_verify(&d,token));
  CHECK(remesa_persistent_shader_prepare_pds_gpu(&d,token)==EBUSY&&f.creates==5);
  CHECK(remesa_persistent_shader_pds_gpu_backing(&d,token,(void *)token)==EINVAL);
  void *fence=(void *)1;
  CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence);
  d.ops->destroy_shader_programs(d.priv,token);
  CHECK(!remesa_persistent_close(&d)&&f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
 }
 /* Valid host geometry outside the strict PDS stride/bias subset stays owned
  * but is rejected before a new allocation or any code is published. */
 for(unsigned mode=0;mode<2;mode++) {
  struct fixture f={0};struct remesa_device d={0};void *token=NULL;
  uint8_t vertices[64]={0};uint16_t indices[4]={0,0,1,2};
  struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),mode?indices:NULL);
  inputs.vertex_offset=inputs.element_offset=0;inputs.vertex_stride=mode?16:20;
  if(mode)inputs.index_bias=1;
  prepare_pds_owners(&f,&d,&token,1220+mode,&inputs);
  CHECK(remesa_persistent_shader_prepare_pds_gpu(&d,token)==ENOTSUP&&f.creates==4);
  d.ops->destroy_shader_programs(d.priv,token);CHECK(!remesa_persistent_close(&d));
 }
 const enum failure failures[]={FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_PIN,FAIL_MAP,
  FAIL_MAP_NULL,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
  struct fixture f={0};struct remesa_device d={0};void *token=NULL;uint8_t vertices[48]={0};
  struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),NULL);
  prepare_pds_owners(&f,&d,&token,1240+i,&inputs);
  f.fail_map_handle=5;f.fail=failures[i];
  CHECK(remesa_persistent_shader_prepare_pds_gpu(&d,token)!=0);
  struct remesa_persistent_shader_pds_gpu_backing pds;
  CHECK(remesa_persistent_shader_pds_gpu_backing(&d,token,&pds)==EAGAIN);
  f.fail=FAIL_NONE;CHECK(!remesa_persistent_shader_prepare_pds_gpu(&d,token));
  CHECK(!remesa_persistent_shader_pds_gpu_backing(&d,token,&pds));
  d.ops->destroy_shader_programs(d.priv,token);CHECK(!remesa_persistent_close(&d));
 }
}
static void pds_gpu_uncertain(void)
{
 const enum failure failures[]={FAIL_CREATE_UNCERTAIN,FAIL_PIN_UNCERTAIN,
  FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR,FAIL_DUPLICATE_CREATE,FAIL_DUPLICATE_CREATE_ERROR};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
  struct fixture *f=calloc(1,sizeof(*f));struct remesa_device *d=calloc(1,sizeof(*d));
  CHECK(f&&d);void *token=NULL;uint8_t vertices[48]={0};
  struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,sizeof(vertices),NULL);
  prepare_pds_owners(f,d,&token,1270+i,&inputs);
  f->fail_map_handle=5;f->fail=failures[i];
  CHECK(remesa_persistent_shader_prepare_pds_gpu(d,token)==EOWNERDEAD);
  CHECK(remesa_persistent_shader_prepare_pds_gpu(d,token)==EOWNERDEAD);
  d->ops->destroy_shader_programs(d->priv,token);CHECK(remesa_persistent_close(d)==EBUSY);
 }
}
static struct remesa_persistent_constant_state_request constant_state_request(void)
{
 struct remesa_persistent_constant_state_request r={0};
 r.isp=(struct remesa_isp_opaque_triangle_request){true,true,true,REMESA_DEPTH_LESS,15};
 r.viewport=(struct remesa_ta_window_viewport_request){32,32,0,0,32,32,24,0,1,
  REMESA_TA_VIEWPORT_INITIAL_ZERO};
 r.scalar_profile=REMESA_TA_SCALAR_POSITION4;return r;
}
static void owned_scene_state(void)
{
 for(unsigned mode=0;mode<4;mode++) {
  const unsigned lengths[]={56,44,52,16},uses[]={64,48,64,16},pdss[]={96,64,96,32};
  const unsigned kinds[]={REMESA_PERSISTENT_STATE_CONSTANT_DRAW,REMESA_PERSISTENT_STATE_SCENE_ZCLEAR,
   REMESA_PERSISTENT_STATE_AFTER_ZCLEAR,REMESA_PERSISTENT_STATE_FRAGMENT_UPDATE};
  const unsigned headers[]={0xf41,0x54c5,0xb41,0x40};
  struct fixture f={0};struct remesa_device d={0};void *token=NULL,*state=NULL;
  uint8_t vertices[48]={0};struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,48,NULL);
  prepare_pds_owners(&f,&d,&token,1300+mode,&inputs);
  CHECK(!remesa_persistent_shader_prepare_pds_gpu(&d,token));
  struct remesa_persistent_constant_state_request request=constant_state_request();
  int e;
  if(mode==1)e=remesa_persistent_zclear_state_prepare(&d,token,4,&state);
  else if(mode==2)e=remesa_persistent_after_zclear_state_prepare(&d,token,&request,4,&state);
  else if(mode==3)e=remesa_persistent_fragment_state_prepare(&d,token,4,&state);
  else e=remesa_persistent_constant_state_prepare(&d,token,&request,4,&state);
  CHECK(!e&&state&&f.creates==6);
  struct remesa_persistent_scene_state_backing view;
  CHECK(!remesa_persistent_scene_state_snapshot(&d,state,&view));
  CHECK(view.kind==kinds[mode]);
  CHECK(view.image.state_bytes==lengths[mode]&&view.image.use_offset==uses[mode]);
  CHECK(view.image.pds_offset==pdss[mode]&&view.image.ta_offset==pdss[mode]+64);
  CHECK(view.image.use_bytes==16&&view.image.pds_bytes==60&&view.image.ta_bytes==8);
  CHECK(view.shader_owner==1300+mode&&view.shader_generation==1);
  CHECK(view.shader_code_handle==2&&view.shader_pds_handle==5);
  const uint8_t *bytes=f.backing[5].bytes;
  CHECK(shader_pds_le32(bytes)==headers[mode]);
  struct remesa_persistent_isp_update_backing old;
  struct remesa_persistent_scene_close_backing closed;
  CHECK(remesa_persistent_isp_update_snapshot(&d,state,&old)==EINVAL);
  CHECK(remesa_persistent_scene_close_snapshot(&d,state,&closed)==EINVAL);
  struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},
   .control12=lengths[mode]/4-1,.control21=1,.control23=1};
  struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,lengths[mode]/4},{2,0}},
   .source_extension={0,1,1},.word0_control21=1,.word1_control18=1};
  uint8_t utility[16];size_t n;
  CHECK(!remesa_usse_cpu_move_encode(&move,utility,8,&n)&&n==8);
  CHECK(!remesa_usse_cpu_three_source_encode(&tail,utility+8,8,&n)&&n==8);
  CHECK(!memcmp(bytes+view.image.use_offset,utility,16));
  uint8_t ta[8];CHECK(!remesa_ta_state_reference_encode(view.image.gpu_va+view.image.pds_offset,
   lengths[mode]/4,false,ta));CHECK(!memcmp(bytes+view.image.ta_offset,ta,8));
  void *write_map=(void *)1;
  CHECK(d.ops->map(d.priv,state,true,&write_map)==ENOTSUP&&!write_map);
  unsigned destroys=f.destroys,unpins=f.unpins;
  d.ops->destroy_shader_programs(d.priv,token);
  CHECK(f.destroys==destroys&&f.unpins==unpins); /* Internal dependency survives deletion. */
  struct remesa_persistent_shader_gpu_backing invalid;
  CHECK(remesa_persistent_shader_gpu_backing(&d,token,&invalid)==EINVAL);
  CHECK(!remesa_persistent_scene_state_snapshot(&d,state,&view));
  void *read_map=NULL;CHECK(!d.ops->map(d.priv,state,false,&read_map)&&read_map);
  d.ops->destroy_bo(d.priv,state);
  CHECK(f.destroys==destroys&&f.unpins==unpins); /* CPU lease defers actual state disposal too. */
  CHECK(remesa_persistent_close(&d)==EBUSY);
  d.ops->unmap(d.priv,state,false);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 const enum failure ordinary[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_MAP,FAIL_READ_BODY};
 for(unsigned i=0;i<sizeof(ordinary)/sizeof(ordinary[0]);i++) {
  struct fixture f={0};struct remesa_device d={0};void *token=NULL,*state=NULL;
  uint8_t vertices[48]={0};struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,48,NULL);
  prepare_pds_owners(&f,&d,&token,1320+i,&inputs);
  CHECK(!remesa_persistent_shader_prepare_pds_gpu(&d,token));
  f.fail_map_handle=6;f.fail=ordinary[i];
  CHECK(remesa_persistent_zclear_state_prepare(&d,token,4,&state)!=0&&!state);
  f.fail=FAIL_NONE;d.ops->destroy_shader_programs(d.priv,token);
  CHECK(!remesa_persistent_close(&d)&&f.creates==f.destroys); /* No leaked dependency ref. */
 }
 const enum failure uncertain[]={FAIL_MAP_UNCERTAIN,FAIL_MAP_NONNULL_ERROR};
 for(unsigned i=0;i<sizeof(uncertain)/sizeof(uncertain[0]);i++) {
  struct fixture *f=calloc(1,sizeof(*f));struct remesa_device *d=calloc(1,sizeof(*d));
  CHECK(f&&d);void *token=NULL,*state=NULL;uint8_t vertices[48]={0};
  struct remesa_shader_draw_inputs inputs=geometry_inputs(vertices,48,NULL);
  prepare_pds_owners(f,d,&token,1340+i,&inputs);
  CHECK(!remesa_persistent_shader_prepare_pds_gpu(d,token));
  unsigned destroys=f->destroys;f->fail_map_handle=6;f->fail=uncertain[i];
  CHECK(remesa_persistent_zclear_state_prepare(d,token,4,&state)==EOWNERDEAD&&!state);
  d->ops->destroy_shader_programs(d->priv,token);
  CHECK(f->destroys==destroys&&remesa_persistent_close(d)==EBUSY);
 }
}
int main(void)
{
 owned_scene_state();
 pds_gpu_owned();pds_gpu_uncertain();
 shader_gpu_owned();shader_gpu_uncertain();
 geometry_gpu_owned();geometry_gpu_uncertain();
 staged();ordinary_failures();uncertain_failures();
 geometry_staged();geometry_ordinary_failures();geometry_uncertain_failures();
 vertex_staged();
 cookie_lifetime_and_cross_token_outputs();
 printf("PASS: %u persistent original-shader staging checks; no GPU operations\n",checks);
 return 0;
}
