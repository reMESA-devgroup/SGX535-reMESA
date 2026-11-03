/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual normal backing owner/controller, modeled memory/fill/source hooks.
 * No GPU rendering, source approval or physical retirement is modeled here. */
#define REMESA_MATRIX_CONTROLLER_FIXTURE
#include "test_matrix_submit_handler.c"
#include "remesa/memory_uapi.h"
#include "../remesa_matrix_construct.inc"
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned mode=(unsigned)strtoul(argv[1],NULL,10);CHECK(mode<23);
 struct drm_psb_private priv={(void*)1};struct drm_device dev={&priv};struct drm_file file={1};
 struct remesa_gem_memory memory={.dev=&dev,.file=&file,.context=77,.root={0,0xdfffffffULL},.pd=(void*)1};
 struct remesa_scene_probe_binding bindings[18]={0};endpoint=&memory;
 for(unsigned i=0;i<18;i++){
  bindings[i].address=0x40000000u+i*0x1000000u;bindings[i].bytes=i==12||i==13?4096000:4096;
  bindings[i].access=i<12||i==17?1:3;
  memory.bo[i]=(struct backing){.live=true,.pinned=true,.reserved=true,.initialized=true,.zero_pristine=true,
   .generation=1,.va={bindings[i].address,bindings[i].address+bindings[i].bytes-1},.gem={bindings[i].bytes}};
  images[i]=(void *)(uintptr_t)(i+1);
 }
 expected_bindings=bindings;expected_count=18;matrix_submit_enabled=true;
 struct remesa_twod_fill_request fill={.version=1,.context=77,.budget_ms=1000,
  .destination={44,1280,800,5120},.width=1280,.height=800};
 const void *owner=NULL;
 if(mode==0){
  unsigned before=allocs;matrix_submit_enabled=false;
  CHECK(!remesa_matrix_fill_begin(&dev,&file,&fill,&owner)&&!owner&&allocs==before);matrix_submit_enabled=true;
  struct remesa_twod_fill_request bad=fill;bad.color=1;
  CHECK(remesa_matrix_fill_begin(&dev,&file,&bad,&owner)==-EOPNOTSUPP&&!owner&&allocs==before);
  bad=fill;bad.width=2048;bad.destination.width=2048;bad.destination.stride=8192;
  CHECK(remesa_matrix_fill_begin(&dev,&file,&bad,&owner)==-EOPNOTSUPP&&!owner&&allocs==before);
  bad=fill;bad.destination.id^=32;
  CHECK(remesa_matrix_fill_begin(&dev,&file,&bad,&owner)==-ENOENT&&!owner&&allocs==before);
  CHECK(!memory.private_session&&!scene_probe_retained&&!claims);
 }
 CHECK(!remesa_matrix_fill_begin(&dev,&file,&fill,&owner)&&owner);
 CHECK(memory.private_session==owner&&matrix_fill_owner&&matrix_fill_owner->active);
 CHECK(!memory.private_matrix_constructing&&memory.private_matrix_color==44&&!claims&&!matrix_executes);
 const void *other=NULL;CHECK(remesa_matrix_fill_begin(&dev,&file,&fill,&other)==-EBUSY&&!other);
 struct remesa_memory_request request={.action=REMESA_MEM_CREATE};
 CHECK(!matrix_cpu_construct(&memory,&request));
 if(mode==1){
  mutex_lock(&endpoint_lock);remesa_matrix_fill_failed_locked(&memory,owner);mutex_unlock(&endpoint_lock);
  CHECK(!remesa_matrix_fill_cancel_unissued(owner)&&!matrix_fill_owner&&!scene_probe_retained&&!memory.private_session);
  CHECK(allocs==frees);return 0;
 }
 struct remesa_twod_retirement proof={.dev=&dev,.pd=memory.pd,.ticket={.context=77,.operation=1,.generation=1},
  .cookie=17,.destination_id=44,.packet_dwords=7,.core=0x01130000,.revision=0x10201,
  .native_retired=true,.gem_retired=true};
 fill.operation=1;fill.cookie=17;fill.stages=31;
 switch(mode){
 case 0:case 2:case 3:case 4:break;
 case 5:proof.native_retired=false;break;case 6:proof.gem_retired=false;break;
 case 7:proof.dev=NULL;break;case 8:proof.pd=NULL;break;case 9:proof.ticket.context++;break;
 case 10:proof.ticket.operation++;break;case 11:proof.cookie++;break;case 12:proof.destination_id++;break;
 case 13:proof.packet_dwords=9;break;case 14:proof.core++;break;case 15:proof.revision++;break;
 case 16:fill.error=-EIO;break;case 17:fill.retained=1;break;case 18:fill.stages=15;break;
 case 19:memory.private_device_lost=true;break;case 20:memory.file=NULL;break;
 case 21:fill.width--;break;case 22:fill.destination.stride--;break;
 }
 mutex_lock(&endpoint_lock);int error=remesa_matrix_fill_finish(&memory,owner,&fill,&proof);mutex_unlock(&endpoint_lock);
 if(mode>=5){CHECK(error==-EPROTO&&matrix_fill_owner->failed&&!memory.private_matrix_constructing&&memory.private_session==owner);return 0;}
 CHECK(!error&&!matrix_fill_owner->active&&memory.private_matrix_constructing);
 CHECK(matrix_cpu_construct(&memory,&request));
 for(unsigned action=1;action<=14;action++){
  request=(struct remesa_memory_request){.action=action,.id=44,.flags=1};
  bool readable=action==REMESA_MEM_READ||action==REMESA_MEM_UNLEASE||action==REMESA_MEM_SYNC;
  CHECK(matrix_color_protected_write(&memory,&request)==!readable);
 }
 request=(struct remesa_memory_request){.action=REMESA_MEM_LEASE,.id=44};
 CHECK(!matrix_color_protected_write(&memory,&request)&&matrix_cpu_construct(&memory,&request));
 if(mode==2){
  fill.operation=fill.cookie=fill.stages=0;
  CHECK(!remesa_matrix_fill_begin(&dev,&file,&fill,&other)&&other==owner&&!memory.private_matrix_constructing);
  mutex_lock(&endpoint_lock);remesa_matrix_fill_failed_locked(&memory,owner);mutex_unlock(&endpoint_lock);
  CHECK(!remesa_matrix_fill_cancel_unissued(owner)&&memory.private_matrix_constructing&&memory.private_session==owner);
 }
 if(mode==3){
  memory.jobs[0].live=true;request=(struct remesa_memory_request){.action=REMESA_MEM_CREATE};
  CHECK(!matrix_cpu_construct(&memory,&request));memory.jobs[0].live=false;
 }
 if(mode==4){
  /* Freeze for a real typed draw, then reproduce a source refusal without
   * pretending this backing record repaired the first-owner lineage. */
  struct remesa_matrix_submit_request draw={.version=1,.budget_ms=1000,.context=77};
  struct remesa_scene_packet *p=&draw.packet;*p=(struct remesa_scene_packet){.owner=77,.serial=1,.version=1,.binding_count=18,.use_count=11,.window_count=2};
  for(unsigned i=0;i<18;i++){p->bindings[i].id=32+i;p->bindings[i].access=bindings[i].access;}
  p->actions=(struct remesa_scene_packet_actions){1,32,1280,800,46,47,48,0};
  p->controls=(struct remesa_scene_packet_controls){2,80,1280,800,1280,1280,4,0x3f800000,44,45,32,0,104,42,0,68,43,0,148,1};
  for(unsigned i=0;i<11;i++)p->uses[i]=(struct remesa_scene_packet_use){32+i,0,8,4,0,0};
  p->windows[0]=(struct remesa_scene_packet_window){33,0,6,34,0,48,3,0};
  p->windows[1]=(struct remesa_scene_packet_window){35,0,6,36,0,144,3,0};
  fault=CLAIM;CHECK(!remesa_matrix_submit_ioctl(&dev,&draw,&file));
  CHECK(draw.error==-EIO&&draw.stages==8&&!draw.retained&&!draw.operation&&!matrix_executes&&claims==1);
  CHECK(!memory.private_session&&!memory.private_matrix_constructing&&!memory.private_matrix_color&&!matrix_fill_owner&&allocs==frees);
  return 0;
 }
 printf("PASS normal fill backing case%u: construction/protectedCOLOR, source not approved, GPU0\n",mode);
 return 0;
}
