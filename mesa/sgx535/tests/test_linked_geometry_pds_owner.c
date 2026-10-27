/* SPDX-License-Identifier: MIT. Storage/lifetime prerequisite; GPU executions0. */
int persistent_geometry_regressions(void);
#define main persistent_geometry_regressions
#include "test_persistent_shader.c"
#undef main
int geometry_linkage_regressions(void);
#define main geometry_linkage_regressions
#include "test_shader_linkage.c"
#undef main
static void open_link_owner(struct fixture *f,struct remesa_device *d,uint64_t context)
{
 struct remesa_persistent_config cfg={.ops=&ops,.priv=f,.context=context,
  .va_start=0x20000000,.va_end=0x20100000,.byte_budget=8*4096,.resource_only_backend=true};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};
 CHECK(!remesa_persistent_open(d,&cfg));
}
static struct remesa_shader_linkage linked(unsigned kind)
{
 struct remesa_shader_program v=vertex(2),f=fragment(kind);
 struct remesa_shader_linkage link;CHECK(!remesa_shader_linkage_prepare(&v,&f,&link));return link;
}

static void raw_input(unsigned char *bytes,size_t n)
{ for(size_t i=0;i<n;i++)bytes[i]=(unsigned char)(i*37+9); }

int main(void)
{
 CHECK(!persistent_geometry_regressions());CHECK(!geometry_linkage_regressions());
 const uint32_t banks[2]={4,3};
 unsigned char vertices[6*48];raw_input(vertices,sizeof(vertices));
 const uint16_t indices[6]={5,2,0,1,3,4};
 for(unsigned indexed=0;indexed<2;indexed++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  open_link_owner(&f,&d,2000+indexed);
  struct remesa_linked_geometry_input input={.vertices=vertices,.vertex_bytes=sizeof(vertices),
   .vertex_count=6,.indices=indexed?indices:NULL,.index_count=indexed?6:0,
   .index_bytes=indexed?sizeof(indices):0};
  void *code=NULL,*geometry=NULL,*pds=NULL;struct remesa_shader_linkage link=linked(1);
  CHECK(!remesa_persistent_linked_program_prepare(&d,&link,banks,&code));
  CHECK(!remesa_persistent_linked_geometry_prepare(&d,&input,&geometry));
  struct remesa_persistent_linked_geometry_backing g;
  CHECK(!remesa_persistent_linked_geometry_snapshot(&d,geometry,&g));
  CHECK(g.vertex_count==6&&g.index_count==6&&g.stride_bytes==48&&g.min_vertex==0&&g.max_vertex==5);
  CHECK(g.vertex_bytes==sizeof(vertices)&&g.index_offset==sizeof(vertices)&&g.index_bytes==12);
  struct backing *raw=get(&f,g.handle);CHECK(!memcmp(raw->bytes,vertices,sizeof(vertices)));
  for(unsigned i=0;i<6;i++){
   CHECK(raw->bytes[g.index_offset+2*i]==(indexed?indices[i]:i));
   CHECK(!raw->bytes[g.index_offset+2*i+1]);
  }
  for(size_t i=g.index_offset+12;i<raw->length;i++)CHECK(!raw->bytes[i]);
  CHECK(!remesa_persistent_linked_pds_prepare(&d,code,geometry,&pds)&&pds);
  struct remesa_persistent_linked_pds_backing p;
  CHECK(!remesa_persistent_linked_pds_snapshot(&d,pds,&p));
  CHECK(p.geometry.handle==g.handle&&p.geometry.gpu_va==g.gpu_va);
  CHECK(p.object_offset[0]==0&&p.object_offset[1]==192);
  CHECK(p.image_bytes[0]==64&&p.image_bytes[1]==60&&p.reserved_bytes[0]==192&&p.reserved_bytes[1]==64);
  CHECK(!remesa_pds_selected_pair_validate(p.plans));
  unsigned char encoded[256]={0},known[256]={0};size_t n;
  CHECK(!remesa_pds_selected_encode(p.plans,encoded,known,192,&n)&&n==64);
  CHECK(!remesa_pds_selected_encode(p.plans+1,encoded+192,known+192,64,&n)&&n==60);
  CHECK(!memcmp(encoded,get(&f,p.handle)->bytes,256)&&!memcmp(known,p.known_mask,256));
  for(unsigned i=64;i<192;i++)CHECK(!encoded[i]&&!known[i]);
  for(unsigned i=252;i<256;i++)CHECK(!encoded[i]&&!known[i]);
  unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
  for(unsigned i=0;i<100;i++)CHECK(!remesa_persistent_linked_pds_snapshot(&d,pds,&p));
  CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes);
  void *cpu=(void *)1;CHECK(d.ops->map(d.priv,pds,true,&cpu)==ENOTSUP&&!cpu);
  CHECK(d.ops->map(d.priv,geometry,true,&cpu)==ENOTSUP&&!cpu);
  /* The closure outlives callers' original references, then a read lease. */
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);
  CHECK(!f.destroys&&!remesa_persistent_linked_pds_snapshot(&d,pds,&p));
  CHECK(!d.ops->map(d.priv,pds,false,&cpu)&&cpu);
  d.ops->destroy_bo(d.priv,pds);CHECK(!f.destroys);
  CHECK(remesa_persistent_close(&d)==EBUSY);
  d.ops->unmap(d.priv,pds,false);
  CHECK(f.creates==3&&f.destroys==3&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 /* Input rejection occurs before provider calls. */
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  open_link_owner(&f,&d,2010);void *geometry=NULL;
  struct remesa_linked_geometry_input in={.vertices=vertices,.vertex_bytes=143,.vertex_count=3};
  CHECK(remesa_persistent_linked_geometry_prepare(&d,&in,&geometry)==ENOSPC&&!geometry&&!f.creates);
  in.vertex_bytes=sizeof(vertices);in.vertex_count=4;
  CHECK(remesa_persistent_linked_geometry_prepare(&d,&in,&geometry)==ENOTSUP&&!f.creates);
  in.vertex_count=3;in.indices=indices;in.index_count=6;in.index_bytes=sizeof(indices);
  CHECK(remesa_persistent_linked_geometry_prepare(&d,&in,&geometry)==ERANGE&&!f.creates);
  in.index_bytes=2;
  CHECK(remesa_persistent_linked_geometry_prepare(&d,&in,&geometry)==EINVAL&&!f.creates);
  in.vertex_count=REMESA_LINKED_MAX_VERTICES+1;
  CHECK(remesa_persistent_linked_geometry_prepare(&d,&in,&geometry)==EINVAL&&!f.creates);
  CHECK(!remesa_persistent_close(&d));
 }
 /* PDS failures roll back their dependency holds, without permitting dispatch. */
 const enum failure failures[]={FAIL_CREATE,FAIL_PIN,FAIL_MAP,FAIL_READ_BODY,FAIL_READ_TAIL};
 for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  open_link_owner(&f,&d,2020+i);void *code=NULL,*geometry=NULL,*pds=NULL;
  struct remesa_shader_linkage link=linked(1);
  struct remesa_linked_geometry_input in={.vertices=vertices,.vertex_bytes=sizeof(vertices),.vertex_count=3};
  CHECK(!remesa_persistent_linked_program_prepare(&d,&link,banks,&code));
  CHECK(!remesa_persistent_linked_geometry_prepare(&d,&in,&geometry));
  f.fail=failures[i];CHECK(remesa_persistent_linked_pds_prepare(&d,code,geometry,&pds)!=0&&!pds);
  f.fail=FAIL_NONE;d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 /* A second PDS closure keeps the same dependencies after the first is gone. */
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  struct fixture foreign_f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device foreign={0};
  open_link_owner(&f,&d,2040);open_link_owner(&foreign_f,&foreign,2041);
  void *code=NULL,*geometry=NULL,*first=NULL,*second=NULL,*bad=NULL;
  struct remesa_shader_linkage link=linked(1);
  struct remesa_linked_geometry_input in={.vertices=vertices,.vertex_bytes=sizeof(vertices),.vertex_count=3};
  CHECK(!remesa_persistent_linked_program_prepare(&d,&link,banks,&code));
  CHECK(!remesa_persistent_linked_geometry_prepare(&d,&in,&geometry));
  CHECK(remesa_persistent_linked_pds_prepare(&foreign,code,geometry,&bad)==EINVAL&&!bad&&!foreign_f.creates);
  CHECK(!remesa_persistent_close(&foreign));
  CHECK(!remesa_persistent_linked_pds_prepare(&d,code,geometry,&first));
  CHECK(!remesa_persistent_linked_pds_prepare(&d,code,geometry,&second));
  struct remesa_persistent_linked_pds_backing p,q;
  CHECK(!remesa_persistent_linked_pds_snapshot(&d,first,&p));
  CHECK(!remesa_persistent_linked_pds_snapshot(&d,second,&q));
  CHECK(p.handle!=q.handle&&p.generation!=q.generation&&p.program.handle==q.program.handle&&
   p.geometry.handle==q.geometry.handle);
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);
  d.ops->destroy_bo(d.priv,first);CHECK(f.destroys==1);
  CHECK(!remesa_persistent_linked_pds_snapshot(&d,second,&q));
  d.ops->destroy_bo(d.priv,second);
  CHECK(f.creates==4&&f.destroys==4&&f.pins==f.unpins&&!f.forbidden);
  CHECK(!remesa_persistent_close(&d));
 }
 /* Uncertain effects retain the entire closure, including fetched inputs.
  * These are intentionally leaked fake providers, never a hardware test. */
 const enum failure uncertain[]={FAIL_MAP_UNCERTAIN,FAIL_UNMAP_UNCERTAIN,FAIL_UNPIN};
 for(unsigned i=0;i<sizeof(uncertain)/sizeof(uncertain[0]);i++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  open_link_owner(&f,&d,2050+i);void *code=NULL,*geometry=NULL,*pds=NULL;
  struct remesa_shader_linkage link=linked(1);
  struct remesa_linked_geometry_input in={.vertices=vertices,.vertex_bytes=sizeof(vertices),.vertex_count=3};
  CHECK(!remesa_persistent_linked_program_prepare(&d,&link,banks,&code));
  CHECK(!remesa_persistent_linked_geometry_prepare(&d,&in,&geometry));
  if(uncertain[i]==FAIL_UNPIN){
   CHECK(!remesa_persistent_linked_pds_prepare(&d,code,geometry,&pds));
   f.fail=FAIL_UNPIN;
  }else{
   f.fail=uncertain[i];
   CHECK(remesa_persistent_linked_pds_prepare(&d,code,geometry,&pds)==EOWNERDEAD&&pds);
  }
  d.ops->destroy_bo(d.priv,code);d.ops->destroy_bo(d.priv,geometry);
  d.ops->destroy_bo(d.priv,pds);
  CHECK(f.creates==3&&!f.destroys&&!f.unpins&&!f.forbidden);
  for(unsigned n=1;n<=3;n++)CHECK(get(&f,n)->live&&get(&f,n)->pinned);
  CHECK(remesa_persistent_close(&d)==EBUSY);
 }
 puts("PASS raw geometry/PDS owners: unchanged vertex bytes, LE bounded indices, derived bindings, effective/reserved distinction, zero padding, metadata without IO, shared dependency/lease lifetime, foreign-owner rejection, ordinary rollback and uncertain closure retention; GPU0");
 return 0;
}
