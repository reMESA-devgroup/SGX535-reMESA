/* SPDX-License-Identifier: MIT. Actual frozen owners/provider; receipts modeled.
 * No kernel, device, rasterizer, hardware completion or rendered-pixel claim. */
#include "remesa_persistent.h"
#include <stdlib.h>
static enum remesa_dispatch native_scene(void *,const struct remesa_gpu_scene_submit *,struct remesa_gpu_scene_receipt *);
static remesa_gpu_scene_fn configured_scene=native_scene;
#define REMESA_LINKED_TEST_NATIVE_SCENE configured_scene
#define REMESA_LINKED_BACKGROUND_FIXTURE
#define REMESA_LINKED_TEST_CLEAR_DEPTH(context) 1.0f
#define REMESA_LINKED_TEST_BYTES (96u*1024u*1024u)
#define REMESA_LINKED_TEST_RASTER_END 0x34000000
#define REMESA_LINKED_TEST_RASTER_ALIGNMENT 1048576
#define REMESA_LINKED_TEST_ALLOCATION_MAX 0x2000000
#include "test_linked_background.c"
static unsigned mode,calls;static uint64_t last_operation;
static enum remesa_dispatch native_scene(void *priv,const struct remesa_gpu_scene_submit *scene,struct remesa_gpu_scene_receipt *r)
{
 (void)priv;calls++;CHECK(scene&&scene->packet&&scene->context==scene->packet->owner&&scene->operation>last_operation);last_operation=scene->operation;
 CHECK(scene->packet->binding_count==17&&scene->packet->use_count==10&&scene->packet->window_count==2&&remesa_scene_packet_valid(scene->packet));
 if(mode==1)return REMESA_NOT_ISSUED;
 if(mode==2)return REMESA_ISSUANCE_AMBIGUOUS;
 *r=(struct remesa_gpu_scene_receipt){.context=scene->context,.operation=scene->operation,.retired=true,.published=true,.acknowledged=true};
 if(mode==13)r->acknowledged=false;
 if(mode==3)r->context++;if(mode==4)r->operation++;if(mode==5)r->retired=false;if(mode==6)r->published=false;if(mode==7)r->failed=true;
 return mode==8?(enum remesa_dispatch)99:REMESA_ISSUED;
}
struct graph {void *children[11],*parts[4],*table,*parameter,*event;struct remesa_scene_capture capture;};
static void graph_setup(struct fixture *f,struct remesa_device *d,struct graph *g)
{
 bg_setup(f,d,8100,1280,800,g->children);
 CHECK(!remesa_persistent_linked_background_prepare(d,g->children[10],3,&g->parts[0]));
 CHECK(!remesa_persistent_linked_event_auxiliary_prepare(d,g->children[10],3,&g->parts[1]));
 CHECK(!remesa_persistent_linked_region_allocate(d,g->children[10],&g->parts[2]));
 CHECK(!remesa_persistent_allocate_placed(d,0x2000000,REMESA_VA_SURFACE,&g->table));CHECK(!remesa_persistent_allocate_placed(d,0x2000000,REMESA_VA_RASTER,&g->parameter));
 CHECK(!remesa_persistent_linked_dpm_prepare(d,g->table,g->parameter,&g->parts[3]));CHECK(!remesa_persistent_linked_event_profile_prepare(d,g->parts[1],&g->event));
 CHECK(!remesa_persistent_scene_capture(d,g->parts[0],g->event,g->parts[2],g->parts[3],&g->capture));
}
static void owners_drop(struct remesa_device *d,struct graph *g)
{
 d->ops->destroy_bo(d->priv,g->table);d->ops->destroy_bo(d->priv,g->parameter);
 bg_drop_children(d,g->children);for(unsigned i=0;i<4;i++)d->ops->destroy_bo(d->priv,g->parts[i]);d->ops->destroy_bo(d->priv,g->event);
}
int main(int argc,char **argv)
{
 CHECK(argc==2);mode=(unsigned)strtoul(argv[1],NULL,10);CHECK(mode<=13);
 /* Finite initialized/finalized inputs through the existing fixture owners.
  * No exporter is called and no files/device operations occur. */
 CHECK(!setenv("REMESA_CAPTURE_EXPORT_DIR","/UNWRITTEN-MODEL",1));CHECK(!setenv("REMESA_CAPTURE_FINITE_VERTICES","1",1));CHECK(!setenv("REMESA_CAPTURE_INITIALIZED_PREFIX","1",1));CHECK(!setenv("REMESA_CAPTURE_FINALIZED_DEPTH","1",1));
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};struct graph g={0};
 if(mode==10)configured_scene=NULL;graph_setup(&f,&d,&g);
 CHECK(d.resource_only_backend&&!d.qualified_hardware_backend&&!d.offline_test_backend);
 CHECK(remesa_persistent_set_native_scene(&d,NULL)==EINVAL);CHECK(remesa_persistent_set_native_scene(&d,native_scene)==EBUSY);
 unsigned before_reads=f.reads,before_writes=f.writes,before_creates=f.creates,before_unpins=f.unpins;
 void *fence=NULL;
 if(mode==9){
  void *nonempty=(void *)1;CHECK(remesa_persistent_scene_capture_submit(&d,g.capture,&nonempty)==EBUSY&&nonempty==(void *)1);
  struct remesa_scene_capture bad=g.capture;bad.owner++;CHECK(remesa_persistent_scene_capture_submit(&d,bad,&fence)==EINVAL&&!fence);
  bad=g.capture;bad.serial++;CHECK(remesa_persistent_scene_capture_submit(&d,bad,&fence)==ESTALE&&!fence);
  CHECK(remesa_persistent_scene_capture_submit(&d,g.capture,(void *)g.children[10])==EINVAL&&!calls);mode=0;
 }
 if(mode==11){
  void *fences[8]={0};
  for(unsigned i=0;i<8;i++){
   CHECK(!remesa_persistent_scene_capture_submit(&d,g.capture,&fences[i])&&fences[i]);CHECK(!d.ops->wait(d.priv,fences[i],0,true));
   CHECK(!remesa_persistent_scene_capture(&d,g.parts[0],g.event,g.parts[2],g.parts[3],&g.capture));
  }
  CHECK(remesa_persistent_scene_capture_submit(&d,g.capture,&fence)==EBUSY&&!fence&&calls==8);CHECK(!remesa_persistent_scene_capture_release(&d,g.capture));
  d.ops->destroy_fence(d.priv,fences[0]);CHECK(!remesa_persistent_scene_capture(&d,g.parts[0],g.event,g.parts[2],g.parts[3],&g.capture));CHECK(!remesa_persistent_scene_capture_submit(&d,g.capture,&fence)&&fence&&calls==9);
  for(unsigned i=1;i<8;i++)d.ops->destroy_fence(d.priv,fences[i]);d.ops->destroy_fence(d.priv,fence);owners_drop(&d,&g);CHECK(!remesa_persistent_close(&d));puts("PASS eight held published scene fences, ninth refusal, slot reuse/monotonic identity; CPU model/GPU0");return 0;
 }
 if(mode==12)owners_drop(&d,&g);
 int error=remesa_persistent_scene_capture_submit(&d,g.capture,&fence);
 CHECK(f.reads==before_reads&&f.writes==before_writes&&f.creates==before_creates);
 if(mode==1||mode==10){
  CHECK(error==EOPNOTSUPP&&!fence&&calls==(mode==1?1u:0u)&&f.unpins==before_unpins);
  CHECK(!remesa_persistent_scene_capture_release(&d,g.capture));owners_drop(&d,&g);CHECK(!remesa_persistent_close(&d));puts("PASS unissued/no-native scene preserves capture/cancellation and normal draw gate; GPU0");return 0;
 }
 if((mode>=2&&mode<=8)||mode==13){
  CHECK(error==EOWNERDEAD&&fence&&calls==1&&f.unpins==before_unpins);
  CHECK(d.ops->wait(d.priv,fence,0,true)==EOWNERDEAD);CHECK(remesa_persistent_scene_capture_release(&d,g.capture)==EOWNERDEAD);
  owners_drop(&d,&g);d.ops->destroy_fence(d.priv,fence);CHECK(f.unpins==before_unpins&&f.destroys==0&&remesa_persistent_close(&d)==EBUSY);
  puts("PASS ambiguous/malformed native receipt retains failed fence and whole actual scene graph; process exits with MODELED holds only, GPU0");return 0;
 }
 CHECK(!error&&fence&&calls==1);CHECK(!d.ops->wait(d.priv,fence,0,true));CHECK(remesa_persistent_scene_capture_release(&d,g.capture)==ESTALE);
 if(mode!=12){void *cpu=NULL;CHECK(!d.ops->map(d.priv,g.children[8],false,&cpu)&&cpu);d.ops->unmap(d.priv,g.children[8],false);owners_drop(&d,&g);}
 CHECK(f.creates==f.destroys&&f.pins==f.unpins);CHECK(remesa_persistent_close(&d)==EBUSY);d.ops->destroy_fence(d.priv,fence);CHECK(!remesa_persistent_close(&d));
 puts("PASS actual17-resource frozen scene -> provider retirement/publication/ack -> consumed capture/published device fence, complete owner cleanup; no extra provider IO; CPU model/GPU0");
}
