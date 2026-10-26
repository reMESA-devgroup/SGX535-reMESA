/* SPDX-License-Identifier: MIT
 * Original logical task-input graph. No DS/ISA fixture or GPU execution.
 */
int retained_host_builder_test_main(void);
#define main retained_host_builder_test_main
#include "test_pds_host_builder.c"
#undef main

static void routing(struct remesa_pds_host_transfer x[2][64],
                    struct remesa_pds_host_transfer_request r[2],unsigned count)
{
 for(unsigned i=0;i<2;i++) {
  for(unsigned c=0;c<count;c++)x[i][c]=(struct remesa_pds_host_transfer){c,63-c};
  r[i]=(struct remesa_pds_host_transfer_request){x[i],count};
 }
}
static void task_positive(int separated)
{
 struct fixture f;prepare(&f,separated);
 struct remesa_pds_host_transfer transfers[2][64];
 struct remesa_pds_host_transfer_request requests[2];routing(transfers,requests,12);
 struct remesa_pds_host_task_plan tasks[2],canonical[2];
 CHECK(!remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
 CHECK(!remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
 memcpy(canonical,tasks,sizeof(tasks));
 for(unsigned i=0;i<2;i++) {
  CHECK(tasks[i].schema==REMESA_PDS_HOST_TASK_SCHEMA&&tasks[i].reserved==0);
  CHECK(tasks[i].stage==f.plans[i].stage&&tasks[i].slot_count==12);
  CHECK(tasks[i].entry_logical_slot==62-i);
  CHECK(tasks[i].data_to_ds_error==ENOTSUP&&tasks[i].dispatch_error==ENOTSUP&&tasks[i].termination_error==ENOTSUP);
  for(unsigned c=0;c<64;c++) {
   const struct remesa_pds_host_task_slot *s=&tasks[i].slots[c];
   if(c<12) {
    CHECK(s->data_cell==c&&s->logical_slot==63-c);
    CHECK(s->data_kind==f.cells[i][c].kind);
    CHECK(s->value==(f.cells[i][c].value|(c==1+i?f.links[i].address_bits:0)));
    CHECK(s->data_address==f.links[i].request.data.address+f.links[i].request.data.offset+4*c);
    CHECK(s->cpu_relocation_mask==(c==1+i?UINT32_C(0x7ffff):0));
   } else CHECK(!s->data_cell&&!s->logical_slot&&!s->data_kind&&!s->value&&!s->cpu_relocation_mask&&!s->data_address);
  }
  /* A different transfer enumeration resolves the same canonical graph. */
  for(unsigned c=0;c<6;c++) {
   struct remesa_pds_host_transfer t=transfers[i][c];
   transfers[i][c]=transfers[i][11-c];transfers[i][11-c]=t;
  }
 }
 CHECK(!remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
 for(unsigned i=0;i<2;i++)for(unsigned c=0;c<64;c++)
  CHECK(!memcmp(&tasks[i].slots[c],&canonical[i].slots[c],sizeof(tasks[i].slots[c])));
 CHECK(!remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,canonical));
 /* CPU literal values are known, but no task-control qualification is gained. */
 f.cells[1][2].value^=UINT32_C(0x80000000);
 CHECK(remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks)==ESTALE);
 CHECK(!remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
 CHECK(tasks[1].slots[2].cpu_relocation_mask==UINT32_C(0x7ffff));
 CHECK(tasks[1].dispatch_error==ENOTSUP&&tasks[1].termination_error==ENOTSUP);
}
static void task_invalid(void)
{
 for(unsigned mode=0;mode<35;mode++) {
  struct fixture f;prepare(&f,mode&1);
  struct remesa_pds_host_transfer transfers[2][64];
  struct remesa_pds_host_transfer_request requests[2];routing(transfers,requests,12);
  const struct remesa_pds_plan *plans=f.plans;
  const struct remesa_pds_binding *programs=f.programs;
  const struct remesa_pds_use_link *links=f.links;
  const struct remesa_pds_host_data_recipe *recipes=f.recipes;
  const struct remesa_pds_host_transfer_request *routes=requests;
  struct remesa_pds_host_task_plan tasks[2],saved[2];
  memset(tasks,0xa5,sizeof(tasks));memcpy(saved,tasks,sizeof(tasks));
  switch(mode) {
  case 0:plans=NULL;break;
  case 1:programs=NULL;break;
  case 2:links=NULL;break;
  case 3:recipes=NULL;break;
  case 4:routes=NULL;break;
  case 5:plans=(void *)(UINTPTR_MAX-3);break;
  case 6:programs=(void *)(UINTPTR_MAX-3);break;
  case 7:links=(void *)(UINTPTR_MAX-3);break;
  case 8:recipes=(void *)(UINTPTR_MAX-3);break;
  case 9:routes=(void *)(UINTPTR_MAX-3);break;
  case 10:requests[1].transfers=NULL;break;
  case 11:requests[1].transfers=(void *)(UINTPTR_MAX-3);break;
  case 12:requests[1].transfer_count=SIZE_MAX;break;
  case 13:requests[1].transfer_count=0;break;
  case 14:requests[1].transfer_count=11;break;
  case 15:transfers[1][11].data_cell=12;break;
  case 16:transfers[1][11].logical_slot=64;break;
  case 17:transfers[1][11].data_cell=10;break;
  case 18:transfers[1][11].logical_slot=transfers[1][10].logical_slot;break;
  case 19:f.cells[1][2].kind=REMESA_PDS_DATA_LITERAL32;break;
  case 20:f.cells[1][2].value|=1;break;
  case 21:f.cells[1][3].kind=REMESA_PDS_DATA_USE_ENTRY_LOWER19;f.cells[1][3].value=0;break;
  case 22:f.cells[1][11].kind=99;break;
  case 23:f.recipes[1].cell_count=11;break;
  case 24:f.recipes[1].cells=NULL;break;
  case 25:f.recipes[1].cells=(void *)(UINTPTR_MAX-3);break;
  case 26:f.links[1].address_bits^=1;break;
  case 27:f.programs[1].mapping_generation++;break;
  case 28:f.plans[1].stage=REMESA_IR_VERTEX;break;
  case 29:f.plans[1].request.generation++;break;
  case 30:f.recipes[1].cells=(void *)tasks;break;
  case 31:requests[1].transfers=(void *)tasks;break;
  case 32:f.recipes[1].cells=(void *)requests;break;
  case 33:requests[1].transfers=(void *)f.plans;break;
  case 34:requests[1].transfers=(void *)f.cells[0];break;
  }
  CHECK(remesa_pds_prepare_task_pair(plans,programs,links,recipes,routes,tasks)!=0);
  CHECK(!memcmp(tasks,saved,sizeof(tasks)));
  unchanged(&f,0xa5);
 }
 struct fixture f;prepare(&f,0);
 struct remesa_pds_host_transfer transfers[2][64];
 struct remesa_pds_host_transfer_request requests[2];routing(transfers,requests,12);
 CHECK(remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,NULL)==EINVAL);
 CHECK(remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,(void *)f.plans)==EINVAL);
 CHECK(remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,NULL)==EINVAL);
}
static void task_tampering(void)
{
 struct fixture f;prepare(&f,0);
 struct remesa_pds_host_transfer transfers[2][64];
 struct remesa_pds_host_transfer_request requests[2];routing(transfers,requests,12);
 struct remesa_pds_host_task_plan tasks[2],canonical[2];
 CHECK(!remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,canonical));
 for(unsigned stage=0;stage<2;stage++)for(unsigned mode=0;mode<12;mode++) {
  memcpy(tasks,canonical,sizeof(tasks));
  switch(mode) {
  case 0:tasks[stage].schema++;break;
  case 1:tasks[stage].reserved=1;break;
  case 2:tasks[stage].stage=stage?REMESA_IR_VERTEX:REMESA_IR_FRAGMENT;break;
  case 3:tasks[stage].slot_count++;break;
  case 4:tasks[stage].entry_logical_slot++;break;
  case 5:tasks[stage].data_to_ds_error=0;break;
  case 6:tasks[stage].dispatch_error=0;break;
  case 7:tasks[stage].termination_error=0;break;
  case 8:tasks[stage].link.address_bits^=1;break;
  case 9:tasks[stage].link.request.generation++;break;
  case 10:tasks[stage].link.program_identity++;break;
  case 11:tasks[stage].link.stage=stage?REMESA_IR_VERTEX:REMESA_IR_FRAGMENT;break;
  }
  CHECK(remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks)!=0);
 }
 /* A second self-consistent link pair is not the supplied current pair. */
 {
  struct remesa_pds_use_link alternate[2];
  const uint32_t banks[2]={3,2};const uint64_t offsets[2]={12,8};
  CHECK(!remesa_pds_prepare_use_links(f.plans,f.programs,banks,offsets,alternate));
  CHECK(!remesa_pds_validate_use_links(f.plans,f.programs,alternate));
  memcpy(tasks,canonical,sizeof(tasks));tasks[0].link=alternate[0];
  CHECK(remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks)==ESTALE);
 }
 for(unsigned stage=0;stage<2;stage++)for(unsigned cell=0;cell<64;cell++)for(unsigned mode=0;mode<6;mode++) {
  memcpy(tasks,canonical,sizeof(tasks));
  struct remesa_pds_host_task_slot *s=&tasks[stage].slots[cell];
  switch(mode) {
  case 0:s->data_cell++;break;
  case 1:s->logical_slot++;break;
  case 2:s->data_kind++;break;
  case 3:s->value^=1;break;
  case 4:s->cpu_relocation_mask|=UINT32_C(0x80000000);break;
  case 5:s->data_address++;break;
  }
  CHECK(remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks)==ESTALE);
 }
}
static void task_boundary(void)
{
 struct fixture f;prepare(&f,0);
 struct remesa_pds_data_cell cells[2][65];
 struct remesa_pds_host_transfer transfers[2][64];
 struct remesa_pds_host_transfer_request requests[2];routing(transfers,requests,64);
 struct remesa_pds_host_task_plan tasks[2],saved[2];
 for(unsigned i=0;i<2;i++) {
  memcpy(cells[i],f.cells[i],sizeof(f.cells[i]));
  for(unsigned c=12;c<65;c++)cells[i][c]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_LITERAL32,c};
  struct remesa_pds_request r=f.plans[i].request;r.data.bytes=256;r.code.offset=272;
  CHECK(!remesa_pds_prepare(&f.semantic[i],&r,&f.plans[i]));
  f.recipes[i]=(struct remesa_pds_host_data_recipe){cells[i],64};
 }
 const uint32_t banks[2]={1,2};const uint64_t offsets[2]={4,8};
 CHECK(!remesa_pds_prepare_use_links(f.plans,f.programs,banks,offsets,f.links));
 CHECK(!remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
 CHECK(!remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
 CHECK(tasks[0].slot_count==64&&tasks[1].slot_count==64);
 memcpy(saved,tasks,sizeof(tasks));f.recipes[1].cell_count=65;
 CHECK(remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks)==E2BIG);
 CHECK(!memcmp(tasks,saved,sizeof(tasks)));
}
static void task_cpu_meta6(void)
{
 uint32_t word=UINT32_C(0xdeadbeef),metadata=123;
 CHECK(remesa_pds_cpu_meta6_pack(0,NULL)==EINVAL);
 CHECK(remesa_pds_cpu_meta6_unpack(0,NULL)==EINVAL);
 CHECK(remesa_pds_cpu_meta6_pack(0,(void *)(UINTPTR_MAX-1))==EINVAL);
 CHECK(remesa_pds_cpu_meta6_unpack(0,(void *)(UINTPTR_MAX-1))==EINVAL);
 for(unsigned value=0;value<2;value++) {
  CHECK(!remesa_pds_cpu_meta6_pack(value,&word)&&word==(uint32_t)value<<27);
  CHECK(!remesa_pds_cpu_meta6_unpack(word,&metadata)&&metadata==value);
 }
 const uint32_t invalid_values[]={2,31,32,UINT32_MAX};
 for(unsigned n=0;n<sizeof(invalid_values)/sizeof(invalid_values[0]);n++) {
  word=UINT32_C(0xdeadbeef);
  CHECK(remesa_pds_cpu_meta6_pack(invalid_values[n],&word)==ERANGE);
  CHECK(word==UINT32_C(0xdeadbeef));
 }
 for(unsigned bit=0;bit<32;bit++)if(bit!=27) {
  metadata=123;
  CHECK(remesa_pds_cpu_meta6_unpack((UINT32_C(1)<<bit)|UINT32_C(0x08000000),&metadata)==EPROTO);
  CHECK(metadata==123);
 }
 for(unsigned value=0;value<2;value++) {
  struct fixture f;prepare(&f,0);
  struct remesa_pds_host_transfer transfers[2][64];
  struct remesa_pds_host_transfer_request requests[2];routing(transfers,requests,12);
  struct remesa_pds_host_task_plan tasks[2],saved[2];
  for(unsigned i=0;i<2;i++)f.cells[i][4]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_CPU_META6,value};
  CHECK(!remesa_pds_build_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data));
  CHECK(!remesa_pds_validate_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data));
  CHECK(!remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
  CHECK(!remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks));
  for(unsigned i=0;i<2;i++) {
   CHECK(get32(f.host[i]+32)==(uint32_t)value<<27);
   CHECK(tasks[i].slots[4].data_kind==REMESA_PDS_DATA_CPU_META6);
   CHECK(tasks[i].slots[4].value==(uint32_t)value<<27);
   CHECK(!tasks[i].slots[4].cpu_relocation_mask);
  }
  memcpy(saved,tasks,sizeof(tasks));
  unsigned char host[sizeof(f.host)];memcpy(host,f.host,sizeof(host));
  f.cells[1][4].value=2;
  CHECK(remesa_pds_build_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data)==ERANGE);
  CHECK(!memcmp(host,f.host,sizeof(host)));
  CHECK(remesa_pds_prepare_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks)==ERANGE);
  CHECK(!memcmp(tasks,saved,sizeof(tasks)));
  f.cells[1][4].value=1-value;
  CHECK(remesa_pds_validate_task_pair(f.plans,f.programs,f.links,f.recipes,requests,tasks)==ESTALE);
  CHECK(remesa_pds_validate_host_data_pair(f.plans,f.programs,f.links,f.recipes,f.data)==EPROTO);
 }
}
int main(void)
{
 task_cpu_meta6();task_positive(0);task_positive(1);task_invalid();task_tampering();task_boundary();
 printf("PASS PDS original logical task contract: %u checks; physical DS/dispatch/termination ENOTSUP; no GPU\n",checks);
 return 0;
}
