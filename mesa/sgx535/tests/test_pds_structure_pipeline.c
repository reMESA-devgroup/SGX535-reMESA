/* SPDX-License-Identifier: MIT
 * Original restricted CPU PDS grammar integrated with owned NIR/VS/FS.
 * The independent DWORD fixtures are constructor observations, not an
 * executable program. Initial/final slots and physical execution stay blocked.
 */
int inherited_structure_host_fixture(int,char **);
#define main inherited_structure_host_fixture
#include "test_host_shader_pipeline.c"
#undef main
#include "remesa_device.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"

static struct remesa_pds_structure_entry structure_r(uint32_t id)
{
 struct remesa_pds_structure_entry e={0};
 e.kind=REMESA_PDS_STRUCTURE_R;e.id=id;e.components=4;e.mode=3;
 return e;
}
static struct remesa_pds_structure_entry structure_t(uint32_t id)
{
 struct remesa_pds_structure_entry e={0};
 e.kind=REMESA_PDS_STRUCTURE_T;e.id=id;
 e.state_words[0]=UINT32_C(0x001e0002);e.state_words[1]=UINT32_C(0x60000000);
 e.reloc_arg3=UINT32_C(0x100);e.reloc_arg4=UINT32_C(0x200);
 return e;
}
static struct remesa_pds_structure_request structure_request(unsigned mode,unsigned extra,
 uint32_t metadata)
{
 struct remesa_pds_structure_request q={0};
 q.extra_r=extra;q.task_word1=metadata<<27;
 if(mode==0) {q.entry_count=1;q.entries[0]=structure_t(4);}
 if(mode==1) {q.entry_count=2;q.entries[0]=structure_r(2);q.entries[1]=structure_t(4);}
 if(mode==2) {q.entry_count=3;q.entries[0]=structure_t(4);q.entries[1]=structure_r(2);q.entries[2]=structure_t(5);}
 if(mode==3) {q.entry_count=1;q.entries[0]=structure_r(2);}
 return q;
}
static struct remesa_shader_host_pipeline_request structure_layout(
 struct remesa_pds_data_cell cells[2][12],const struct remesa_pds_structure_request *q)
{
 struct remesa_shader_host_pipeline_request r=request(cells);
 struct remesa_pds_structure shape;
 CHECK(!remesa_pds_structure_encode(q,&shape));
 r.data_byte_offsets[1]=0;
 /* This FS recipe is explicitly ignored. A stale pointer must never be read. */
 r.data[1]=(struct remesa_pds_host_data_recipe){(void *)(UINTPTR_MAX-3),SIZE_MAX};
 r.pds[1].data_bytes=4u*shape.data_dwords;
 r.pds[1].code_bytes=4u*shape.code_slot_count;
 return r;
}
static void inspect_shape(const struct remesa_shader_host_pipeline_view *v,
 const struct remesa_pds_structure_request *q,unsigned mode)
{
 struct remesa_pds_structure shape;CHECK(!remesa_pds_structure_encode(q,&shape));
 CHECK(v->structure_error==0&&v->pds_code_error==ENOTSUP&&v->execution_error==ENOTSUP);
 CHECK(v->image.execution_error==ENOTSUP&&v->links->launch.semantic.binary_error==ENOTSUP);
 CHECK(v->structure_code.format==REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE);
 CHECK(v->structure_code_mask.format==REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE);
 CHECK(v->structure_data_mask.format==REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE);
 CHECK(v->structure_code.byte_length==4u*shape.code_slot_count);
 CHECK(v->structure_code_mask.byte_length==v->structure_code.byte_length);
 CHECK(v->structure_data_mask.byte_length==4u*shape.data_dwords);
 CHECK(v->data[1].storage.byte_length==4u*shape.data_dwords);
 CHECK(v->links->launch.stage[1].request.code.offset==4u*shape.data_dwords);
 const unsigned char *code=v->structure_code.bytes,*mask=v->structure_code_mask.bytes;
 const unsigned char *data=v->data[1].storage.bytes,*data_mask=v->structure_data_mask.bytes;
 CHECK(!get32(code)&&!get32(mask));
 CHECK(!get32(code+4u*(shape.code_slot_count-1u))&&!get32(mask+4u*(shape.code_slot_count-1u)));
 CHECK(!get32(data_mask)); /* resolved host fixup does not upgrade CPU evidence */
 CHECK(get32(data)==(shape.use_background|v->links->use[1].address_bits));
 CHECK(get32(data+4)==q->task_word1&&get32(data+32)==UINT32_C(0x20));
 for(unsigned i=0;i<shape.data_dwords;i++) {
  CHECK(get32(data_mask+4u*i)==shape.data_known_mask[i]);
  if(i&&shape.data_known_mask[i])CHECK(get32(data+4u*i)==shape.data[i]);
 }
 for(unsigned i=0;i<shape.code_slot_count;i++) {
  CHECK(get32(mask+4u*i)==shape.code[i].known_mask);
  if(shape.code[i].known_mask)CHECK(get32(code+4u*i)==shape.code[i].word);
 }
 /* Independent x86-constructor oracle: no local encoder/decoder is involved
  * in these comparisons. Holes are deliberately absent from the oracle. */
 if(mode<4) {
  const unsigned char a2_j1[4]={0xa2,0x18,0x04,0x07};
  CHECK(!memcmp(code+4,a2_j1,sizeof(a2_j1)));
 }
 if(mode<3)CHECK(get32(code+8)==UINT32_C(0x07042364));
 if((mode<2&&q->extra_r)||mode==2) {
  const unsigned char a2_j3[4]={0xa2,0x38,0x04,0x07};
  CHECK(!memcmp(code+12,a2_j3,sizeof(a2_j3)));
 }
 if(mode==0)CHECK(get32(data+36)==UINT32_C(0x0c00f904));
 /* ELF 0x26163 marks only the final R in its domain. Appending the extra
  * R moves bit25 from the merged RT at DATA9 to the new R at DATA11;
  * the independent final-T bit11 stays on the merged RT. */
 if(mode==1)CHECK(get32(data+36)==(q->extra_r?UINT32_C(0x0dc02904):UINT32_C(0x0fc02904)));
 if(mode==3)CHECK(get32(data+36)==UINT32_C(0x0fc0200f));
 if(mode<2&&q->extra_r)CHECK(get32(data+44)==UINT32_C(0x0f80e00f));
 for(unsigned i=1;i<shape.relocation_count;i++) {
  CHECK(shape.relocations[i].kind==REMESA_PDS_STRUCTURE_TEXTURE);
  unsigned cell=shape.relocations[i].data_cell;
  CHECK(!get32(data_mask+4u*cell)&&!get32(data+4u*cell));
  CHECK(shape.relocations[i].arg3==UINT32_C(0x100)&&shape.relocations[i].arg4==UINT32_C(0x200));
 }
}
static void structure_contract_aliases(const struct remesa_shader_host_pipeline *p,
 const struct remesa_shader_host_pipeline_view *v)
{
 struct remesa_pds_host_transfer cells[2][12];
 struct remesa_pds_host_transfer_request routes[2];
 for(unsigned s=0;s<2;s++) {
  for(unsigned i=0;i<12;i++)cells[s][i]=(struct remesa_pds_host_transfer){11u-i,52u+i};
  routes[s]=(struct remesa_pds_host_transfer_request){cells[s],12};
 }
 struct remesa_shader_execution_contract contract;
 CHECK(!remesa_shader_host_pipeline_prepare_contract(p,routes,&contract));
 CHECK(!remesa_shader_host_pipeline_validate_contract(p,routes,&contract));
 const struct remesa_shader_storage_view *stores[3]={&v->structure_code,
  &v->structure_code_mask,&v->structure_data_mask};
 for(unsigned i=0;i<3;i++) {
  const struct remesa_shader_storage_view *store=stores[i];
  unsigned char before[REMESA_PDS_STRUCTURE_IMAGE_BYTES];
  CHECK(store->byte_length<=sizeof(before));
  memcpy(before,store->bytes,store->byte_length);
  /* Public host APIs must reject these newly owned buffers just as they
   * reject the pre-existing DATA/USE storage, before reading or publishing
   * a contract into their much smaller, borrowed immutable byte ranges. */
  CHECK(remesa_shader_host_pipeline_prepare_contract(p,routes,(void *)store->bytes)==EINVAL);
  CHECK(!memcmp(before,store->bytes,store->byte_length));
  CHECK(remesa_shader_host_pipeline_validate_contract(p,routes,(void *)store->bytes)==EINVAL);
  size_t written=73;
  CHECK(remesa_shader_host_pipeline_encode_contract(p,routes,&contract,
   (void *)store->bytes,store->byte_length,&written)==EINVAL);
  CHECK(written==73&&!memcmp(before,store->bytes,store->byte_length));
 }
 CHECK(!remesa_shader_host_pipeline_validate(p));
}
static void structural_pipeline_case(uint32_t color,unsigned mode,unsigned extra,uint32_t metadata)
{
 struct remesa_shader_draw_programs pair=programs(color),saved_pair=pair;
 struct remesa_pds_structure_request q=structure_request(mode,extra,metadata),saved_q=q;
 struct remesa_pds_data_cell cells[2][12],saved_cells[2][12];
 struct remesa_shader_host_pipeline_request r=structure_layout(cells,&q),saved_r=r;
 memcpy(saved_cells,cells,sizeof(cells));
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(!remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&p)&&p);
 CHECK(!memcmp(&pair,&saved_pair,sizeof(pair))&&!memcmp(&q,&saved_q,sizeof(q)));
 CHECK(!memcmp(&r,&saved_r,sizeof(r))&&!memcmp(cells,saved_cells,sizeof(cells)));
 CHECK(!remesa_shader_host_pipeline_validate(p));
 CHECK(!remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&r,&q));
 struct remesa_shader_host_pipeline_view v;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&v));inspect_shape(&v,&q,mode);
 if(color==UINT32_C(0xff00ff00)&&mode==0&&!extra&&!metadata)structure_contract_aliases(p,&v);
 struct remesa_shader_host_pipeline *old=p;
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&p)==EBUSY&&p==old);
 struct remesa_pds_structure_request changed=q;changed.task_word1^=UINT32_C(0x08000000);
 CHECK(remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&r,&changed)==ESTALE);
 if(mode<4) {
  changed=q;changed.entries[0].id^=1;
  CHECK(remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&r,&changed)==ESTALE);
 }
 struct remesa_shader_host_pipeline_request stale=r;stale.use[1].mapping_generation++;
 CHECK(remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&stale,&q)==ESTALE);
 cells[0][0].value^=1;
 CHECK(remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&r,&q)==ESTALE);
 cells[0][0].value^=1;
 CHECK(!remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&r,&q));
 CHECK(remesa_shader_host_pipeline_inspect(p,(void *)v.structure_code.bytes)==EINVAL);
 CHECK(remesa_shader_host_pipeline_inspect(p,(void *)v.structure_code_mask.bytes)==EINVAL);
 CHECK(remesa_shader_host_pipeline_inspect(p,(void *)v.structure_data_mask.bytes)==EINVAL);
 if(export_directory) {
  char name[128];
  snprintf(name,sizeof(name),"structure-%08x-mode%u-extra%u-meta%u.data.bin",color,mode,extra,metadata);
  export_bytes(name,v.data[1].storage.bytes,v.data[1].storage.byte_length);
  snprintf(name,sizeof(name),"structure-%08x-mode%u-extra%u-meta%u.code.bin",color,mode,extra,metadata);
  export_bytes(name,v.structure_code.bytes,v.structure_code.byte_length);
  snprintf(name,sizeof(name),"structure-%08x-mode%u-extra%u-meta%u.code-mask.bin",color,mode,extra,metadata);
  export_bytes(name,v.structure_code_mask.bytes,v.structure_code_mask.byte_length);
  snprintf(name,sizeof(name),"structure-%08x-mode%u-extra%u-meta%u.data-mask.bin",color,mode,extra,metadata);
  export_bytes(name,v.structure_data_mask.bytes,v.structure_data_mask.byte_length);
 }
 memset(&pair,0,sizeof(pair));memset(&q,0,sizeof(q));memset(&r,0,sizeof(r));memset(cells,0,sizeof(cells));
 CHECK(!remesa_shader_host_pipeline_validate(p));
 CHECK(!remesa_shader_host_pipeline_inspect(p,&v));inspect_shape(&v,&saved_q,mode);
 remesa_shader_host_pipeline_destroy(p);
}
static void structure_rejections(void)
{
 struct remesa_shader_draw_programs pair=programs(UINT32_C(0xff00ff00));
 struct remesa_pds_structure_request q=structure_request(0,0,0),saved_q=q;
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_shader_host_pipeline_request r=structure_layout(cells,&q),saved_r=r;
 struct remesa_shader_host_pipeline *p=NULL;
 for(unsigned mode=0;mode<10;mode++) {
  q=saved_q;r=saved_r;
  if(mode==0)r.pds[1].data_bytes-=4;
  if(mode==1)r.pds[1].code_bytes-=4;
  if(mode==2)r.pds[1].code_bytes+=4;
  if(mode==3)r.data_byte_offsets[1]=4;
  if(mode==4)r.use[1].bytes=16;
  if(mode==5)r.banks[1]=r.banks[0];
  if(mode==6)q.entries[0].kind=99;
  if(mode==7)q.extra_r=2;
  if(mode==8)r.pds[1].allocation.bytes=48;
  if(mode==9)r.pds[1].code_alignment=3;
  CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&p)!=0&&!p);
 }
 q=saved_q;r=saved_r;
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,(void *)&q)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,(void *)&r)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,(void *)cells)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,NULL,&p)==EINVAL&&!p);
 /* The admitted yellow literal now uses the same original eight-byte form.
  * Preserve the old oversized-binding case as an exact-size failure. */
 pair=programs(UINT32_C(0xffffff00));
 CHECK(!remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&p)&&p);
 struct remesa_shader_host_pipeline_view yellow_view;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&yellow_view));
 CHECK(yellow_view.image.fragment.byte_length==8&&yellow_view.execution_error==ENOTSUP);
 remesa_shader_host_pipeline_destroy(p);p=NULL;
 r.use[1].bytes=REMESA_FRAGMENT_BODY_BYTES;
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&p)==EPROTO&&!p);
 pair=programs(UINT32_C(0xff00ff00));r=saved_r;
 bool success=false;
 for(unsigned f=1;f<=32;f++) {
  allocation=0;fail_at=f;tracking=true;
  int error=remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&p);tracking=false;
  if(!error) {CHECK(p);remesa_shader_host_pipeline_destroy(p);p=NULL;CHECK(!live);success=true;break;}
  CHECK(error==ENOMEM&&!p&&!live);
 }
 CHECK(success);
 /* Unknown physical initial/final classes remain impossible to encode, and
  * an ENOTSUP cannot publish any partial bytes or modify written. */
 struct remesa_pds_code_instruction ir={REMESA_PDS_STRUCTURE_INITIAL_HOLE,
  REMESA_PDS_STRUCTURE_NO_CELL,REMESA_PDS_STRUCTURE_NO_CELL};
 unsigned char bytes[8],before[8];memset(bytes,0xa5,sizeof(bytes));memcpy(before,bytes,sizeof(bytes));
 size_t written=73;
 CHECK(remesa_pds_structure_encode_code(&ir,1,bytes,sizeof(bytes),&written)==ENOTSUP);
 CHECK(written==73&&!memcmp(bytes,before,sizeof(bytes)));
 /* Existing callers retain the old host-only behavior. Attaching an
  * explicitly requested structural artifact must not turn it on by default. */
 r=request(cells);
 CHECK(!remesa_shader_host_pipeline_create(&pair,&r,&p));
 struct remesa_shader_host_pipeline_view v;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&v));
 CHECK(v.structure_error==ENOTSUP&&v.pds_code_error==ENOTSUP&&v.execution_error==ENOTSUP);
 CHECK(!v.structure_code.bytes&&!v.structure_code.byte_length);
 CHECK(!v.structure_code_mask.bytes&&!v.structure_code_mask.byte_length);
 CHECK(!v.structure_data_mask.bytes&&!v.structure_data_mask.byte_length);
 remesa_shader_host_pipeline_destroy(p);
}
static int structure_no_allocate(void *p,size_t n,void **out)
{(void)p;(void)n;(void)out;return ENOTSUP;}
static void structure_no_destroy(void *p,void *b){(void)p;(void)b;}
static int structure_no_map(void *p,void *b,bool w,void **out)
{(void)p;(void)b;(void)w;(void)out;return ENOTSUP;}
static void structure_no_unmap(void *p,void *b,bool w){(void)p;(void)b;(void)w;}
static int structure_no_submit(void *p,void *b,const struct remesa_draw_packet *d,void **f)
{(void)p;(void)b;(void)d;(void)f;assert(!"GPU submission forbidden in structural CPU test");return ENOTSUP;}
static int structure_no_wait(void *p,void *b,uint64_t t,bool v)
{(void)p;(void)b;(void)t;(void)v;return ENOTSUP;}
/* Allocator output enters the same owned Gallium path as existing shapes.
 * Liveness is an explicit post-optimizer CPU input, not guessed from NIR.
 * The texture entry here is structural evidence only; the bound FS remains
 * the supported constant-color body and execution must remain ENOTSUP. */
static void allocated_structure_gallium(struct pipe_context *ctx)
{
 struct remesa_pds_allocator_request a={0};
 a.source.entry_count=3;
 a.source.entries[0]=structure_r(2);a.source.entries[0].mode=0;
 a.source.entries[1]=structure_r(3);a.source.entries[1].mode=0;
 a.source.entries[2]=structure_t(4);
 a.logical_width[0]=4;a.logical_width[1]=4;a.logical_width[2]=1;
 a.live_components[0]=15;a.live_components[1]=15;a.live_components[2]=1;
 const struct remesa_pds_allocator_request original=a;
 struct remesa_pds_allocation plan;
 CHECK(!remesa_pds_structure_allocate(&a,&plan));
 CHECK(!memcmp(&a,&original,sizeof(a)));
 CHECK(plan.request.entry_count==3&&plan.entry_to_source[0]==0&&
       plan.entry_to_source[1]==2&&plan.entry_to_source[2]==1);
 CHECK(plan.logical_map[0]==0&&plan.logical_map[8]==4&&plan.logical_map[4]==5);
 CHECK(plan.structure.a2_count==2&&plan.structure.class64_count==1);
 CHECK(plan.structure.relocations[1].data_cell==10);
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_shader_host_pipeline_request r=structure_layout(cells,&plan.request);
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(!remesa_context_prepare_pds_structure_pipeline(ctx,&r,&plan.request,&p)&&p);
 CHECK(!remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&plan.request));
 struct remesa_shader_host_pipeline_view v;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&v));
 inspect_shape(&v,&plan.request,4);
 struct remesa_pds_structure_request stale=plan.request;stale.entries[2].id++;
 CHECK(remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&stale)==ESTALE);
 CHECK(!remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&plan.request));
 a.flags=1;
 const struct remesa_pds_allocation before=plan;
 CHECK(remesa_pds_structure_allocate(&a,&plan)==ENOTSUP);
 CHECK(!memcmp(&plan,&before,sizeof(plan)));
 memset(&plan,0,sizeof(plan));memset(cells,0,sizeof(cells));memset(&r,0,sizeof(r));
 CHECK(!remesa_shader_host_pipeline_validate(p));
 CHECK(!remesa_shader_host_pipeline_inspect(p,&v));
 CHECK(v.pds_code_error==ENOTSUP&&v.execution_error==ENOTSUP);
 remesa_shader_host_pipeline_destroy(p);
}
static void structure_gallium(void)
{
 const struct remesa_device_ops ops={.allocate=structure_no_allocate,.destroy_bo=structure_no_destroy,
  .map=structure_no_map,.unmap=structure_no_unmap,.submit=structure_no_submit,
  .wait=structure_no_wait,.destroy_fence=structure_no_destroy};
 struct remesa_device device={.ops=&ops,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct remesa_pds_structure_request q=structure_request(0,1,1);
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_shader_host_pipeline_request r=structure_layout(cells,&q);
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(remesa_context_prepare_pds_structure_pipeline(ctx,&r,&q,&p)==ENODATA&&!p);
 struct pipe_shader_state shader={.type=PIPE_SHADER_IR_NIR,.ir.nir=vertex_fixture()};
 void *vs=ctx->create_vs_state(ctx,&shader);CHECK(vs);ctx->bind_vs_state(ctx,vs);
 shader.ir.nir=fixture(UINT32_C(0xff00ff00));void *fs=ctx->create_fs_state(ctx,&shader);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 allocated_structure_gallium(ctx);
 CHECK(!remesa_context_prepare_pds_structure_pipeline(ctx,&r,&q,&p));
 CHECK(!remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&q));
 struct remesa_shader_host_pipeline_view v;CHECK(!remesa_shader_host_pipeline_inspect(p,&v));inspect_shape(&v,&q,0);
 struct remesa_pds_structure_request changed=q;changed.extra_r=0;
 /* A changed descriptor count requires a correspondingly valid layout;
  * retaining the old CODE extent is malformed input, not a stale binding. */
 CHECK(remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&changed)==EINVAL);
 struct remesa_pds_data_cell changed_cells[2][12];
 struct remesa_shader_host_pipeline_request changed_layout=structure_layout(changed_cells,&changed);
 CHECK(remesa_context_revalidate_pds_structure_pipeline(ctx,p,&changed_layout,&changed)==ESTALE);
 CHECK(remesa_context_prepare_pds_structure_pipeline(ctx,&r,&q,(void *)ctx)==EINVAL);
 CHECK(remesa_context_prepare_pds_structure_pipeline(ctx,&r,&q,(void *)screen)==EINVAL);
 CHECK(remesa_context_prepare_pds_structure_pipeline(ctx,&r,&q,(void *)fs)==EINVAL);
 CHECK(remesa_context_prepare_pds_structure_pipeline(ctx,&r,&q,(void *)&q)==EINVAL);
 shader.ir.nir=fixture(UINT32_C(0xffff0000));CHECK(!ctx->create_fs_state(ctx,&shader));
 CHECK(remesa_context_error(ctx)==ENOTSUP);
 CHECK(!remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&q)&&remesa_context_error(ctx)==ENOTSUP);
 shader.ir.nir=fixture(UINT32_C(0xffff00ff));void *other=ctx->create_fs_state(ctx,&shader);CHECK(other);
 ctx->bind_fs_state(ctx,other);
 CHECK(remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&q)==ESTALE);
 struct remesa_shader_host_pipeline *unchanged=p;
 CHECK(remesa_context_prepare_pds_structure_pipeline(ctx,&r,&q,&p)==EBUSY&&p==unchanged);
 ctx->bind_fs_state(ctx,NULL);
 CHECK(remesa_context_revalidate_pds_structure_pipeline(ctx,p,&r,&q)==ENODATA);
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_fs_state(ctx,other);
 ctx->destroy(ctx);screen->destroy(screen);
 CHECK(!remesa_shader_host_pipeline_validate(p));
 remesa_shader_host_pipeline_destroy(p);
}
/* Full CPU CODE, independent USE bodies and symbolic USE relocation; no
 * physical dispatch or VS export-tail qualification is implied by this test. */
static void complete_cpu_pipeline(uint32_t color,unsigned extra)
{
 struct remesa_shader_draw_programs pair=programs(color);
 struct remesa_pds_structure_request q={.extra_r=extra,
  .code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_shader_host_pipeline_request r=structure_layout(cells,&q);
 for(unsigned c=0;c<4;c++) {
  r.vertex_mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
  r.vertex_mapping.output[c]=(struct remesa_usse_cpu_register){1,c};
 }
 r.use[0].bytes=16;
 struct remesa_shader_host_pipeline *p=NULL,*again=NULL;
 CHECK(!remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&p)&&p);
 CHECK(!remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&r,&q));
 struct remesa_shader_host_pipeline_view v;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&v));
 CHECK(v.image.vertex.byte_length==16&&v.image.fragment.byte_length==8);
 CHECK(v.structure_code.byte_length==8u+4u*extra);
 CHECK(v.structure->code[v.structure->code_slot_count-1].kind==REMESA_PDS_STRUCTURE_CPU_TRAILER_AF);
 for(size_t i=0;i<v.structure_code_mask.byte_length;i++)
  CHECK(((const unsigned char *)v.structure_code_mask.bytes)[i]==255);
 CHECK(v.pds_code_error==ENOTSUP&&v.execution_error==ENOTSUP&&
       v.image.execution_error==ENOTSUP);
 CHECK(get32(v.data[1].storage.bytes)==
       (v.structure->use_background|v.links->use[1].address_bits));
 /* Symbolic/address/padding evidence is distinct from initialized host bytes. */
 CHECK(get32(v.structure_data_mask.bytes)==0);
 /* Original Xpsb primary relocation plus GPL kernel whole-word fixup closes
  * the FS T+8 CPU word. It neither selects a physical PC nor proves preload. */
 struct remesa_pds_record_preview records[2];
 CHECK(!remesa_pds_preview_record_pair(v.links->launch.stage,r.use,v.links->use,records));
 CHECK(records[1].block_address==UINT32_C(0x22000000));
 CHECK(records[1].prefix_bytes==48&&records[1].code_window_address==UINT32_C(0x22000030));
 CHECK(records[1].code_bytes==v.structure_code.byte_length);
 CHECK(records[1].address_prefix_bits==UINT32_C(0x0c200000));
 CHECK(records[1].address_prefix_known_mask==UINT32_MAX);
 CHECK(records[0].address_prefix_known_mask==UINT32_C(0xfcffffff));
 CHECK(records[1].use_address_bits==v.links->use[1].address_bits);
 CHECK(!remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&q,&again)&&again);
 struct remesa_shader_host_pipeline_view duplicate;
 CHECK(!remesa_shader_host_pipeline_inspect(again,&duplicate));
 CHECK(!memcmp(v.structure_code.bytes,duplicate.structure_code.bytes,v.structure_code.byte_length));
 CHECK(!memcmp(v.image.vertex.bytes,duplicate.image.vertex.bytes,16));
 CHECK(!memcmp(v.image.fragment.bytes,duplicate.image.fragment.bytes,8));
 remesa_shader_host_pipeline_destroy(again);
 struct remesa_pds_structure_request changed=q;changed.code_policy=0;
 CHECK(remesa_shader_host_pipeline_revalidate_pds_structure(p,&pair,&r,&changed)==ESTALE);
 changed=q;changed.code_policy=2;again=NULL;
 CHECK(remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&changed,&again)==ENOTSUP&&!again);
 if(export_directory) {
  char name[96];
  snprintf(name,sizeof(name),"cpu-code-%08x-r%u.bin",color,extra);
  export_bytes(name,v.structure_code.bytes,v.structure_code.byte_length);
  snprintf(name,sizeof(name),"vs-cpu-%08x-r%u.bin",color,extra);
  export_bytes(name,v.image.vertex.bytes,16);
  snprintf(name,sizeof(name),"fs-%08x-r%u.bin",color,extra);
  export_bytes(name,v.image.fragment.bytes,8);
  snprintf(name,sizeof(name),"data-%08x-r%u.bin",color,extra);
  export_bytes(name,v.data[1].storage.bytes,v.data[1].storage.byte_length);
  snprintf(name,sizeof(name),"data-mask-%08x-r%u.bin",color,extra);
  export_bytes(name,v.structure_data_mask.bytes,v.structure_data_mask.byte_length);
  unsigned char record_bytes[4],record_mask[4];
  for(unsigned k=0;k<4;k++) {
   record_bytes[k]=(unsigned char)(records[1].address_prefix_bits>>(8u*k));
   record_mask[k]=(unsigned char)(records[1].address_prefix_known_mask>>(8u*k));
  }
  snprintf(name,sizeof(name),"fs-primary-t8-%08x-r%u.bin",color,extra);
  export_bytes(name,record_bytes,sizeof(record_bytes));
  snprintf(name,sizeof(name),"fs-primary-t8-mask-%08x-r%u.bin",color,extra);
  export_bytes(name,record_mask,sizeof(record_mask));
 }
 remesa_shader_host_pipeline_destroy(p);
}
int main(int argc,char **argv)
{
 if(argc!=1) {assert(argc==3&&!strcmp(argv[1],"--export"));export_directory=argv[2];}
 for(unsigned color=0;color<2;color++)for(unsigned extra=0;extra<2;extra++) {
  structural_pipeline_case(color?UINT32_C(0xffff00ff):UINT32_C(0xff00ff00),0,extra,color);
  structural_pipeline_case(color?UINT32_C(0xffff00ff):UINT32_C(0xff00ff00),1,extra,color);
 }
 structural_pipeline_case(UINT32_C(0xff00ff00),2,0,0);
 structural_pipeline_case(UINT32_C(0xff00ff00),3,0,0);
 structural_pipeline_case(UINT32_C(0xff00ff00),4,0,0);
 structure_rejections();structure_gallium();
 const uint32_t complete_colors[]={UINT32_C(0xff00ff00),UINT32_C(0xffff00ff),UINT32_C(0xffffff00)};
 for(unsigned c=0;c<3;c++)for(unsigned extra=0;extra<2;extra++)
  complete_cpu_pipeline(complete_colors[c],extra);
 printf("PASS %u original structural CODE/DATA / NIR / Gallium checks; CPU CODE opt-in complete; physical GPU ENOTSUP\n",checks);
 return 0;
}
