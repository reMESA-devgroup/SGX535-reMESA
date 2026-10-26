/* SPDX-License-Identifier: MIT
 * Typed logical task routing / VS exports through the existing owned pipeline.
 * No device, MMIO, GPU, completion simulation or instruction template.
 */
int inherited_execution_pipeline_fixture(int,char **);
#define main inherited_execution_pipeline_fixture
#include "test_host_shader_pipeline.c"
#undef main
#include "remesa_device.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"

static void routing(struct remesa_pds_host_transfer x[2][12],
 struct remesa_pds_host_transfer_request requests[2])
{
 for(unsigned s=0;s<2;s++) {
  for(unsigned i=0;i<12;i++)x[s][i]=(struct remesa_pds_host_transfer){.data_cell=11-i,.logical_slot=63-i};
  requests[s]=(struct remesa_pds_host_transfer_request){.transfers=x[s],.transfer_count=12};
 }
}
static void contract_cases(uint32_t color)
{
 struct remesa_shader_draw_programs pair=programs(color);
 struct remesa_pds_data_cell cells[2][12];struct remesa_shader_host_pipeline_request r=request(cells);
 cells[0][0]=(struct remesa_pds_data_cell){.kind=REMESA_PDS_DATA_CPU_META6,.value=0};
 cells[1][0]=(struct remesa_pds_data_cell){.kind=REMESA_PDS_DATA_CPU_META6,.value=1};
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(!remesa_shader_host_pipeline_create(&pair,&r,&p));
 struct remesa_pds_host_transfer routes[2][12];struct remesa_pds_host_transfer_request transfers[2];
 routing(routes,transfers);
 struct remesa_shader_execution_contract contract,saved;
 CHECK(!remesa_shader_host_pipeline_prepare_contract(p,transfers,&contract));saved=contract;
 CHECK(!remesa_shader_host_pipeline_validate_contract(p,transfers,&contract));
 CHECK(contract.blockers==127&&contract.execution_error==ENOTSUP&&contract.fragment_bytes==8);
 CHECK(contract.vertex.route_count==4&&contract.vertex.candidate_bytes==32);
 CHECK(contract.fragment_termination_error==ENOTSUP&&contract.completion_error==ENOTSUP);
 for(unsigned s=0;s<2;s++) {
  CHECK(contract.task[s].slot_count==12);
  CHECK(contract.task[s].entry_logical_slot==52+r.data_byte_offsets[s]/4);
  CHECK(contract.task[s].data_to_ds_error==ENOTSUP&&contract.task[s].dispatch_error==ENOTSUP&&
   contract.task[s].termination_error==ENOTSUP);
  CHECK(contract.task[s].slots[0].value==(s?UINT32_C(1)<<27:0));
  CHECK(!contract.task[s].slots[0].cpu_relocation_mask);
  for(unsigned i=0;i<12;i++)CHECK(contract.task[s].slots[i].logical_slot==52+i);
 }
 unsigned char bytes[64],before[64];memset(bytes,0xa5,sizeof(bytes));memcpy(before,bytes,sizeof(bytes));
 size_t written=987;
 CHECK(remesa_shader_host_pipeline_encode_contract(p,transfers,&contract,bytes,sizeof(bytes),&written)==ENOTSUP);
 CHECK(written==987&&!memcmp(bytes,before,sizeof(bytes))&&!memcmp(&contract,&saved,sizeof(contract)));
 CHECK(remesa_shader_host_pipeline_encode_contract(p,transfers,&contract,NULL,0,&written)==ENOTSUP&&written==987);
 CHECK(remesa_shader_host_pipeline_encode_contract(p,transfers,&contract,NULL,1,&written)==EINVAL&&written==987);
 CHECK(remesa_shader_host_pipeline_encode_contract(p,transfers,&contract,bytes,sizeof(bytes),(void *)(bytes+1))==EINVAL);
 CHECK(remesa_shader_host_pipeline_encode_contract(p,transfers,&contract,(void *)&contract,8,&written)==EINVAL);
 /* Every semantic contract byte is checked, including canonical unused slots.
  * This also tests that clearing an error/blocker never admits execution. */
 unsigned mutations=0;
 for(size_t n=0;n<sizeof(contract);n++) {
  ((unsigned char *)&contract)[n]^=1;
  if(remesa_shader_host_pipeline_validate_contract(p,transfers,&contract))mutations++;
  ((unsigned char *)&contract)[n]^=1;
 }
 /* Struct padding is not a contract field; all actual fields have mutations
  * detected. Dedicated mutations below make every admission flag explicit. */
 CHECK(mutations>sizeof(contract)/2);
 for(unsigned n=0;n<7;n++) {
  contract.blockers^=1u<<n;
  CHECK(remesa_shader_host_pipeline_encode_contract(p,transfers,&contract,bytes,sizeof(bytes),&written)==EPROTO);
  CHECK(written==987&&!memcmp(bytes,before,sizeof(bytes)));contract=saved;
 }
 contract.execution_error=0;CHECK(remesa_shader_host_pipeline_validate_contract(p,transfers,&contract)==EPROTO);contract=saved;
 contract.task[1].termination_error=0;
 CHECK(remesa_shader_host_pipeline_validate_contract(p,transfers,&contract)!=0);contract=saved;
 contract.vertex.export_bank_error=0;
 CHECK(remesa_shader_host_pipeline_validate_contract(p,transfers,&contract)==EPROTO);contract=saved;
 struct remesa_shader_execution_contract sentinel,unchanged;memset(&sentinel,0xa5,sizeof(sentinel));unchanged=sentinel;
 for(unsigned mode=0;mode<6;mode++) {
  struct remesa_pds_host_transfer_request invalid[2]={transfers[0],transfers[1]};
  struct remesa_pds_host_transfer bad[12];memcpy(bad,routes[1],sizeof(bad));invalid[1].transfers=bad;
  if(mode==0)invalid[1].transfer_count=0;
  if(mode==1)invalid[1].transfer_count=65;
  if(mode==2)bad[0].logical_slot=64;
  if(mode==3)bad[0].data_cell=12;
  if(mode==4)bad[0].logical_slot=bad[1].logical_slot;
  if(mode==5)invalid[1].transfers=(void *)(UINTPTR_MAX-3);
  CHECK(remesa_shader_host_pipeline_prepare_contract(p,invalid,&sentinel)!=0);
  CHECK(!memcmp(&sentinel,&unchanged,sizeof(sentinel)));
 }
 CHECK(remesa_shader_host_pipeline_prepare_contract(p,transfers,(void *)transfers)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_contract(p,transfers,(void *)routes)==EINVAL);
 struct remesa_shader_host_pipeline_view view;CHECK(!remesa_shader_host_pipeline_inspect(p,&view));
 CHECK(remesa_shader_host_pipeline_prepare_contract(p,transfers,(void *)view.links)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_contract(p,transfers,(void *)view.data[0].storage.bytes)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_contract(p,transfers,(void *)((const unsigned char *)view.image.programs+
  sizeof(*view.image.programs)))==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_contract(p,transfers,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(!remesa_shader_host_pipeline_validate(p));
 memset(&r,0,sizeof(r));memset(cells,0,sizeof(cells));memset(&pair,0,sizeof(pair));
 CHECK(!remesa_shader_host_pipeline_validate_contract(p,transfers,&contract));
 remesa_shader_host_pipeline_destroy(p);
}
static int no_allocate(void *p,size_t bytes,void **out)
{(void)p;(void)bytes;(void)out;return ENOTSUP;}
static void no_destroy(void *p,void *object){(void)p;(void)object;}
static int no_map(void *p,void *object,bool write,void **out)
{(void)p;(void)object;(void)write;(void)out;return ENOTSUP;}
static void no_unmap(void *p,void *object,bool write){(void)p;(void)object;(void)write;}
static int forbidden_submit(void *p,void *object,const struct remesa_draw_packet *packet,void **fence)
{(void)p;(void)object;(void)packet;(void)fence;assert(!"no submission allowed");return ENOTSUP;}
static int no_wait(void *p,void *object,uint64_t timeout,bool visibility)
{(void)p;(void)object;(void)timeout;(void)visibility;return ENOTSUP;}
static void context_cases(void)
{
 const struct remesa_device_ops ops={.allocate=no_allocate,.destroy_bo=no_destroy,
  .map=no_map,.unmap=no_unmap,.submit=forbidden_submit,.wait=no_wait,.destroy_fence=no_destroy};
 struct remesa_device device={.ops=&ops,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct pipe_shader_state shader={.type=PIPE_SHADER_IR_NIR,.ir.nir=vertex_fixture()};
 void *vs=ctx->create_vs_state(ctx,&shader);CHECK(vs);ctx->bind_vs_state(ctx,vs);
 shader.ir.nir=fixture(UINT32_C(0xff00ff00));void *fs=ctx->create_fs_state(ctx,&shader);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 struct remesa_pds_data_cell cells[2][12];struct remesa_shader_host_pipeline_request r=request(cells);
 cells[1][0]=(struct remesa_pds_data_cell){.kind=REMESA_PDS_DATA_CPU_META6,.value=1};
 struct remesa_shader_host_pipeline *p=NULL;CHECK(!remesa_context_prepare_host_shader_pipeline(ctx,&r,&p));
 struct remesa_pds_host_transfer routes[2][12];struct remesa_pds_host_transfer_request transfers[2];routing(routes,transfers);
 struct remesa_shader_execution_contract contract;
 CHECK(!remesa_context_prepare_shader_contract(ctx,p,&r,transfers,&contract));
 CHECK(!remesa_context_revalidate_shader_contract(ctx,p,&r,transfers,&contract));
 CHECK(remesa_context_prepare_shader_contract(ctx,p,&r,transfers,(void *)ctx)==EINVAL);
 CHECK(remesa_context_prepare_shader_contract(ctx,p,&r,transfers,(void *)fs)==EINVAL);
 CHECK(remesa_context_prepare_shader_contract(ctx,p,&r,transfers,(void *)&r)==EINVAL);
 CHECK(remesa_context_prepare_shader_contract(ctx,p,&r,transfers,(void *)cells)==EINVAL);
 CHECK(!remesa_context_error(ctx));
 shader.ir.nir=fixture(UINT32_C(0xffff0000));CHECK(!ctx->create_fs_state(ctx,&shader));
 CHECK(remesa_context_error(ctx)==ENOTSUP);
 CHECK(!remesa_context_revalidate_shader_contract(ctx,p,&r,transfers,&contract)&&remesa_context_error(ctx)==ENOTSUP);
 shader.ir.nir=fixture(UINT32_C(0xffff00ff));void *other=ctx->create_fs_state(ctx,&shader);CHECK(other);
 ctx->bind_fs_state(ctx,other);
 CHECK(remesa_context_revalidate_shader_contract(ctx,p,&r,transfers,&contract)==ESTALE);
 ctx->bind_fs_state(ctx,NULL);
 CHECK(remesa_context_prepare_shader_contract(ctx,p,&r,transfers,&contract)==ENODATA);
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_fs_state(ctx,other);
 ctx->destroy(ctx);screen->destroy(screen);
 CHECK(!remesa_shader_host_pipeline_validate_contract(p,transfers,&contract));
 remesa_shader_host_pipeline_destroy(p);
}
int main(void)
{
 contract_cases(UINT32_C(0xff00ff00));contract_cases(UINT32_C(0xffff00ff));context_cases();
 printf("PASS %u linked PDS/task/VS execution-contract checks; hardware blockers retained; no GPU\n",checks);
 return 0;
}
