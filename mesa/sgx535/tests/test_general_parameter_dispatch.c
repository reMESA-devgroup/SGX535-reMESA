/* SPDX-License-Identifier: MIT. Real Linux adapter, interposed ioctl; GPU0. */
#define REMESA_PARAMETER_TRANSPORT_TEST_MAIN inherited_parameter_transport_main
#include "test_general_parameter_transport.c"
static struct remesa_scene_packet scene_packet(void)
{
 struct remesa_scene_packet p={.owner=77,.serial=1,.version=1,.binding_count=18,.use_count=11,.window_count=2,
  .actions={.schema=1,.bytes=32,.width=1280,.height=800,.region_id=32,.table_id=33,.parameter_id=34},
  .controls={.schema=2,.bytes=80,.width=1280,.height=800,.color_pitch=1280,.depth_pitch=1280,
   .depth_cpp=4,.color_id=35,.depth_id=36,.ta_id=37,.ta_bytes=104,.background_id=38,.event_id=39,.depth_finalize=1}};
 for(unsigned i=0;i<18;i++){p.bindings[i].id=ids[i];p.bindings[i].access=i<5?3:1;}
 for(unsigned i=0;i<11;i++)p.uses[i]=(struct remesa_scene_packet_use){.id=ids[i],.program_bytes=8,.bank=3};
 p.uses[10]=(struct remesa_scene_packet_use){.id=ids[17],.byte_offset=160,.program_bytes=8,.bank=4,.data_master=REMESA_USE_VERTEX};
 for(unsigned i=0;i<2;i++)p.windows[i]=(struct remesa_scene_packet_window){.index_id=ids[14],
  .index_bytes=6,.vertex_id=ids[15],.vertex_bytes=48,.vertex_count=3};
 assert(remesa_scene_packet_valid(&p));return p;
}
int main(void)
{
 for(unsigned fault=0;fault<10;fault++){
  action_fault=byte_fault=io_fault=parameter_calls=0;parameter_byte_fault=SIZE_MAX;
  parameter_errno=parameter_refusal=parameter_positive=0;native_receipt=0;
  ops=remesa_linux_memory_ops();for(unsigned i=0;i<18;i++)ids[i]=32+i;
  struct remesa_linux_memory *d=setup();struct remesa_scene_packet p=scene_packet();
  struct remesa_general_session_request wire;
  assert(!remesa_linux_memory_general_scene_typed_open(d,&p,3,3,1000,&wire));
  uint32_t bits[36];for(unsigned i=0;i<36;i++)bits[i]=0x3f800000;
  struct remesa_gpu_scene_parameters u={.context=77,.serial=1,.published_operation=1,.sequence=1,
   .window={.owner=77,.generation=919,.gpu_va=0x40011000,.id=ids[17],.allocation_bytes=4096,
    .coefficient_dwords=36,.effective_source_dwords=37,.transfer_dwords=40,.use_offset=160,.use_bytes=8,
    .pds_offset=192,.pds_bytes=60,.ta_offset=256,.ta_bytes=8},.draw=1,.words=36,.bits=bits};
  struct remesa_gpu_scene_parameter_receipt receipt={0},saved=receipt;
  assert(remesa_linux_memory_general_scene_parameters(d,&u,&receipt)==EAGAIN&&!parameter_calls);
  struct remesa_gpu_scene_submit submit={.context=77,.operation=1,.packet=&p};struct remesa_gpu_scene_receipt gpu;
  assert(remesa_linux_memory_general_scene_dispatch(d,&submit,&gpu)==REMESA_ISSUED);
  unsigned gpu_calls=calls,memory_calls=memcalls;
  assert(remesa_linux_memory_general_scene_parameters(d,&u,(void *)d)==EINVAL);
  assert(remesa_linux_memory_general_scene_parameters(d,&u,(void *)&u)==EINVAL);
  assert(remesa_linux_memory_general_scene_parameters(d,&u,(void *)bits)==EINVAL);
  assert(!parameter_calls);
  if(fault==1)u.serial++;
  if(fault==2)u.window.id=ids[0];
  if(fault==3)u.window.gpu_va+=4096;
  if(fault==4)u.draw=2;
  if(fault>=1&&fault<=4){
   assert(remesa_linux_memory_general_scene_parameters(d,&u,&receipt)==EINVAL&&!parameter_calls);
   assert(!memcmp(&receipt,&saved,sizeof receipt));cleanup(d);continue;
  }
  if(fault==5)u.published_operation++;
  if(fault==6)u.sequence++;
  if(fault==7)parameter_refusal=ESTALE;
  if(fault==8)parameter_byte_fault=offsetof(struct remesa_general_parameter_request,accepted_sequence);
  if(fault>=5&&fault<=8){
   assert(remesa_linux_memory_general_scene_parameters(d,&u,&receipt)==EOWNERDEAD);
   unsigned n=parameter_calls;
   assert(remesa_linux_memory_general_scene_parameters(d,&u,&receipt)==EOWNERDEAD&&parameter_calls==n);
   assert(!memcmp(&receipt,&saved,sizeof receipt)&&calls==gpu_calls&&memcalls==memory_calls);
   poisoned(d);continue;
  }
  if(fault==9){
   parameter_refusal=ENOMEM;
   assert(remesa_linux_memory_general_scene_parameters(d,&u,&receipt)==ENOMEM);
   assert(!memcmp(&receipt,&saved,sizeof receipt));parameter_refusal=0;
  }
  assert(!remesa_linux_memory_general_scene_parameters(d,&u,&receipt));
  assert(receipt.context==77&&receipt.serial==1&&receipt.published_operation==1&&receipt.sequence==1);
  u.sequence=2;assert(!remesa_linux_memory_general_scene_parameters(d,&u,&receipt)&&receipt.sequence==2);
  assert(calls==gpu_calls&&memcalls==memory_calls); /* Data ACK is not GPU work. */
  submit.operation=2;assert(remesa_linux_memory_general_scene_dispatch(d,&submit,&gpu)==REMESA_ISSUED);
  u.published_operation=2;u.sequence=3;
  assert(!remesa_linux_memory_general_scene_parameters(d,&u,&receipt)&&receipt.sequence==3);
  assert(calls==gpu_calls+1&&memcalls==memory_calls);cleanup(d);
 }
 puts("PASS typed capture-to-Linux data ACKs, separate GPU/data epochs, native generation independence, aliases, atomic refusals and uncertainty; GPU0");return 0;
}
