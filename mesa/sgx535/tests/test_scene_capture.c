/* SPDX-License-Identifier: MIT. Unissued userspace scene holds; no GPU/API. */
#define REMESA_LINKED_BACKGROUND_FIXTURE
#define REMESA_LINKED_TEST_CLEAR_DEPTH(context) (((context)&1u)?0.25f:1.0f)
#define REMESA_LINKED_TEST_BYTES (96u*1024u*1024u)
#define REMESA_LINKED_TEST_RASTER_END 0x34000000
#define REMESA_LINKED_TEST_RASTER_ALIGNMENT 1048576
#define REMESA_LINKED_TEST_ALLOCATION_MAX 0x2000000
#include "test_linked_background.c"
#include <unistd.h>
#include <sys/wait.h>
static void controls_error(struct remesa_device *d,struct remesa_scene_capture token,int error)
{
 struct remesa_persistent_scene_controls out,saved;memset(&out,0xa5,sizeof out);saved=out;
 CHECK(remesa_persistent_scene_controls_snapshot(d,token,&out)==error&&!memcmp(&out,&saved,sizeof out));
}
static void unchanged(struct remesa_device *d,struct remesa_scene_capture c,int error)
{
 struct remesa_persistent_scene_footprint r,saved;memset(&r,0xa5,sizeof r);saved=r;
 CHECK(remesa_persistent_scene_capture_snapshot(d,c,&r)==error&&!memcmp(&r,&saved,sizeof r));
 controls_error(d,c,error);
 CHECK(remesa_persistent_scene_capture_release(d,c)==error);
}
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479}};
 for(unsigned v=0;v<6;v++){
  bool event_profile=v>=3;unsigned size=v%3;
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *c[11]={0},*p[4]={0},*table=NULL,*parameter=NULL,*event=NULL,*selected=NULL;
  bg_setup(&f,&d,4100+v,sizes[size][0],sizes[size][1],c);
  CHECK(!remesa_persistent_linked_background_prepare(&d,c[10],3,&p[0]));
  CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&p[1]));
  CHECK(!remesa_persistent_linked_region_prepare(&d,c[10],&p[2]));
  CHECK(!remesa_persistent_allocate_placed(&d,0x2000000,REMESA_VA_SURFACE,&table));
  CHECK(!remesa_persistent_allocate_placed(&d,0x2000000,REMESA_VA_RASTER,&parameter));
  CHECK(!remesa_persistent_linked_dpm_prepare(&d,table,parameter,&p[3]));
  selected=p[1];
  if(event_profile){CHECK(!remesa_persistent_linked_event_profile_prepare(&d,p[1],&event));selected=event;}
  struct remesa_persistent_scene_footprint expected,r;
  CHECK(!remesa_persistent_scene_footprint(&d,p[0],selected,p[2],p[3],&expected));
  CHECK(expected.allocation_count==16+event_profile&&expected.incomplete==31);
  if(event_profile){
   struct remesa_persistent_linked_event_profile_backing eb;
   CHECK(!remesa_persistent_linked_event_profile_snapshot(&d,event,&eb));
   unsigned found=0;
   for(unsigned n=0;n<expected.allocation_count;n++)if(expected.allocations[n].roles&REMESA_SCENE_REF_EVENT_PDS){
    CHECK(expected.allocations[n].roles==REMESA_SCENE_REF_EVENT_PDS&&expected.allocations[n].handle==eb.handle&&
     expected.allocations[n].access==REMESA_ACCESS_READ&&expected.allocations[n].logical_bytes==400);found++;
   }
   CHECK(found==1);
  }
  struct remesa_persistent_scene_controls control_reference={0},control={0};
  struct remesa_scene_capture tokens[REMESA_PROVIDER_MAX_JOBS],bad,saved;
  memset(&bad,0xa5,sizeof bad);saved=bad;
  void *lease=NULL;CHECK(!d.ops->map(d.priv,p[2],false,&lease));
  CHECK(remesa_persistent_scene_capture(&d,p[0],selected,p[2],p[3],&bad)==EBUSY&&!memcmp(&bad,&saved,sizeof bad));
  d.ops->unmap(d.priv,p[2],false);
  CHECK(remesa_persistent_scene_capture(&d,p[0],p[1],p[2],p[3],(void *)((unsigned char *)&bad+1))==EINVAL);
  CHECK(remesa_persistent_scene_capture(&d,p[0],p[1],p[2],p[3],p[0])==EINVAL);
  CHECK(remesa_persistent_scene_capture(&d,p[0],p[0],p[2],p[3],&bad)==EINVAL&&!memcmp(&bad,&saved,sizeof bad));
  unsigned maps=f.cpu_maps,creates=f.creates,gpu_maps=f.gpu_maps,unpins=f.unpins;
  for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++){
   CHECK(!remesa_persistent_scene_capture(&d,p[0],selected,p[2],p[3],&tokens[i]));
   CHECK(tokens[i].owner==4100+v&&tokens[i].serial==i+1&&!tokens[i].reserved&&tokens[i].slot==i);
   CHECK(!remesa_persistent_scene_capture_snapshot(&d,tokens[i],&r)&&!memcmp(&r,&expected,sizeof r));
   if(event_profile){
    CHECK(!remesa_persistent_scene_controls_snapshot(&d,tokens[i],&control));
    CHECK(control.owner==tokens[i].owner&&control.serial==tokens[i].serial&&control.width==sizes[size][0]&&
     control.height==sizes[size][1]&&control.incomplete==31&&control.raster_bytes==208&&control.ta_bytes==56);
    if(!i){
     struct remesa_persistent_linked_background_backing bm;
     struct remesa_persistent_linked_event_profile_backing em;
     struct remesa_persistent_linked_region_backing rm;
     struct remesa_persistent_zclear_prefix_backing zm;
     CHECK(!remesa_persistent_linked_background_snapshot(&d,p[0],&bm));
     CHECK(!remesa_persistent_linked_event_profile_snapshot(&d,event,&em));
     CHECK(!remesa_persistent_linked_region_snapshot(&d,p[2],&rm));
     CHECK(!remesa_persistent_zclear_prefix_snapshot(&d,c[6],&zm));
     CHECK(zm.clear_depth==REMESA_LINKED_TEST_CLEAR_DEPTH(4100+v));
     uint32_t clear;memcpy(&clear,&zm.clear_depth,4);
     CHECK(shader_pds_le32(control.raster+19*8+4)==clear);
     CHECK(shader_pds_le32(get(&f,zm.handle)->bytes+zm.vertex_offset+8)==clear);
     CHECK(shader_pds_le32(control.raster+3*8+4)==bm.targets.color.state.raster_410);
     CHECK(shader_pds_le32(control.raster+10*8+4)==bm.targets.depth.initial_surface.control_480);
     for(unsigned a=0;a<5;a++){
      CHECK(shader_pds_le32(control.raster+(11+a)*8)==bm.targets.depth.initial_surface.addresses[a].reg);
      CHECK(shader_pds_le32(control.raster+(11+a)*8+4)==bm.targets.depth.initial_surface.addresses[a].word);
     }
     CHECK(shader_pds_le32(control.raster+17*8+4)==(((bm.gpu_va>>4)&0xffffff)|0x02000000));
     CHECK(shader_pds_le32(control.raster+23*8+4)==em.gpu_va&&shader_pds_le32(control.raster+24*8+4)==4);
     CHECK(shader_pds_le32(control.ta+52)==bm.targets.commands.gpu_va);
     CHECK(!memcmp(control.ta_scene,rm.ta_scene,sizeof control.ta_scene)&&!memcmp(control.raster_scene,rm.raster_scene,sizeof control.raster_scene));
     control_reference=control;
     CHECK(remesa_persistent_scene_controls_snapshot(&d,tokens[i],(void *)((uint8_t *)&control+1))==EINVAL&&!memcmp(&control,&control_reference,sizeof control));
     CHECK(remesa_persistent_scene_controls_snapshot(&d,tokens[i],event)==EINVAL);
    }else{control.serial=control_reference.serial;CHECK(!memcmp(&control,&control_reference,sizeof control));}
   }else controls_error(&d,tokens[i],ENOTSUP);
  }
  CHECK(remesa_persistent_scene_capture(&d,p[0],selected,p[2],p[3],&bad)==EBUSY&&!memcmp(&bad,&saved,sizeof bad));
  bad=tokens[0];bad.owner++;unchanged(&d,bad,EINVAL);
  bad=tokens[0];bad.reserved=1;unchanged(&d,bad,EINVAL);
  bad=tokens[0];bad.slot=REMESA_PROVIDER_MAX_JOBS;unchanged(&d,bad,EINVAL);
  bad=tokens[0];bad.serial++;unchanged(&d,bad,ESTALE);
  CHECK(remesa_persistent_scene_capture_snapshot(&d,tokens[0],p[0])==EINVAL);
  void *frozen[]={c[0],c[1],c[2],c[3],c[7],c[8],c[9],c[10],p[0],p[1],p[2],p[3],table,parameter};
  for(unsigned i=0;i<sizeof frozen/sizeof frozen[0];i++){
   lease=(void *)1;CHECK(d.ops->map(d.priv,frozen[i],false,&lease)==EBUSY&&!lease);
  }
  if(event_profile){lease=(void *)1;CHECK(d.ops->map(d.priv,event,false,&lease)==EBUSY&&!lease);}
  lease=(void *)1;CHECK(d.ops->map(d.priv,table,true,&lease)==EBUSY&&!lease);
  CHECK(f.cpu_maps==maps&&f.creates==creates&&f.gpu_maps==gpu_maps&&f.unpins==unpins);
  struct remesa_scene_capture stale=tokens[0];
  CHECK(!remesa_persistent_scene_capture_release(&d,tokens[0]));unchanged(&d,stale,ESTALE);
  CHECK(!remesa_persistent_scene_capture(&d,p[0],selected,p[2],p[3],&tokens[0]));
  CHECK(tokens[0].slot==stale.slot&&tokens[0].serial!=stale.serial);unchanged(&d,stale,ESTALE);
  struct remesa_device foreign={0};struct fixture ff={.allow_gpu_map=true,.allow_sync=true};
  struct remesa_persistent_config cfg={.ops=&target_ops,.priv=&ff,.context=4200+v,
   .va_start=0x20000000,.va_end=0x45000000,.byte_budget=1u<<20,.resource_only_backend=true};
  CHECK(!remesa_persistent_open(&foreign,&cfg));unchanged(&foreign,tokens[0],EINVAL);
  CHECK(!remesa_persistent_close(&foreign));
  d.ops->destroy_bo(d.priv,table);d.ops->destroy_bo(d.priv,parameter);bg_drop_children(&d,c);
  for(unsigned i=0;i<4;i++)d.ops->destroy_bo(d.priv,p[i]);
  if(event_profile)d.ops->destroy_bo(d.priv,event);
  CHECK(remesa_persistent_close(&d)==EBUSY&&f.destroys==0);
  for(unsigned repeat=0;repeat<100;repeat++){
   CHECK(!remesa_persistent_scene_capture_snapshot(&d,tokens[0],&r));CHECK(!memcmp(&r,&expected,sizeof r));
   if(event_profile){
    CHECK(!remesa_persistent_scene_controls_snapshot(&d,tokens[0],&control)&&control.serial==tokens[0].serial);
    control.serial=control_reference.serial;CHECK(!memcmp(&control,&control_reference,sizeof control));
   }else controls_error(&d,tokens[0],ENOTSUP);
  }
  for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++){
   if((v==0||v==3)&&i+1==REMESA_PROVIDER_MAX_JOBS){
    for(unsigned mode=0;mode<2;mode++){
     pid_t child=fork();CHECK(child>=0);
     if(!child){
      f.fail=mode?FAIL_DESTROY:FAIL_UNPIN;
      CHECK(remesa_persistent_scene_capture_release(&d,tokens[i])==EOWNERDEAD);
      CHECK(f.destroys<f.creates&&remesa_persistent_close(&d)==EBUSY);
      if(!mode)CHECK(f.unpins<f.pins);
      CHECK(remesa_persistent_scene_capture_release(&d,tokens[i])==ESTALE);
      _exit(0); /* No forced disposal of the quarantined injected backing. */
     }
     int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
    }
   }
   CHECK(!remesa_persistent_scene_capture_release(&d,tokens[i]));
   if(i+1<REMESA_PROVIDER_MAX_JOBS)CHECK(f.destroys==0);
  }
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
  unchanged(&d,tokens[0],ESTALE);CHECK(!remesa_persistent_close(&d));
 }
 puts("PASS unissued scene captures: three storage dimensions with auxiliary-only and fixed event PDS, shared frozen closures, full frontend release, eight-slot limit, stale/foreign/alias rejection, bound raster208/TA56/region control snapshots with retained clear depth, zero provider I/O and complete cancellation cleanup; GPU0/API0");return 0;
}
