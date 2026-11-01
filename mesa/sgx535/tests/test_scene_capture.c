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
#include "scene_capture_export.inc"
static void controls_error(struct remesa_device *d,struct remesa_scene_capture token,int error)
{
 struct remesa_persistent_scene_controls out,saved;memset(&out,0xa5,sizeof out);saved=out;
 CHECK(remesa_persistent_scene_controls_snapshot(d,token,&out)==error&&!memcmp(&out,&saved,sizeof out));
 struct remesa_persistent_scene_use use,use_saved;memset(&use,0xa5,sizeof use);use_saved=use;
 CHECK(remesa_persistent_scene_use_snapshot(d,token,&use)==error&&!memcmp(&use,&use_saved,sizeof use));
}
static void unchanged(struct remesa_device *d,struct remesa_scene_capture c,int error)
{
 struct remesa_persistent_scene_footprint r,saved;memset(&r,0xa5,sizeof r);saved=r;
 CHECK(remesa_persistent_scene_capture_snapshot(d,c,&r)==error&&!memcmp(&r,&saved,sizeof r));
 struct remesa_persistent_scene_ta ta,ta_saved;memset(&ta,0xa5,sizeof ta);ta_saved=ta;
 CHECK(remesa_persistent_scene_ta_snapshot(d,c,&ta)==error&&!memcmp(&ta,&ta_saved,sizeof ta));
 struct remesa_persistent_scene_vertices vertices,vertices_saved;memset(&vertices,0xa5,sizeof vertices);vertices_saved=vertices;
 CHECK(remesa_persistent_scene_vertices_snapshot(d,c,&vertices)==error&&!memcmp(&vertices,&vertices_saved,sizeof vertices));
 struct remesa_scene_packet packet,snapshot;memset(&packet,0xa5,sizeof packet);snapshot=packet;
 CHECK(remesa_persistent_scene_packet_snapshot(d,c,&packet)==error&&!memcmp(&packet,&snapshot,sizeof packet));
 controls_error(d,c,error);
 CHECK(remesa_persistent_scene_capture_release(d,c)==error);
}
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479}};
 for(unsigned v=0;v<9;v++){
  bool event_profile=v>=3,raw_region=v>=6;unsigned size=v%3;
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
  void *c[11]={0},*p[4]={0},*table=NULL,*parameter=NULL,*event=NULL,*selected=NULL;
  bg_setup(&f,&d,4100+v,sizes[size][0],sizes[size][1],c);
  CHECK(!remesa_persistent_linked_background_prepare(&d,c[10],3,&p[0]));
  CHECK(!remesa_persistent_linked_event_auxiliary_prepare(&d,c[10],3,&p[1]));
  if(raw_region){
   unsigned maps=f.cpu_maps,reads=f.reads,writes=f.writes;
   CHECK(!remesa_persistent_linked_region_allocate(&d,c[10],&p[2]));
   CHECK(f.cpu_maps==maps&&f.reads==reads&&f.writes==writes);
   struct remesa_persistent_linked_region_backing rb;void *cpu=NULL;
   CHECK(!remesa_persistent_linked_region_snapshot(&d,p[2],&rb));
   CHECK(get(&f,rb.region.handle)->bytes[0]==0xa5&&d.ops->map(d.priv,p[2],false,&cpu)==EAGAIN&&!cpu);
  }else CHECK(!remesa_persistent_linked_region_prepare(&d,c[10],&p[2]));
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
  struct remesa_persistent_scene_use use_reference={0},use={0};
  struct remesa_persistent_scene_ta ta_reference={0},ta={0};
  struct remesa_persistent_scene_vertices vertices_reference={0},vertices={0};
  struct remesa_scene_capture tokens[REMESA_PROVIDER_MAX_JOBS],bad,saved;
  memset(&bad,0xa5,sizeof bad);saved=bad;
  /* Raw REGION remains unreadable until GPU initialization. Exercise the
   * active-lease refusal through the already initialized background instead. */
  void *lease=NULL,*lease_object=raw_region?p[0]:p[2];
  CHECK(!d.ops->map(d.priv,lease_object,false,&lease));
  CHECK(remesa_persistent_linked_targets_color_initialize(&d,c[10],0)==EBUSY);
  CHECK(remesa_persistent_scene_capture(&d,p[0],selected,p[2],p[3],&bad)==EBUSY&&!memcmp(&bad,&saved,sizeof bad));
  d.ops->unmap(d.priv,lease_object,false);
  CHECK(remesa_persistent_scene_capture(&d,p[0],p[1],p[2],p[3],(void *)((unsigned char *)&bad+1))==EINVAL);
  CHECK(remesa_persistent_scene_capture(&d,p[0],p[1],p[2],p[3],p[0])==EINVAL);
  CHECK(remesa_persistent_scene_capture(&d,p[0],p[0],p[2],p[3],&bad)==EINVAL&&!memcmp(&bad,&saved,sizeof bad));
  unsigned maps=f.cpu_maps,creates=f.creates,gpu_maps=f.gpu_maps,unpins=f.unpins;
  for(unsigned i=0;i<REMESA_PROVIDER_MAX_JOBS;i++){
   CHECK(!remesa_persistent_scene_capture(&d,p[0],selected,p[2],p[3],&tokens[i]));
   CHECK(tokens[i].owner==4100+v&&tokens[i].serial==i+1&&!tokens[i].reserved&&tokens[i].slot==i);
   struct remesa_scene_packet packet;memset(&packet,0xa5,sizeof packet);
   if(event_profile){
    CHECK(!remesa_persistent_scene_packet_snapshot(&d,tokens[i],&packet));
    CHECK(remesa_scene_packet_valid(&packet)&&packet.owner==tokens[i].owner&&packet.serial==tokens[i].serial);
    CHECK(packet.binding_count==17&&packet.use_count==10&&packet.window_count==2);
    if(!i&&raw_region)scene_capture_export(&f,&expected,&packet);
    CHECK(packet.actions.width==sizes[size][0]&&packet.controls.height==sizes[size][1]);
    CHECK(packet.controls.event_bytes==148&&packet.controls.background_bytes==68);
    bool finalized=getenv("REMESA_CAPTURE_EXPORT_DIR")&&getenv("REMESA_CAPTURE_FINALIZED_DEPTH");
    CHECK(packet.controls.schema==(finalized?2u:1u)&&packet.controls.depth_finalize==(finalized?1u:0u));
    struct remesa_scene_packet invalid_controls=packet;
    invalid_controls.controls.schema=1;invalid_controls.controls.depth_finalize=1;
    CHECK(!remesa_scene_packet_valid(&invalid_controls));
    invalid_controls.controls.schema=2;invalid_controls.controls.depth_finalize=0;
    CHECK(!remesa_scene_packet_valid(&invalid_controls));
    invalid_controls.controls.depth_finalize=2;CHECK(!remesa_scene_packet_valid(&invalid_controls));
    invalid_controls.controls.schema=3;invalid_controls.controls.depth_finalize=1;
    CHECK(!remesa_scene_packet_valid(&invalid_controls));
    unsigned char commands[REMESA_SCENE_PACKET_COMMAND_MAX];memset(commands,0xa5,sizeof commands);
    unsigned bytes=123;struct remesa_scene_packet_sections offsets;
    CHECK(remesa_scene_packet_commands(&packet,commands,sizeof commands,&bytes,&offsets));
    CHECK(bytes==448&&offsets.actions==0&&offsets.controls==32&&offsets.use==112&&offsets.vertices==368);
    CHECK(!memcmp(commands,&packet.actions,32)&&!memcmp(commands+32,&packet.controls,80));
    struct remesa_scene_packet corrupt=packet;corrupt.bindings[1].id=corrupt.bindings[0].id;
    unsigned before_bytes=bytes;unsigned char before[sizeof commands];memcpy(before,commands,sizeof before);
    CHECK(!remesa_scene_packet_commands(&corrupt,commands,sizeof commands,&bytes,&offsets)&&bytes==before_bytes&&!memcmp(before,commands,sizeof before));
    corrupt=packet;corrupt.uses[10].id=1;CHECK(!remesa_scene_packet_valid(&corrupt));
    corrupt=packet;corrupt.windows[0].vertex_id=0;CHECK(!remesa_scene_packet_valid(&corrupt));
    CHECK(!remesa_scene_packet_commands(&packet,commands,447,&bytes,&offsets)&&!memcmp(before,commands,sizeof before));
   }else{struct remesa_scene_packet before=packet;CHECK(remesa_persistent_scene_packet_snapshot(&d,tokens[i],&packet)==ENOTSUP&&!memcmp(&packet,&before,sizeof before));}

   CHECK(!remesa_persistent_scene_capture_snapshot(&d,tokens[i],&r)&&!memcmp(&r,&expected,sizeof r));
   CHECK(!remesa_persistent_scene_ta_snapshot(&d,tokens[i],&ta));
   CHECK(!remesa_persistent_scene_vertices_snapshot(&d,tokens[i],&vertices));
   CHECK(vertices.owner==tokens[i].owner&&vertices.serial==tokens[i].serial&&vertices.draw_count==1&&vertices.window_count==2&&vertices.incomplete==31);
   if(!i){
    vertices_reference=vertices;
    CHECK(vertices.windows[0].vertex_count==3&&vertices.windows[0].vertex_bytes==48&&vertices.windows[0].vertex_offset==64&&
     vertices.windows[0].index_bytes==6&&vertices.windows[0].index_offset==128);
    CHECK(vertices.windows[1].vertex_count==3&&vertices.windows[1].vertex_bytes==144&&vertices.windows[1].vertex_offset==0&&
     vertices.windows[1].index_bytes==6&&vertices.windows[1].index_offset==144);
    for(unsigned q=0;q<2;q++){
     CHECK(vertices.windows[q].index_handle==ta.references[q*3+1+(ta.references[0].kind==REMESA_TA_REF_INITIAL_STATE)].handle&&vertices.windows[q].index_generation==ta.references[q*3+1+(ta.references[0].kind==REMESA_TA_REF_INITIAL_STATE)].generation);
     unsigned a=0;for(;a<expected.allocation_count&&expected.allocations[a].handle!=vertices.windows[q].vertex_handle;a++);
     CHECK(a<expected.allocation_count&&expected.allocations[a].generation==vertices.windows[q].vertex_generation&&
      expected.allocations[a].access==REMESA_ACCESS_READ&&vertices.windows[q].vertex_offset+vertices.windows[q].vertex_bytes<=expected.allocations[a].logical_bytes);
    }
    CHECK(remesa_persistent_scene_vertices_snapshot(&d,tokens[i],(void *)((uint8_t *)&vertices+1))==EINVAL);
    CHECK(remesa_persistent_scene_vertices_snapshot(&d,tokens[i],p[0])==EINVAL);
   }else{vertices.serial=vertices_reference.serial;CHECK(!memcmp(&vertices,&vertices_reference,sizeof vertices));}
   CHECK(ta.owner==tokens[i].owner&&ta.serial==tokens[i].serial&&ta.draw_count==1&&ta.reference_count==7+(getenv("REMESA_CAPTURE_EXPORT_DIR")&&getenv("REMESA_CAPTURE_INITIALIZED_PREFIX"))&&ta.incomplete==31);
   if(!i){
    ta_reference=ta;
    for(unsigned q=0;q<ta.reference_count;q++){
     unsigned a=0;for(;a<expected.allocation_count&&expected.allocations[a].handle!=ta.references[q].handle;a++);
     CHECK(a<expected.allocation_count&&expected.allocations[a].generation==ta.references[q].generation&&
      expected.allocations[a].access==REMESA_ACCESS_READ&&ta.references[q].minimum_bytes<=ta.references[q].object_bytes&&
      ta.references[q].byte_offset+ta.references[q].object_bytes<=expected.allocations[a].logical_bytes&&ta.references[q].kind==((getenv("REMESA_CAPTURE_EXPORT_DIR")&&getenv("REMESA_CAPTURE_INITIALIZED_PREFIX"))?(q?q:REMESA_TA_REF_INITIAL_STATE):q+1));
    }
    unsigned initial=getenv("REMESA_CAPTURE_EXPORT_DIR")&&getenv("REMESA_CAPTURE_INITIALIZED_PREFIX");
    if(initial)CHECK(ta.references[0].kind==REMESA_TA_REF_INITIAL_STATE&&ta.references[0].object_bytes==60);
    static const unsigned lengths[7]={60,6,64,60,6,64,60};
    for(unsigned q=0;q<7;q++)CHECK(ta.references[q+initial].object_bytes==lengths[q]);
    CHECK(remesa_persistent_scene_ta_snapshot(&d,tokens[i],(void *)((uint8_t *)&ta+1))==EINVAL);
    CHECK(remesa_persistent_scene_ta_snapshot(&d,tokens[i],p[0])==EINVAL);
   }else{ta.serial=ta_reference.serial;CHECK(!memcmp(&ta,&ta_reference,sizeof ta));}
   if(event_profile){
    CHECK(!remesa_persistent_scene_use_snapshot(&d,tokens[i],&use));
    CHECK(use.owner==tokens[i].owner&&use.serial==tokens[i].serial&&use.incomplete==31&&use.entry_count==10&&use.bank_mask==24);
    if(!i){
     use_reference=use;
     for(unsigned e=0;e<use.entry_count;e++){
      unsigned a=0;for(;a<expected.allocation_count&&expected.allocations[a].handle!=use.entries[e].handle;a++);
      CHECK(a<expected.allocation_count&&expected.allocations[a].access==REMESA_ACCESS_READ&&
       expected.allocations[a].generation==use.entries[e].generation&&use.entries[e].program_bytes&&
       use.entries[e].byte_offset+use.entries[e].program_bytes<=expected.allocations[a].logical_bytes);
      struct remesa_use_entry_request request={expected.allocations[a].gpu_va,expected.allocations[a].allocation_bytes,
       use.entries[e].byte_offset,use.entries[e].program_bytes,use.entries[e].bank,use.entries[e].data_master};
      struct remesa_use_entry_plan plan;uint32_t fixup;
      CHECK(!remesa_use_entry_prepare(&request,&plan)&&!remesa_use_entry_preview_fixup(&plan,&fixup));
      CHECK(fixup==use.entries[e].lower19&&use.banks[plan.bank].offset==plan.register_offset&&use.banks[plan.bank].word==plan.register_word);
     }
     CHECK(remesa_persistent_scene_use_snapshot(&d,tokens[i],(void *)((uint8_t *)&use+1))==EINVAL);
     CHECK(remesa_persistent_scene_use_snapshot(&d,tokens[i],p[0])==EINVAL);
    }else{use.serial=use_reference.serial;CHECK(!memcmp(&use,&use_reference,sizeof use));}
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
     struct remesa_depth_surface_state finalized_depth;
     CHECK(!remesa_depth_surface_finalize(&bm.targets.depth.initial_surface,bm.targets.depth_finalize,true,&finalized_depth));
     CHECK(bm.targets.depth_finalize==(getenv("REMESA_CAPTURE_EXPORT_DIR")&&getenv("REMESA_CAPTURE_FINALIZED_DEPTH")));
     CHECK(shader_pds_le32(control.raster+10*8+4)==finalized_depth.control_480);
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
   CHECK(!remesa_persistent_scene_ta_snapshot(&d,tokens[0],&ta)&&ta.serial==tokens[0].serial);
   CHECK(!remesa_persistent_scene_vertices_snapshot(&d,tokens[0],&vertices)&&vertices.serial==tokens[0].serial);
   vertices.serial=vertices_reference.serial;CHECK(!memcmp(&vertices,&vertices_reference,sizeof vertices));
   ta.serial=ta_reference.serial;CHECK(!memcmp(&ta,&ta_reference,sizeof ta));
   if(event_profile){
    CHECK(!remesa_persistent_scene_use_snapshot(&d,tokens[0],&use)&&use.serial==tokens[0].serial);
    use.serial=use_reference.serial;CHECK(!memcmp(&use,&use_reference,sizeof use));
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
 puts("PASS unissued scene captures: three dimensions with auxiliary-only, fixed event PDS and raw undefined region, shared frozen closures, full frontend release, eight-slot limit, stale/foreign/alias rejection, bound raster208/TA56/region control snapshots with retained clear depth, zero provider I/O and complete cancellation cleanup; GPU0/API0");return 0;
}
