/* SPDX-License-Identifier: MIT. Actual Linux adapter, interposed ioctl; GPU0. */
#define main general_transport_regression_main
#include "test_general_session_transport.c"
#undef main

static struct remesa_scene_packet packet(void)
{
 struct remesa_scene_packet p={.owner=77,.serial=1,.version=1,
  .binding_count=18,.use_count=11,.window_count=2,
  .actions={.schema=1,.bytes=32,.width=1280,.height=800,
   .region_id=32,.table_id=33,.parameter_id=34},
  .controls={.schema=2,.bytes=80,.width=1280,.height=800,.color_pitch=5120,
   .depth_pitch=5120,.depth_cpp=4,.color_id=35,.depth_id=36,.ta_id=37,
   .ta_bytes=104,.background_id=38,.event_id=39,.depth_finalize=1}};
 for(unsigned i=0;i<18;i++){p.bindings[i].id=ids[i];p.bindings[i].access=i<5?3:1;}
 for(unsigned i=0;i<11;i++)p.uses[i]=(struct remesa_scene_packet_use){.id=ids[i],.program_bytes=8,.bank=3};
 for(unsigned i=0;i<2;i++)p.windows[i]=(struct remesa_scene_packet_window){.index_id=ids[14],
  .index_bytes=6,.vertex_id=ids[15],.vertex_bytes=48,.vertex_count=3};
 assert(remesa_scene_packet_valid(&p));return p;
}
int main(void)
{
 ops=remesa_linux_memory_ops();for(unsigned i=0;i<18;i++)ids[i]=32+i;
 struct remesa_linux_memory *d=setup();struct remesa_scene_packet p=packet(),saved=p;
 struct remesa_general_session_request response;
 struct remesa_gpu_scene_submit s={.context=77,.operation=1,.packet=&p};
 struct remesa_gpu_scene_receipt sentinel={.context=456},r=sentinel;
 assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_NOT_ISSUED&&!calls);
 assert(!memcmp(&r,&sentinel,sizeof r));
 assert(remesa_linux_memory_general_open(d,ids,5,3,(void *)d)==EINVAL&&!calls);
 assert(remesa_linux_memory_general_open(d,(void *)d,5,3,&response)==EINVAL&&!calls);
 assert(remesa_linux_memory_general_open(d,(void *)&response,5,3,&response)==EINVAL&&!calls);
 assert(remesa_linux_memory_general_scene_open(d,&p,6,3,1000,&response)==EINVAL&&!calls);
 assert(remesa_linux_memory_general_scene_open(d,&p,5,3,0,&response)==EINVAL&&!calls);
 assert(remesa_linux_memory_general_scene_open(d,&p,5,3,1000,(void *)&p)==EINVAL&&!calls);
 assert(!remesa_linux_memory_general_scene_open(d,&p,5,3,1000,&response)&&calls==1);
 unsigned m=memcalls;
 assert(remesa_linux_memory_general_submit(d,1000,(void *)d)==EINVAL&&calls==1);
 assert(remesa_linux_memory_general_close(d,(void *)d)==EINVAL&&calls==1);
 p.serial++;assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_NOT_ISSUED&&calls==1);p=saved;
 s.context++;assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_NOT_ISSUED&&calls==1);s.context--;
 assert(remesa_linux_memory_general_scene_dispatch(d,&s,(void *)d)==REMESA_NOT_ISSUED&&calls==1);
 assert(remesa_linux_memory_general_scene_dispatch(d,&s,(void *)&p)==REMESA_NOT_ISSUED&&calls==1);
 for(unsigned i=1;i<=3;i++){
  s.operation=i;r=sentinel;
  assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_ISSUED&&calls==i+1);
  assert(r.context==77&&r.operation==i&&r.retired&&r.published&&r.acknowledged&&!r.failed);
  assert(memcalls==m&&!memcmp(&p,&saved,sizeof p));
  r=sentinel;assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_NOT_ISSUED);
  assert(calls==i+1&&!memcmp(&r,&sentinel,sizeof r));
 }
 s.operation=4;assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_NOT_ISSUED&&calls==4);
 assert(!remesa_linux_memory_general_close(d,&response)&&calls==5&&memcalls==m);
 assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_NOT_ISSUED&&calls==5);
 for(unsigned i=0;i<18;i++){
  assert(!ops->gpu_unmap(d,ids[i],0x40000000u+i*4096,4096));
  assert(!ops->unpin(d,ids[i])&&!ops->destroy(d,ids[i]));
 }
 assert(!remesa_linux_memory_detach(d));
 /* A later native uncertainty cannot be converted to NOT_ISSUED, retried or freed. */
 d=setup();assert(!remesa_linux_memory_general_scene_open(d,&p,5,3,1000,&response));
 s.operation=1;assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_ISSUED);
 action_fault=2;byte_fault=offsetof(struct remesa_general_session_request,ledger);
 s.operation=2;r=sentinel;
 assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_ISSUANCE_AMBIGUOUS);
 assert(!memcmp(&r,&sentinel,sizeof r));unsigned c=calls;
 s.operation=3;assert(remesa_linux_memory_general_scene_dispatch(d,&s,&r)==REMESA_ISSUANCE_AMBIGUOUS&&calls==c);
 poisoned(d);
 puts("PASS general scene provider adapter: one GPU ioctl per host operation, three jobs, exact packet, aliases, bounded backpressure, acknowledged close and later uncertainty; GPU0");
 return 0;
}
