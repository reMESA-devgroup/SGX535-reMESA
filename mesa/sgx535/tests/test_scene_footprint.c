/* SPDX-License-Identifier: MIT. GPU-reference metadata, no execution. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#define REMESA_LINKED_TEST_BYTES (96u*1024u*1024u)
#define REMESA_LINKED_TEST_RASTER_END 0x34000000
#define REMESA_LINKED_TEST_RASTER_ALIGNMENT 1048576
#define REMESA_LINKED_TEST_ALLOCATION_MAX 0x2000000
#include "test_linked_background.c"
static unsigned find(const struct remesa_persistent_scene_footprint *r,uint32_t id)
{for(unsigned i=0;i<r->allocation_count;i++)if(r->allocations[i].handle==id)return i;CHECK(0);return 0;}
static void rejected(struct remesa_device *d,void **p,int error)
{
 struct remesa_persistent_scene_footprint r,saved;memset(&r,0xa5,sizeof(r));saved=r;
 CHECK(remesa_persistent_scene_footprint(d,p[0],p[1],p[2],p[3],&r)==error&&!memcmp(&r,&saved,sizeof(r)));
}
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[3][2]={{1280,800},{640,640},{641,479}};
 for(unsigned v=0;v<3;v++){
  unsigned w=sizes[v][0],h=sizes[v][1];struct fixture f={.allow_gpu_map=true,.allow_sync=true};
  struct remesa_device d={0};void *c[11]={0},*p[4]={0},*table=NULL,*parameter=NULL;
  bg_setup(&f,&d,3100+v,w,h,c);
  CHECK(!remesa_persistent_linked_background_prepare(&d,c[10],3,&p[0]));
  CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&p[1]));
  CHECK(!remesa_persistent_linked_region_prepare(&d,c[10],&p[2]));
  CHECK(!remesa_persistent_allocate_placed(&d,0x2000000,REMESA_VA_SURFACE,&table));
  CHECK(!remesa_persistent_allocate_placed(&d,0x2000000,REMESA_VA_RASTER,&parameter));
  CHECK(!remesa_persistent_linked_dpm_prepare(&d,table,parameter,&p[3])&&f.creates==18);
  struct remesa_persistent_scene_footprint r;
  unsigned maps=f.cpu_maps,creates=f.creates,gpu_maps=f.gpu_maps;
  CHECK(!remesa_persistent_scene_footprint(&d,p[0],p[1],p[2],p[3],&r));
  CHECK(r.owner==3100+v&&r.width==w&&r.height==h&&r.draw_count==1&&r.allocation_count==16);
  CHECK(r.incomplete==(REMESA_SCENE_NEEDS_EVENT_PDS|REMESA_SCENE_NEEDS_COLOR_LOAD_INIT|REMESA_SCENE_NEEDS_DPM_INIT|REMESA_SCENE_NEEDS_RASTER_CONTROL|REMESA_SCENE_NEEDS_NATIVE_ADMISSION));
  unsigned seen=0,roles=0,reads=0,rw=0;
  for(unsigned i=0;i<r.allocation_count;i++){
   unsigned id=r.allocations[i].handle;CHECK(id&&id<19&&id!=6&&id!=18&&!(seen&(1u<<id)));seen|=1u<<id;roles|=r.allocations[i].roles;
   CHECK(r.allocations[i].generation&&r.allocations[i].logical_bytes&&r.allocations[i].logical_bytes<=r.allocations[i].allocation_bytes);
   CHECK(!(r.allocations[i].gpu_va&4095)&&!(r.allocations[i].allocation_bytes&4095));
   if(r.allocations[i].access==REMESA_ACCESS_READ)reads++;
   else{CHECK(r.allocations[i].access==(REMESA_ACCESS_READ|REMESA_ACCESS_WRITE));rw++;}
   for(unsigned j=0;j<i;j++)CHECK(r.allocations[i].gpu_va>=r.allocations[j].gpu_va+r.allocations[j].allocation_bytes||r.allocations[j].gpu_va>=r.allocations[i].gpu_va+r.allocations[i].allocation_bytes);
  }
  CHECK(roles==0x3ff&&reads==11&&rw==5);
  CHECK(r.allocations[find(&r,9)].roles==REMESA_SCENE_REF_COLOR&&r.allocations[find(&r,10)].roles==REMESA_SCENE_REF_DEPTH);
  CHECK(r.allocations[find(&r,15)].roles==REMESA_SCENE_REF_REGION&&r.allocations[find(&r,16)].roles==REMESA_SCENE_REF_DPM_TABLE&&r.allocations[find(&r,17)].roles==REMESA_SCENE_REF_DPM_PARAMETER);
  CHECK(get(&f,9)->bytes[0]==0xa5&&get(&f,10)->bytes[0]==0xa5&&get(&f,16)->bytes[0]==0xa5&&get(&f,17)->bytes[0]==0xa5);
  struct remesa_persistent_scene_footprint saved=r;
  for(unsigned i=0;i<100;i++){CHECK(!remesa_persistent_scene_footprint(&d,p[0],p[1],p[2],p[3],&r));CHECK(!memcmp(&r,&saved,sizeof(r)));}
  CHECK(f.cpu_maps==maps&&f.creates==creates&&f.gpu_maps==gpu_maps);
  CHECK(remesa_persistent_scene_footprint(&d,p[0],p[1],p[2],p[3],(void *)((uint8_t *)&r+1))==EINVAL&&!memcmp(&r,&saved,sizeof(r)));
  CHECK(remesa_persistent_scene_footprint(&d,p[0],p[1],p[2],p[3],p[0])==EINVAL);
  void *bad[4]={p[1],p[0],p[2],p[3]};rejected(&d,bad,EINVAL);
  bad[0]=p[0];bad[1]=p[0];rejected(&d,bad,EINVAL);
  void *cpu=NULL;CHECK(!d.ops->map(d.priv,p[2],false,&cpu));rejected(&d,p,EBUSY);d.ops->unmap(d.priv,p[2],false);
  if(!v){
   void *other_targets=NULL,*other[3]={0};
   CHECK(!remesa_persistent_linked_targets_prepare(&d,c[7],c[8],c[9],(w+31u)&~31u,&other_targets));
   CHECK(!remesa_persistent_linked_background_prepare(&d,other_targets,3,&other[0]));
   CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,other_targets,3,&other[1]));
   CHECK(!remesa_persistent_linked_region_prepare(&d,other_targets,&other[2]));
   for(unsigned i=0;i<3;i++){void *mix[4]={p[0],p[1],p[2],p[3]};mix[i]=other[i];rejected(&d,mix,EINVAL);}
   for(unsigned i=0;i<3;i++)d.ops->destroy_bo(d.priv,other[i]);d.ops->destroy_bo(d.priv,other_targets);
   /* Two draws share the same state/program/geometry. Their GPU-reference
    * union must remain16 allocations, while command metadata records2 draws. */
   void *body2=NULL,*ta2=NULL,*targets2=NULL,*q[3]={0};
   const struct remesa_linked_body_draw draws[2]={{c[3],0,3},{c[3],0,3}};
   CHECK(!remesa_persistent_linked_body_prepare(&d,draws,2,c[4],&body2));
   CHECK(!remesa_persistent_linked_ta_prepare(&d,c[6],body2,&ta2));
   CHECK(!remesa_persistent_linked_targets_prepare(&d,ta2,c[8],c[9],(w+31u)&~31u,&targets2));
   CHECK(!remesa_persistent_linked_background_prepare(&d,targets2,3,&q[0]));
   CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,targets2,3,&q[1]));
   CHECK(!remesa_persistent_linked_region_prepare(&d,targets2,&q[2]));
   struct remesa_persistent_scene_footprint two;
   CHECK(!remesa_persistent_scene_footprint(&d,q[0],q[1],q[2],p[3],&two)&&two.draw_count==2&&two.allocation_count==16&&two.incomplete==r.incomplete);
   for(unsigned i=0;i<3;i++)d.ops->destroy_bo(d.priv,q[i]);
   d.ops->destroy_bo(d.priv,targets2);d.ops->destroy_bo(d.priv,ta2);d.ops->destroy_bo(d.priv,body2);
  }
  d.ops->destroy_bo(d.priv,table);d.ops->destroy_bo(d.priv,parameter);bg_drop_children(&d,c);
  CHECK(!remesa_persistent_scene_footprint(&d,p[0],p[1],p[2],p[3],&r)&&!memcmp(&r,&saved,sizeof(r)));
  struct remesa_device foreign={0};struct fixture ff={.allow_gpu_map=true,.allow_sync=true};
  struct remesa_persistent_config fc={.ops=&target_ops,.priv=&ff,.context=3200+v,.va_start=0x20000000,.va_end=0x45000000,.byte_budget=1u<<20,.resource_only_backend=true};
  CHECK(!remesa_persistent_open(&foreign,&fc));rejected(&foreign,p,EINVAL);CHECK(!remesa_persistent_close(&foreign));
  for(unsigned i=0;i<4;i++)d.ops->destroy_bo(d.priv,p[i]);
  rejected(&d,p,EINVAL);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 printf("PASS coherent scene footprint:3 size storage graphs,16 unique conservative GPU references/10roles, READ versus READ|WRITE, no body/DPM-plan false fetch claims, same-target identity and bank checks, zero allocation/providerIO, lease/foreign/type/alias/stale refusal and persistent unresolved admission flags; GPU0/API0 (%uchecks)\n",checks);return 0;
}
