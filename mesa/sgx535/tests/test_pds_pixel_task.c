/* SPDX-License-Identifier: MIT
 * Independent scalar CPU-profile arithmetic; no GPU instruction fixture.
 */
int retained_pixel_host_builder_test_main(void);
#define main retained_pixel_host_builder_test_main
#include "test_pds_host_builder.c"
#undef main

static struct remesa_use_entry_plan pixel_entry(uint32_t bank,uint32_t offset)
{
 struct remesa_use_entry_request request={
  .allocation_address=UINT64_C(0x20000000),.allocation_bytes=UINT64_C(0x80000),
  .entry_offset=offset,.program_bytes=8,.bank=bank,.data_master=REMESA_USE_PIXEL
 };
 struct remesa_use_entry_plan entry;
 CHECK(!remesa_use_entry_prepare(&request,&entry));return entry;
}
static void codecs(void)
{
 /* Cover every losslessly representable bank/offset pair and both admitted
  * metadata values. This proves the CPU codec, not physical task semantics. */
 for(uint32_t bank=0;bank<16;bank++)for(uint32_t offset=0;offset<UINT32_C(0x80000);offset+=16) {
  struct remesa_use_entry_plan entry=pixel_entry(bank,offset);
  for(uint32_t metadata=0;metadata<2;metadata++) {
   struct remesa_pds_cpu_pixel_task task;
   struct remesa_pds_cpu_pixel_task_fields fields;
   CHECK(!remesa_pds_cpu_pixel_task_pack(&entry,metadata,&task));
   CHECK(task.words[0]==(UINT32_C(0x100000)|bank|((offset>>15)<<4)|(((offset>>4)&2047)<<8)));
   CHECK(task.words[1]==metadata*UINT32_C(0x08000000)&&task.words[2]==UINT32_C(0x20));
   CHECK(!remesa_pds_cpu_pixel_task_unpack(&task,&fields));
   CHECK(fields.use.bank==bank&&fields.use.entry_byte_offset==offset&&fields.metadata6==metadata);
  }
 }
 struct remesa_use_entry_plan entry=pixel_entry(2,32),saved_entry=entry;
 struct remesa_pds_cpu_pixel_task valid,task,saved_task;
 CHECK(!remesa_pds_cpu_pixel_task_pack(&entry,0,&valid));
 for(unsigned word=0;word<3;word++)for(unsigned bit=0;bit<32;bit++) {
  task=valid;task.words[word]^=UINT32_C(1)<<bit;
  struct remesa_pds_cpu_pixel_task_fields fields,saved;
  memset(&fields,0xa5,sizeof(fields));saved=fields;
  int error=remesa_pds_cpu_pixel_task_unpack(&task,&fields);
  if((word==0&&bit<19)||(word==1&&bit==27))CHECK(!error);
  else {CHECK(error==EPROTO);CHECK(!memcmp(&fields,&saved,sizeof(fields)));}
 }
 memset(&task,0xa5,sizeof(task));saved_task=task;
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,2,&task)==ERANGE);
 CHECK(!memcmp(&task,&saved_task,sizeof(task)));
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,UINT32_MAX,&task)==ERANGE);
 CHECK(!memcmp(&task,&saved_task,sizeof(task)));
 CHECK(!remesa_use_base_pack_address(entry.base_address,REMESA_USE_VERTEX,&entry.register_word));
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,0,&task)==ENOTSUP);
 CHECK(!memcmp(&task,&saved_task,sizeof(task)));
 CHECK(!remesa_use_base_pack_address(entry.base_address,REMESA_USE_EDM,&entry.register_word));
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,0,&task)==ENOTSUP);
 CHECK(!memcmp(&task,&saved_task,sizeof(task)));
 entry=saved_entry;entry.entry_byte_offset++;
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,0,&task)!=0);
 CHECK(!memcmp(&task,&saved_task,sizeof(task)));
 entry=saved_entry;
 CHECK(remesa_pds_cpu_pixel_task_pack(NULL,0,&task)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_pack((void *)(UINTPTR_MAX-3),0,&task)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,0,NULL)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,0,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_pack(&entry,0,(void *)&entry)==EINVAL);
 CHECK(!memcmp(&entry,&saved_entry,sizeof(entry)));
 CHECK(remesa_pds_cpu_pixel_task_unpack(NULL,(void *)&entry)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_unpack((void *)(UINTPTR_MAX-3),(void *)&entry)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_unpack(&task,NULL)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_unpack(&task,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_unpack(&task,(void *)&task)==EINVAL);
 CHECK(!memcmp(&task,&saved_task,sizeof(task)));
}
static void recipe(void)
{
 struct remesa_pds_data_cell cells[12],saved[12];
 for(uint32_t metadata=0;metadata<2;metadata++) {
  CHECK(!remesa_pds_cpu_pixel_task_recipe(metadata,cells));
  for(unsigned c=0;c<12;c++) {
   CHECK(cells[c].kind==(c==0?REMESA_PDS_DATA_USE_ENTRY_LOWER19:c==1?REMESA_PDS_DATA_CPU_META6:REMESA_PDS_DATA_LITERAL32));
   CHECK(cells[c].value==(c==0?UINT32_C(0x100000):c==1?metadata:c==8?UINT32_C(0x20):0));
  }
 }
 memset(cells,0xa5,sizeof(cells));memcpy(saved,cells,sizeof(cells));
 CHECK(remesa_pds_cpu_pixel_task_recipe(2,cells)==ERANGE);
 CHECK(!memcmp(cells,saved,sizeof(cells)));
 CHECK(remesa_pds_cpu_pixel_task_recipe(UINT32_MAX,cells)==ERANGE);
 CHECK(!memcmp(cells,saved,sizeof(cells)));
 CHECK(remesa_pds_cpu_pixel_task_recipe(0,NULL)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_recipe(0,(void *)(UINTPTR_MAX-3))==EINVAL);
}
static void pixel_fixture(struct fixture *f,int separated,uint32_t metadata,
                          struct remesa_pds_host_task_plan tasks[2])
{
 prepare(f,separated);
 CHECK(!remesa_pds_cpu_pixel_task_recipe(metadata,f->cells[1]));
 const uint32_t banks[2]={1,2};const uint64_t offsets[2]={4,0};
 CHECK(!remesa_pds_prepare_use_links(f->plans,f->programs,banks,offsets,f->links));
 struct remesa_pds_host_transfer transfers[2][12];
 struct remesa_pds_host_transfer_request requests[2];
 for(unsigned stage=0;stage<2;stage++) {
  for(unsigned c=0;c<12;c++)transfers[stage][c]=(struct remesa_pds_host_transfer){11-c,63-(11-c)};
  requests[stage]=(struct remesa_pds_host_transfer_request){transfers[stage],12};
 }
 CHECK(!remesa_pds_prepare_task_pair(f->plans,f->programs,f->links,f->recipes,requests,tasks));
 CHECK(!remesa_pds_validate_task_pair(f->plans,f->programs,f->links,f->recipes,requests,tasks));
 CHECK(!remesa_pds_build_host_data_pair(f->plans,f->programs,f->links,f->recipes,f->data));
 CHECK(!remesa_pds_validate_host_data_pair(f->plans,f->programs,f->links,f->recipes,f->data));
}
static void consumer(void)
{
 for(unsigned separated=0;separated<2;separated++)for(uint32_t metadata=0;metadata<2;metadata++) {
  struct fixture f;struct remesa_pds_host_task_plan tasks[2];pixel_fixture(&f,separated,metadata,tasks);
  struct remesa_pds_cpu_pixel_task_output out;
  CHECK(!remesa_pds_cpu_pixel_task_prepare(&tasks[1],&out));
  CHECK(out.schema==REMESA_PDS_CPU_PIXEL_TASK_SCHEMA&&out.cpu_word_count==3&&!out.reserved);
  CHECK(out.task.words[0]==get32(f.host[1]+16));
  CHECK(out.task.words[1]==get32(f.host[1]+20));
  CHECK(out.task.words[2]==get32(f.host[1]+48));
  CHECK(out.data_to_ds_error==ENOTSUP&&out.dispatch_error==ENOTSUP&&out.termination_error==ENOTSUP);
  const unsigned sources[3]={0,1,8};
  for(unsigned n=0;n<3;n++) {
   CHECK(out.sources[n].data_cell==sources[n]&&out.sources[n].logical_slot==63-sources[n]);
   CHECK(out.sources[n].value==out.task.words[n]);
   CHECK(out.sources[n].data_address==tasks[1].link.data_address+4*sources[n]);
  }
 }
 struct fixture f;struct remesa_pds_host_task_plan tasks[2],valid;
 pixel_fixture(&f,0,1,tasks);valid=tasks[1];
 tasks[1].slots[11].logical_slot=0;
 struct remesa_pds_cpu_pixel_task_output rerouted;
 CHECK(!remesa_pds_cpu_pixel_task_prepare(&tasks[1],&rerouted));
 for(unsigned c=0;c<64;c++)for(unsigned field=0;field<6;field++) {
  tasks[1]=valid;
  switch(field) {
  case 0:tasks[1].slots[c].data_cell++;break;
  case 1:tasks[1].slots[c].logical_slot++;break;
  case 2:tasks[1].slots[c].data_kind++;break;
  case 3:tasks[1].slots[c].value^=1;break;
  case 4:tasks[1].slots[c].cpu_relocation_mask^=1;break;
  case 5:tasks[1].slots[c].data_address++;break;
  }
  struct remesa_pds_cpu_pixel_task_output out,saved;memset(&out,0xa5,sizeof(out));saved=out;
  int error=remesa_pds_cpu_pixel_task_prepare(&tasks[1],&out);
  CHECK(error!=0);CHECK(!memcmp(&out,&saved,sizeof(out)));
 }
 for(unsigned mode=0;mode<36;mode++) {
  tasks[1]=valid;
  const struct remesa_pds_host_task_plan *input=&tasks[1];
  switch(mode) {
  case 0:input=NULL;break;
  case 1:input=(void *)(UINTPTR_MAX-3);break;
  case 2:tasks[1].schema++;break;
  case 3:tasks[1].reserved=1;break;
  case 4:tasks[1].stage=REMESA_IR_VERTEX;break;
  case 5:tasks[1].link.stage=REMESA_IR_VERTEX;break;
  case 6:tasks[1].slot_count--;break;
  case 7:tasks[1].entry_logical_slot--;break;
  case 8:tasks[1].data_to_ds_error=0;break;
  case 9:tasks[1].dispatch_error=0;break;
  case 10:tasks[1].termination_error=0;break;
  case 11:tasks[1].link.request.operation=0;break;
  case 12:tasks[1].link.request.generation=0;break;
  case 13:tasks[1].link.request.data.bytes=44;break;
  case 14:tasks[1].link.request.data.address++;break;
  case 15:tasks[1].link.request.data.offset++;break;
  case 16:tasks[1].link.data_byte_offset=4;break;
  case 17:tasks[1].link.data_address++;break;
  case 18:tasks[1].link.program_identity=0;break;
  case 19:tasks[1].link.program.owner_cookie++;break;
  case 20:tasks[1].link.program.bytes++;break;
  case 21:tasks[1].link.entry.entry_byte_offset++;break;
  case 22:tasks[1].link.entry.program_end++;break;
  case 23:tasks[1].link.entry.register_word^=UINT32_C(1)<<25;break;
  case 24:tasks[1].link.address_bits^=1;break;
  case 25:tasks[1].slots[1].value=UINT32_C(0x10000000);break;
  case 26:tasks[1].slots[11].logical_slot=tasks[1].slots[10].logical_slot;break;
  case 27:tasks[1].slots[11].logical_slot=64;break;
  case 28:tasks[1].link.request.data.allocation_bytes=UINT64_MAX;break;
  case 29:tasks[1].link.program.mapping_generation=0;break;
  case 30:tasks[1].link.program.offset++;break;
  case 31:tasks[1].link.request.data.address=tasks[1].link.program.address;break;
  case 32:tasks[1].link.entry.bank=16;break;
  case 33:tasks[1].link.request.data.bytes=52;break;
  case 34:tasks[1].link.request.data.bytes=0;break;
  case 35:tasks[1].link.program.allocation_bytes=UINT64_MAX;break;
  }
  struct remesa_pds_cpu_pixel_task_output out,saved;memset(&out,0xa5,sizeof(out));saved=out;
  CHECK(remesa_pds_cpu_pixel_task_prepare(input,&out)!=0);
  CHECK(!memcmp(&out,&saved,sizeof(out)));
 }
 tasks[1]=valid;
 CHECK(remesa_pds_cpu_pixel_task_prepare(&tasks[1],NULL)==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_prepare(&tasks[1],(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(remesa_pds_cpu_pixel_task_prepare(&tasks[1],(void *)&tasks[1])==EINVAL);
 CHECK(!memcmp(&tasks[1],&valid,sizeof(valid)));
}
int main(void)
{
 codecs();recipe();consumer();
 printf("PASS PDS original CPU pixel task consumer: %u checks; physical CODE remains ENOTSUP; no GPU execution\n",checks);
 return 0;
}
