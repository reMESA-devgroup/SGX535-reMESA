/* SPDX-License-Identifier: MIT. Existing encoders/owners, CPU provider/GPU0. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#define REMESA_LINKED_TEST_BYTES (96u*1024u*1024u)
#define REMESA_LINKED_TEST_RASTER_END 0x34000000
#define REMESA_LINKED_TEST_RASTER_ALIGNMENT 1048576
#define REMESA_LINKED_TEST_ALLOCATION_MAX 0x2000000
#include "test_linked_background.c"
#include "scene_capture_export.inc"
#include <math.h>
static unsigned fail_at,fail_uncertain;
static int builder_create(void *priv,size_t n,uint32_t *id)
{
 struct fixture *f=priv;
 if(fail_at&&f->creates+1==fail_at){
  if(!fail_uncertain)return ENOMEM;
  int e=target_create(priv,n,id);CHECK(!e);return EOWNERDEAD;
 }
 return target_create(priv,n,id);
}
static enum remesa_dispatch complete(void *priv,const struct remesa_gpu_scene_submit *s,struct remesa_gpu_scene_receipt *r)
{
 (void)priv;CHECK(remesa_scene_packet_valid(s->packet)&&s->packet->binding_count==17);
 *r=(struct remesa_gpu_scene_receipt){.context=s->context,.operation=s->operation,.retired=true,.published=true,.acknowledged=true};return REMESA_ISSUED;
}
#ifndef REMESA_SCENE_BUILDER_FIXTURE
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned mode=(unsigned)strtoul(argv[1],NULL,10);CHECK(mode<13);
 unsigned width=mode==1?640:mode==2?641:1280,height=mode==1?640:mode==2?479:800,pitch=(width+31)&~31u;
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};target_ops=ops;target_ops.create=builder_create;struct remesa_device d={0};
 struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&f,.context=9100,.va_start=0x20000000,.va_end=0x45000000,.byte_budget=96u*1024u*1024u,.resource_only_backend=true,.resource_limits={2048,2048,128,16u*1024u*1024u}};
 cfg.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20080000,0x20090000,4096};cfg.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30000000,0x34000000,1048576};cfg.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40001000,0x45000000,4096};
 if(mode==9)cfg.byte_budget=16u*1024u*1024u;
 CHECK(!remesa_persistent_open(&d,&cfg));CHECK(!remesa_persistent_set_native_scene(&d,complete));void *color=NULL,*depth=NULL;
 CHECK(!remesa_persistent_allocate_placed(&d,(size_t)pitch*height*4,REMESA_VA_SURFACE,&color));CHECK(!remesa_persistent_depth_allocate(&d,width,height,4,&depth));
 struct remesa_persistent_memory_backing color_meta;struct remesa_persistent_depth_backing depth_meta;
 CHECK(!remesa_persistent_memory_snapshot(&d,color,&color_meta));CHECK(!remesa_persistent_depth_snapshot(&d,depth,&depth_meta));
 struct remesa_shader_program vs=vertex(2),fs=fragment(1);struct remesa_shader_linkage link;CHECK(!remesa_shader_linkage_prepare(&vs,&fs,&link));
 const float raw[3][12]={{width*.25f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.75f,height*.25f,.125f,1,0,0,1,0,0,0,1,0},{width*.25f,height*.75f,.125f,1,0,0,1,0,0,0,1,0}};
 const uint16_t bad_indices[3]={0,1,3};
 struct remesa_linked_scene_request request={.color=color,.depth=depth,.width=width,.height=height,.color_pitch=pitch,.linkage=&link,.geometry={.vertices=raw,.vertex_bytes=sizeof raw,.vertex_count=3},.state={.isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},.viewport={.framebuffer_width=width,.framebuffer_height=height,.width=width,.height=height,.depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_INITIAL_ZERO}},.vertex_bank=4,.pixel_bank=3,.state_bank=4,.clear_depth=width==640?.25f:1};
 if(mode==3)request.height--;
 if(mode==4){request.geometry.indices=bad_indices;request.geometry.index_bytes=sizeof bad_indices;request.geometry.index_count=3;}
 if(mode==5){vs=vertex(3);CHECK(remesa_shader_linkage_prepare(&vs,&fs,&link)!=0); /* incompatible pair is refused by linkage itself */ link.vertex.schema=99;}
 if(mode==6)request.clear_depth=NAN;
 if(mode==7)request.pixel_bank=request.vertex_bank;
 if(mode==8)request.state.viewport.framebuffer_width++;
 if(mode==10||mode==11){fail_at=8;fail_uncertain=mode==11;}
 struct remesa_scene_capture capture={0},zero=capture;
 if(mode==12){
  struct remesa_scene_capture busy={.serial=1},saved=busy;CHECK(remesa_persistent_linked_scene_prepare(&d,&request,&busy)==EBUSY&&!memcmp(&busy,&saved,sizeof busy));
  CHECK(remesa_persistent_linked_scene_prepare(&d,&request,(void *)&request)==EINVAL);
  CHECK(remesa_persistent_linked_scene_prepare(&d,&request,(void *)raw)==EINVAL);
 }
 int e=remesa_persistent_linked_scene_prepare(&d,&request,&capture);
 if(mode>=3&&mode<=11){
  CHECK(e&&!memcmp(&capture,&zero,sizeof zero));
  if(mode==11){CHECK(e==EOWNERDEAD&&remesa_persistent_close(&d)==EBUSY);puts("PASS uncertain create retains modeled endpoint; no dispatch/GPU0");return 0;}
  CHECK(f.creates-f.destroys==2);CHECK(!remesa_persistent_memory_snapshot(&d,color,&color_meta));CHECK(!remesa_persistent_depth_snapshot(&d,depth,&depth_meta));
  d.ops->destroy_bo(d.priv,color);d.ops->destroy_bo(d.priv,depth);CHECK(!remesa_persistent_close(&d));puts("PASS invalid/partial assembly unwinds generated graph, preserves caller attachments; GPU0");return 0;
 }
 CHECK(!e&&capture.owner==9100&&capture.serial==1);
 struct remesa_persistent_scene_footprint footprint;struct remesa_scene_packet packet;
 CHECK(!remesa_persistent_scene_capture_snapshot(&d,capture,&footprint));CHECK(!remesa_persistent_scene_packet_snapshot(&d,capture,&packet));
 CHECK(footprint.allocation_count==17&&footprint.incomplete==31&&packet.controls.schema==2&&packet.controls.depth_finalize==1&&packet.controls.ta_bytes==96);
 /* The builder must not initialize COLOR/Z or either32MiB DPM pool in CPU. */
 for(unsigned i=0;i<17;i++)if(footprint.allocations[i].access==3){struct backing *b=get(&f,footprint.allocations[i].handle);for(size_t n=0;n<b->length;n++)CHECK(b->bytes[n]==0xa5);}
 scene_capture_export(&f,&footprint,&packet);
 if(mode==12){d.ops->destroy_bo(d.priv,color);d.ops->destroy_bo(d.priv,depth);void *fence=NULL;CHECK(!remesa_persistent_scene_capture_submit(&d,capture,&fence)&&fence);CHECK(!d.ops->wait(d.priv,fence,0,true));d.ops->destroy_fence(d.priv,fence);}
 else {CHECK(!remesa_persistent_scene_capture_release(&d,capture));CHECK(f.creates-f.destroys==2);d.ops->destroy_bo(d.priv,color);d.ops->destroy_bo(d.priv,depth);}
 CHECK(f.creates==f.destroys&&f.pins==f.unpins);CHECK(!remesa_persistent_close(&d));puts("PASS existing driver encoders compose frontend-owned attachments/raw vertices into complete frozen scene, no attachment/DPM CPU clear and complete cleanup; GPU0");
}

#endif
