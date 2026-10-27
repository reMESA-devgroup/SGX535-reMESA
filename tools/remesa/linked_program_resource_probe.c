/* SPDX-License-Identifier: MIT
 * Explicit resource-only qualification. Never submits an SGX job. */
#include "remesa_linux_memory.h"
#include "remesa_shader_program.h"
#include "remesa_resource_layout.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static FILE *journal;
static unsigned memory_calls;
static int record(void *unused,const struct remesa_memory_request *before,
 const struct remesa_memory_request *after,int result,int error,bool entry)
{
 (void)unused;
 const uint32_t header[6]={0x50434c52u,entry?0u:1u,(uint32_t)result,(uint32_t)error,sizeof(*before),0};
 if(entry)memory_calls++;
 return fwrite(header,sizeof(header),1,journal)!=1||
  fwrite(entry?before:after,sizeof(*before),1,journal)!=1?EIO:0;
}
static struct remesa_shader_linkage make_link(unsigned varying)
{
 struct remesa_shader_ir ir={.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_VERTEX,.count=8};
 for(unsigned n=0;n<6;n++)ir.instructions[n]=(struct remesa_ir_instruction){
  .dst=n,.components=n<3||n==5?4:1,.type=REMESA_IR_FLOAT32};
 for(unsigned n=0;n<3;n++){ir.instructions[n].op=REMESA_IR_INPUT;ir.instructions[n].input_slot=n;}
 ir.instructions[3].op=REMESA_IR_DOT3;ir.instructions[3].src[0].reg=1;ir.instructions[3].src[1].reg=2;
 ir.instructions[4].op=REMESA_IR_SATURATE;ir.instructions[4].src[0].reg=3;
 ir.instructions[5].op=REMESA_IR_COMPOSE4;
 for(unsigned n=0;n<4;n++)ir.instructions[5].src[n].reg=4;
 ir.instructions[6]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32};
 ir.instructions[7]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.dst=REMESA_SHADER_IR_NO_REGISTER,.components=4,.type=REMESA_IR_FLOAT32,.src={{5,0}}};
 struct remesa_shader_program v,f;assert(!remesa_shader_program_prepare(&ir,&v));
 if(varying){
  ir=(struct remesa_shader_ir){.schema=REMESA_SHADER_IR_SCHEMA,.stage=REMESA_IR_FRAGMENT,.count=2};
  ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32,.input_slot=1};
  ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_COLOR,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
  assert(!remesa_shader_program_prepare(&ir,&f));
 }else assert(!remesa_shader_program_prepare_color(0xff00ff00u,&f));
 struct remesa_shader_linkage link;assert(!remesa_shader_linkage_prepare(&v,&f,&link));return link;
}
static void empty_info(struct remesa_linux_memory *m)
{
 struct remesa_memory_info info,zero={0};
 assert(!remesa_linux_memory_info(m,0,&info));assert(!memcmp(&info,&zero,sizeof(info)));
}
int main(int argc,char **argv)
{
 const unsigned dimensions[3][2]={{1280,800},{640,640},{641,479}};
 if(argc==2&&!strcmp(argv[1],"--self-test")){
  for(unsigned i=0;i<2;i++){
   struct remesa_shader_linkage link=make_link(i);struct remesa_shader_selected_resources r;
   assert(!remesa_shader_program_selected_resources(&link.vertex,&r)&&r.logical_use_bytes==64&&r.reserved_use_bytes==144);
   assert(!remesa_shader_program_selected_resources(&link.fragment,&r)&&r.logical_use_bytes==8&&r.reserved_use_bytes==(i?16u:8u));
  }
  puts("CPU SELFTEST ONLY; semantic programs/resources valid; GPU0; no DRM opened");return 0;
 }
 if(argc!=4||strcmp(argv[1],"--run")){fprintf(stderr,"usage: %s --self-test | --run /dev/dri/cardN journal.bin\n",argv[0]);return 2;}
 journal=fopen(argv[3],"wbx");assert(journal);
 int fd=open(argv[2],O_RDWR|O_CLOEXEC);assert(fd>=0);
 struct remesa_linux_memory *memory=NULL;const struct remesa_memory_recorder recorder={record,NULL};
 assert(!remesa_linux_memory_begin_recorded(fd,&memory,&recorder));empty_info(memory);
 const struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=memory,
  .context=remesa_linux_memory_context(memory),.va_start=0x20000000,.va_end=0x45000000,
  .byte_budget=16u*1024u*1024u,.resource_only_backend=true,
  .resource_limits={2048,2048,128,16u*1024u*1024u},
  .placement={[REMESA_VA_PROGRAM]={0x20080000,0x200a0000,4096},
   [REMESA_VA_DEFAULT]={0x40001000,0x45000000,4096},[REMESA_VA_SURFACE]={0x40001000,0x45000000,4096}}};
 struct remesa_device device={0};assert(!remesa_persistent_open(&device,&cfg));
 const uint32_t banks[2]={4,3};
 for(unsigned varying=0;varying<2;varying++){
  struct remesa_shader_linkage link=make_link(varying);void *bo=NULL;
  assert(!remesa_persistent_linked_program_prepare(&device,&link,banks,&bo)&&bo);
  struct remesa_persistent_linked_program_backing code;
  assert(!remesa_persistent_linked_program_snapshot(&device,bo,&code));
  assert(code.gpu_va==0x20080000&&code.program_offset[0]==0&&code.program_offset[1]==160);
  assert(code.program_bytes[0]==64&&code.reserved_bytes[0]==144&&code.program_bytes[1]==8&&code.reserved_bytes[1]==(varying?16u:8u));
  struct remesa_memory_info info;assert(!remesa_linux_memory_info(memory,code.handle,&info));
  assert(info.object_pinned&&info.object_mapped_pages==1&&info.verified_pages==1&&!info.leases&&!info.held_jobs&&!info.quarantined);
  unsigned before=memory_calls;
  for(unsigned i=0;i<200;i++)assert(!remesa_persistent_linked_program_snapshot(&device,bo,&code));
  assert(memory_calls==before);
  void *p=NULL;assert(device.ops->map(device.priv,bo,true,&p)==ENOTSUP&&!p);
  assert(!device.ops->map(device.priv,bo,false,&p)&&p);
  unsigned char expected[176]={0};size_t n=0;
  assert(!remesa_shader_program_encode(&link.vertex,expected,144,&n)&&n==64);
  assert(!remesa_shader_program_encode(&link.fragment,expected+160,16,&n)&&n==8);
  assert(!memcmp(p,expected,sizeof(expected)));
  for(unsigned i=176;i<4096;i++)assert(!((unsigned char *)p)[i]);
  device.ops->unmap(device.priv,bo,false);
  if(varying)for(unsigned d=0;d<3;d++){
   const unsigned width=dimensions[d][0],height=dimensions[d][1];struct remesa_resource_layout layout;
   assert(!remesa_resource_layout(&cfg.resource_limits,width,height,4,&layout));
   void *color=NULL,*depth=NULL;
   assert(!remesa_persistent_allocate_placed(&device,layout.bytes,REMESA_VA_SURFACE,&color));
   assert(!remesa_persistent_depth_allocate(&device,width,height,4,&depth));
   struct remesa_persistent_render_target_backing cb;struct remesa_persistent_depth_backing zb;
   assert(!remesa_persistent_render_target_snapshot(&device,color,width,height,layout.stride/4,&cb));
   assert(!remesa_persistent_depth_snapshot(&device,depth,&zb));
   assert(cb.handle!=zb.handle&&cb.handle!=code.handle&&zb.handle!=code.handle);
   assert(cb.gpu_va+cb.allocation_bytes<=zb.gpu_va||zb.gpu_va+zb.allocation_bytes<=cb.gpu_va);
   assert(zb.pitch_pixels==((width+31u)&~31u));
   assert(!remesa_linux_memory_info(memory,cb.handle,&info)&&info.object_pinned&&info.verified_pages==cb.allocation_bytes/4096);
   assert(!remesa_linux_memory_info(memory,zb.handle,&info)&&info.object_pinned&&info.verified_pages==zb.allocation_bytes/4096);
   struct remesa_persistent_linked_program_backing reused;
   before=memory_calls;assert(!remesa_persistent_linked_program_snapshot(&device,bo,&reused)&&memory_calls==before);
   assert(reused.handle==code.handle&&reused.generation==code.generation);
   printf("{\"classification\":\"REAL_GEM_RESOURCE_LAYOUT_ONLY_NOT_RENDERING\",\"width\":%u,\"height\":%u,\"color_stride\":%u,\"color_bytes\":%zu,\"depth_bytes\":%zu,\"program_reused\":true,\"API_contexts\":0,\"GPU_submissions\":0}\n",width,height,layout.stride,cb.allocation_bytes,zb.allocation_bytes);
   device.ops->destroy_bo(device.priv,depth);device.ops->destroy_bo(device.priv,color);
  }
  device.ops->destroy_bo(device.priv,bo);empty_info(memory);
  printf("{\"classification\":\"REAL_GEM_LINKED_PROGRAM_CODE_ONLY_NOT_EXECUTION\",\"varying\":%u,\"effective_VS\":64,\"reserved_VS\":144,\"effective_FS\":8,\"reserved_FS\":%u,\"full_code_readback_exact\":true,\"metadata_200_snapshots_ioctls\":0,\"final_INFO_zero\":true,\"GPU_submissions\":0}\n",varying,varying?16:8);
 }
 assert(!remesa_persistent_close(&device));empty_info(memory);
 assert(!remesa_linux_memory_detach(memory));assert(!close(fd));assert(!fclose(journal));
 printf("{\"classification\":\"RESOURCE_ONLY_SESSION_CLOSED_NOT_OPENGL_GLES_GPU_APPROVAL\",\"GPU_submissions\":0,\"API_contexts\":0,\"memory_calls\":%u}\n",memory_calls);return 0;
}
