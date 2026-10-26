/* SPDX-License-Identifier: MIT
 * Original CPU pixel task group through immutable storage, NIR and Gallium.
 * No executable PDS fixture, physical DS assignment or device submission.
 */
int inherited_pixel_pipeline_fixture(int,char **);
#define main inherited_pixel_pipeline_fixture
#include "test_host_shader_pipeline.c"
#undef main
#include "remesa_device.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"

static struct remesa_shader_host_pipeline_request pixel_request(struct remesa_pds_data_cell cells[2][12])
{
 struct remesa_shader_host_pipeline_request r=request(cells);
 r.data_byte_offsets[1]=0;
 /* Explicitly ignored by this constructor: do not read a stale or absent
  * caller FS recipe. Only the VS recipe and layout remain caller inputs. */
 r.data[1]=(struct remesa_pds_host_data_recipe){(void *)(UINTPTR_MAX-3),SIZE_MAX};
 return r;
}
static void routes(struct remesa_pds_host_transfer cells[2][12],
 struct remesa_pds_host_transfer_request out[2])
{
 for(unsigned s=0;s<2;s++) {
  for(unsigned n=0;n<12;n++)cells[s][n]=(struct remesa_pds_host_transfer){11-n,52+n};
  out[s]=(struct remesa_pds_host_transfer_request){cells[s],12};
 }
}
static void pipeline_cases(uint32_t color,uint32_t metadata)
{
 struct remesa_shader_draw_programs pair=programs(color),saved_pair=pair;
 struct remesa_pds_data_cell cells[2][12],saved_cells[2][12];
 struct remesa_shader_host_pipeline_request r=pixel_request(cells),saved_r=r;
 memcpy(saved_cells,cells,sizeof(cells));
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(!remesa_shader_host_pipeline_create_pixel_task(&pair,&r,metadata,&p));
 CHECK(!memcmp(&pair,&saved_pair,sizeof(pair))&&!memcmp(&r,&saved_r,sizeof(r))&&
       !memcmp(cells,saved_cells,sizeof(cells)));
 CHECK(!remesa_shader_host_pipeline_revalidate_pixel_task(p,&pair,&r,metadata));
 CHECK(remesa_shader_host_pipeline_revalidate_pixel_task(p,&pair,&r,metadata^1)==ESTALE);
 struct remesa_shader_host_pipeline_view view;CHECK(!remesa_shader_host_pipeline_inspect(p,&view));
 CHECK(view.data[1].storage.byte_length==48&&view.image.fragment.byte_length==8&&view.image.vertex.byte_length==32);
 const unsigned char *data=view.data[1].storage.bytes;
 for(unsigned c=0;c<12;c++) {
  uint32_t expected=c==0?(UINT32_C(0x100000)|view.links->use[1].address_bits):
   c==1?(metadata<<27):c==8?UINT32_C(0x20):0;
  CHECK(get32(data+4*c)==expected);
 }
 struct remesa_pds_host_transfer x[2][12];struct remesa_pds_host_transfer_request transfers[2];routes(x,transfers);
 struct remesa_pds_cpu_pixel_task_output out,saved;
 CHECK(!remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,&out));saved=out;
 CHECK(!remesa_shader_host_pipeline_validate_pixel_task_output(p,transfers,&out));
 CHECK(out.cpu_word_count==3&&out.schema==REMESA_PDS_CPU_PIXEL_TASK_SCHEMA&&!out.reserved);
 const unsigned source[3]={0,1,8};
 for(unsigned n=0;n<3;n++) {
  CHECK(out.task.words[n]==get32(data+4*source[n]));
  CHECK(out.sources[n].data_cell==source[n]&&out.sources[n].logical_slot==63-source[n]);
  CHECK(out.sources[n].data_address==view.links->use[1].data_address+4*source[n]);
 }
 CHECK(out.data_to_ds_error==ENOTSUP&&out.dispatch_error==ENOTSUP&&out.termination_error==ENOTSUP);
 if(export_directory) {
  char name[96];
  snprintf(name,sizeof(name),"pixel-%08x-meta%u.host-data.bin",color,metadata);
  export_bytes(name,data,view.data[1].storage.byte_length);
  if(color==UINT32_C(0xff00ff00)&&!metadata) {
   export_bytes("vertex.cpu-candidate.bin",view.image.vertex.bytes,view.image.vertex.byte_length);
   export_bytes("fragment.cpu-candidate.bin",view.image.fragment.bytes,view.image.fragment.byte_length);
  }
 }
 struct remesa_shader_execution_contract contract;
 CHECK(!remesa_shader_host_pipeline_prepare_contract(p,transfers,&contract)&&contract.blockers==127);
 unsigned char code[32],before[32];memset(code,0xa5,sizeof(code));memcpy(before,code,sizeof(code));size_t written=91;
 CHECK(remesa_shader_host_pipeline_encode_contract(p,transfers,&contract,code,sizeof(code),&written)==ENOTSUP);
 CHECK(written==91&&!memcmp(code,before,sizeof(code)));
 for(unsigned mode=0;mode<22;mode++) {
  out=saved;
  if(mode==0)out.schema++;
  else if(mode==1)out.cpu_word_count++;
  else if(mode==2)out.reserved=1;
  else if(mode==3)out.data_to_ds_error=0;
  else if(mode==4)out.dispatch_error=0;
  else if(mode==5)out.termination_error=0;
  else if(mode<9)out.task.words[mode-6]^=1;
  else {
   unsigned index=(mode-9)%3,field=(mode-9)/3;
   if(field==0)out.sources[index].data_cell++;
   else if(field==1)out.sources[index].logical_slot++;
   else if(field==2)out.sources[index].value++;
   else out.sources[index].data_address++;
  }
  CHECK(remesa_shader_host_pipeline_validate_pixel_task_output(p,transfers,&out)==EPROTO);
 }
 out=saved;
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,(void *)view.links)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,(void *)view.data[1].storage.bytes)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,(void *)view.image.programs)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,(void *)transfers)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,(void *)x)==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,NULL)==EINVAL);
 x[1][0].logical_slot=x[1][1].logical_slot;
 CHECK(remesa_shader_host_pipeline_prepare_pixel_task_output(p,transfers,&out)==EINVAL);
 CHECK(!memcmp(&out,&saved,sizeof(out)));routes(x,transfers);
 r.use[1].mapping_generation++;
 CHECK(remesa_shader_host_pipeline_revalidate_pixel_task(p,&pair,&r,metadata)==ESTALE);r=saved_r;
 memset(cells,0,sizeof(cells));memset(&pair,0,sizeof(pair));memset(&r,0,sizeof(r));
 CHECK(!remesa_shader_host_pipeline_validate_pixel_task_output(p,transfers,&out));
 remesa_shader_host_pipeline_destroy(p);
}
static void rejected_inputs(void)
{
 struct remesa_shader_draw_programs pair=programs(UINT32_C(0xff00ff00));
 struct remesa_pds_data_cell cells[2][12];struct remesa_shader_host_pipeline_request r=pixel_request(cells),saved=r;
 for(unsigned mode=0;mode<8;mode++) {
  r=saved;uint32_t metadata=0;
  if(mode==0)r.pds[1].data_bytes=44;
  if(mode==1)r.pds[1].data_bytes=52;
  if(mode==2)r.data_byte_offsets[1]=4;
  if(mode==3)metadata=2;
  if(mode==4)r.use[1].bytes=16;
  if(mode==5)r.banks[1]=r.banks[0];
  if(mode==6)r.data[0].cells=NULL;
  if(mode==7)r.pds[1].allocation.address=UINT64_MAX;
  struct remesa_shader_host_pipeline *p=NULL;
  CHECK(remesa_shader_host_pipeline_create_pixel_task(&pair,&r,metadata,&p)!=0&&!p);
 }
 r=saved;
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(remesa_shader_host_pipeline_create_pixel_task(&pair,&r,0,(void *)&r)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pixel_task(&pair,&r,0,(void *)cells)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pixel_task(&pair,&r,0,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pixel_task(&pair,(void *)(UINTPTR_MAX-3),0,&p)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pixel_task((void *)(UINTPTR_MAX-3),&r,0,&p)==EINVAL);
 /* The admitted yellow literal now uses the same original eight-byte form.
  * Preserve the old oversized-binding case as an exact-size failure. */
 pair=programs(UINT32_C(0xffffff00));
 CHECK(!remesa_shader_host_pipeline_create_pixel_task(&pair,&r,0,&p)&&p);
 struct remesa_shader_host_pipeline_view yellow_view;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&yellow_view));
 CHECK(yellow_view.image.fragment.byte_length==8&&yellow_view.execution_error==ENOTSUP);
 remesa_shader_host_pipeline_destroy(p);p=NULL;
 r.use[1].bytes=REMESA_FRAGMENT_BODY_BYTES;
 CHECK(remesa_shader_host_pipeline_create_pixel_task(&pair,&r,0,&p)==EPROTO&&!p);
 pair=programs(UINT32_C(0xff00ff00));r=saved;
 bool reached_success=false;
 for(unsigned f=1;f<=20;f++) {
  allocation=0;fail_at=f;tracking=true;
  int error=remesa_shader_host_pipeline_create_pixel_task(&pair,&r,0,&p);tracking=false;
  if(!error) {CHECK(p);remesa_shader_host_pipeline_destroy(p);p=NULL;CHECK(!live);reached_success=true;break;}
  CHECK(error==ENOMEM&&!p&&!live);
 }
 CHECK(reached_success);
}
static int no_allocate(void *p,size_t n,void **out){(void)p;(void)n;(void)out;return ENOTSUP;}
static void no_destroy(void *p,void *b){(void)p;(void)b;}
static int no_map(void *p,void *b,bool w,void **out){(void)p;(void)b;(void)w;(void)out;return ENOTSUP;}
static void no_unmap(void *p,void *b,bool w){(void)p;(void)b;(void)w;}
static int no_submit(void *p,void *b,const struct remesa_draw_packet *d,void **f)
{(void)p;(void)b;(void)d;(void)f;assert(!"device submission forbidden");return ENOTSUP;}
static int no_wait(void *p,void *b,uint64_t t,bool v){(void)p;(void)b;(void)t;(void)v;return ENOTSUP;}
static void gallium(void)
{
 const struct remesa_device_ops ops={.allocate=no_allocate,.destroy_bo=no_destroy,.map=no_map,
  .unmap=no_unmap,.submit=no_submit,.wait=no_wait,.destroy_fence=no_destroy};
 struct remesa_device device={.ops=&ops,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct remesa_pds_data_cell cells[2][12];struct remesa_shader_host_pipeline_request r=pixel_request(cells);
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(remesa_context_prepare_pixel_task_pipeline(ctx,&r,0,&p)==ENODATA&&!p);
 struct pipe_shader_state shader={.type=PIPE_SHADER_IR_NIR,.ir.nir=vertex_fixture()};
 void *vs=ctx->create_vs_state(ctx,&shader);CHECK(vs);ctx->bind_vs_state(ctx,vs);
 shader.ir.nir=fixture(UINT32_C(0xff00ff00));void *fs=ctx->create_fs_state(ctx,&shader);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 CHECK(!remesa_context_prepare_pixel_task_pipeline(ctx,&r,1,&p));
 struct remesa_pds_host_transfer x[2][12];struct remesa_pds_host_transfer_request t[2];routes(x,t);
 struct remesa_pds_cpu_pixel_task_output out,saved;
 CHECK(!remesa_context_prepare_pixel_task_output(ctx,p,&r,1,t,&out));saved=out;
 CHECK(!remesa_context_revalidate_pixel_task_output(ctx,p,&r,1,t,&out));
 CHECK(remesa_context_revalidate_pixel_task_output(ctx,p,&r,0,t,&out)==ESTALE);
 CHECK(remesa_context_prepare_pixel_task_output(ctx,p,&r,1,t,(void *)ctx)==EINVAL);
 CHECK(remesa_context_prepare_pixel_task_output(ctx,p,&r,1,t,(void *)screen)==EINVAL);
 CHECK(remesa_context_prepare_pixel_task_output(ctx,p,&r,1,t,(void *)fs)==EINVAL);
 CHECK(remesa_context_prepare_pixel_task_output(ctx,p,&r,1,t,(void *)&r)==EINVAL);
 CHECK(remesa_context_prepare_pixel_task_output(ctx,p,&r,1,t,(void *)cells)==EINVAL);
 shader.ir.nir=fixture(UINT32_C(0xffff0000));CHECK(!ctx->create_fs_state(ctx,&shader));
 CHECK(remesa_context_error(ctx)==ENOTSUP);
 CHECK(!remesa_context_revalidate_pixel_task_output(ctx,p,&r,1,t,&out)&&remesa_context_error(ctx)==ENOTSUP);
 shader.ir.nir=fixture(UINT32_C(0xffff00ff));void *other=ctx->create_fs_state(ctx,&shader);CHECK(other);
 ctx->bind_fs_state(ctx,other);
 CHECK(remesa_context_prepare_pixel_task_output(ctx,p,&r,1,t,&out)==ESTALE);
 CHECK(!memcmp(&out,&saved,sizeof(out)));
 ctx->bind_fs_state(ctx,NULL);CHECK(remesa_context_revalidate_pixel_task_output(ctx,p,&r,1,t,&out)==ENODATA);
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_fs_state(ctx,other);
 ctx->destroy(ctx);screen->destroy(screen);
 CHECK(!remesa_shader_host_pipeline_validate_pixel_task_output(p,t,&out));
 remesa_shader_host_pipeline_destroy(p);
}
int main(int argc,char **argv)
{
 if(argc!=1) {assert(argc==3&&!strcmp(argv[1],"--export"));export_directory=argv[2];}
 for(unsigned m=0;m<2;m++) {pipeline_cases(UINT32_C(0xff00ff00),m);pipeline_cases(UINT32_C(0xffff00ff),m);}
 rejected_inputs();gallium();
 printf("PASS %u automatic CPU pixel task / NIR / Gallium checks; physical CODE ENOTSUP; no GPU\n",checks);
 return 0;
}
