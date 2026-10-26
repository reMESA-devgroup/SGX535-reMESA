/* SPDX-License-Identifier: MIT; syscall fixtures are never hardware evidence. */
#include "remesa_linux.h"
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "remesa/shader_request.h"
#include "remesa/native_uapi.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#ifdef REMESA_WITH_NIR
#include "nir_fixture.h"
#include "vertex_nir_fixture.h"
#include "remesa_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#endif

static unsigned checks,calls,opens,closes,allocation,fail_at,live;
static bool tracking;
static void *owned[128];
#define CHECK(x) do { assert(x);checks++; } while(0)
void *__real_calloc(size_t,size_t);
void *__real_malloc(size_t);
void __real_free(void *);
void *__wrap_calloc(size_t,size_t);
void *__wrap_malloc(size_t);
void __wrap_free(void *);
static void *track(void *p)
{
 if(p)for(unsigned i=0;i<128;i++)if(!owned[i]){owned[i]=p;live++;return p;}
 assert(!p);return p;
}
void *__wrap_calloc(size_t n,size_t bytes)
{if(!tracking)return __real_calloc(n,bytes);if(++allocation==fail_at)return NULL;return track(__real_calloc(n,bytes));}
void *__wrap_malloc(size_t bytes)
{if(!tracking)return __real_malloc(bytes);if(++allocation==fail_at)return NULL;return track(__real_malloc(bytes));}
void __wrap_free(void *p)
{
 if(!p)return;
 for(unsigned i=0;i<128;i++)if(owned[i]==p){owned[i]=NULL;live--;__real_free(p);return;}
 /* Static i386 libc may release startup allocations that preceded tracking. */
 __real_free(p);
}
int open(const char *,int,...);
int open64(const char *,int,...);
int close(int);
int ioctl(int,unsigned long,...);
int open(const char *path,int flags,...)
{(void)path;(void)flags;opens++;return 123;}
/* Actual Mesa uses _FILE_OFFSET_BITS=64, including on i386. */
int open64(const char *path,int flags,...)
{return open(path,flags);}
int close(int fd){assert(fd==123);closes++;return 0;}
int ioctl(int fd,unsigned long op,...)
{
 va_list ap;assert(fd==123);va_start(ap,op);void *arg=va_arg(ap,void *);va_end(ap);
 calls++;assert(op==DRM_IOCTL_VERSION);
 struct drm_version *v=arg;memcpy(v->name,"gma500",6);v->name_len=6;return 0;
}
static struct remesa_shader_draw_programs programs(uint32_t color)
{
 struct remesa_shader_ir ir={.schema=1,.stage=REMESA_IR_VERTEX,.count=2};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,
  .components=4,.type=REMESA_IR_FLOAT32};
 ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,
  .components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 struct remesa_shader_program vs,fs;struct remesa_shader_draw_programs p;
 CHECK(!remesa_shader_program_prepare(&ir,&vs));
 CHECK(!remesa_shader_program_prepare_color(color,&fs));
 CHECK(!remesa_shader_draw_programs_prepare(&vs,&fs,&p));return p;
}
#ifdef REMESA_WITH_NIR
static void gallium_native_candidate(uint32_t color,bool with_vertex)
{
 struct remesa_device device={0};struct remesa_shader_request request;
 CHECK(!remesa_linux_open(&device,"INTERPOSED-NO-DEVICE"));
 unsigned before_calls=calls;
 struct remesa_usse_vertex_mapping mapping={0};
 if(with_vertex) {
  for(unsigned c=0;c<4;c++) {
   mapping.input[c]=(struct remesa_usse_cpu_register){0,8+c};
   mapping.output[c]=(struct remesa_usse_cpu_register){2,24+c};
  }
  struct remesa_usse_vertex_mapping bad=mapping;bad.output[1]=mapping.input[0];
  CHECK(remesa_linux_set_vertex_candidate_mapping(&device,&bad)==EINVAL&&calls==before_calls);
  CHECK(!remesa_linux_set_vertex_candidate_mapping(&device,&mapping)&&calls==before_calls);
  memset(&mapping,0,sizeof(mapping)); /* Endpoint owns the assignment. */
 }
 struct pipe_screen *screen=remesa_screen_create(&device);CHECK(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);CHECK(ctx);
 struct pipe_resource templ={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,
  .width0=48,.height0=1,.depth0=1,.array_size=1,.bind=PIPE_BIND_VERTEX_BUFFER};
 struct pipe_resource *vb=screen->resource_create(screen,&templ);CHECK(vb);
 templ=(struct pipe_resource){.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *target=screen->resource_create(screen,&templ);CHECK(target);
 const float vertices[3][4]={{-.5,-.5,0,1},{.5,-.5,0,1},{-.5,.5,0,1}};
 struct pipe_box box={.width=48,.height=1,.depth=1};struct pipe_transfer *transfer=NULL;
 void *bytes=ctx->buffer_map(ctx,vb,0,PIPE_MAP_WRITE,&box,&transfer);CHECK(bytes&&transfer);
 memcpy(bytes,vertices,sizeof(vertices));ctx->buffer_unmap(ctx,transfer);
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};
 fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;ctx->set_framebuffer_state(ctx,&fb);
 struct pipe_vertex_buffer binding={.buffer.resource=vb};vb->reference.count++;
 ctx->set_vertex_buffers(ctx,1,&binding);
 struct pipe_vertex_element element={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};
 void *ve=ctx->create_vertex_elements_state(ctx,1,&element);CHECK(ve);ctx->bind_vertex_elements_state(ctx,ve);
 struct pipe_shader_state shader={.type=PIPE_SHADER_IR_NIR,.ir.nir=vertex_fixture()};
 void *vs=ctx->create_vs_state(ctx,&shader);CHECK(vs);ctx->bind_vs_state(ctx,vs);
 shader.ir.nir=fixture(color);void *fs=ctx->create_fs_state(ctx,&shader);CHECK(fs);ctx->bind_fs_state(ctx,fs);
 struct pipe_blend_state blend={0};blend.rt[0].colormask=PIPE_MASK_RGBA;
 struct pipe_depth_stencil_alpha_state depth={0};
 struct pipe_rasterizer_state raster={.fill_front=PIPE_POLYGON_MODE_FILL,.fill_back=PIPE_POLYGON_MODE_FILL};
 void *bs=ctx->create_blend_state(ctx,&blend),*ds=ctx->create_depth_stencil_alpha_state(ctx,&depth),
  *rs=ctx->create_rasterizer_state(ctx,&raster);CHECK(bs&&ds&&rs);
 ctx->bind_blend_state(ctx,bs);ctx->bind_depth_stencil_alpha_state(ctx,ds);ctx->bind_rasterizer_state(ctx,rs);
 struct pipe_viewport_state viewport={.scale={16,16,.5},.translate={16,16,.5}};
 ctx->set_viewport_states(ctx,0,1,&viewport);CHECK(!remesa_context_error(ctx));
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
 struct pipe_draw_start_count_bias draw={.count=3};ctx->draw_vbo(ctx,&info,0,NULL,&draw,1);
 CHECK(remesa_context_error(ctx)==ENOTSUP&&calls==before_calls);
 CHECK(!remesa_linux_last_shader_request(&device,&request));
 unsigned fragment_offset=with_vertex?32:0;
 CHECK(request.resource_mask==(with_vertex?3u:2u)&&
  request.byte_count==fragment_offset+REMESA_FRAGMENT_CONSTANT_BYTES&&request.generation==1);
 if(with_vertex) {
  for(unsigned c=0;c<4;c++) {
   struct remesa_usse_cpu_move move;
   CHECK(!remesa_usse_cpu_move_decode(request.data+c*8,8,&move)&&
    move.source.class_id==0&&move.source.number==8+c&&
    move.destination.class_id==2&&move.destination.number==24+c&&!move.control11);
  }
  CHECK(remesa_linux_set_vertex_candidate_mapping(&device,&mapping)==EBUSY&&calls==before_calls);
 }
 struct remesa_shader_draw_programs semantic=programs(color);struct remesa_fragment_constant expected;
 CHECK(!remesa_fragment_constant_prepare(&semantic.fragment.usse,&expected));
 CHECK(!memcmp(request.data+fragment_offset,expected.record,REMESA_FRAGMENT_CONSTANT_BYTES));
 struct pipe_fence_handle *fence=NULL;ctx->flush(ctx,&fence,0);CHECK(!fence);
 struct remesa_linux_receipt receipt;CHECK(!remesa_linux_last_receipt(&device,&receipt)&&!receipt.attempted);
 CHECK(!remesa_context_cpu_clear_count(ctx)&&!remesa_context_cpu_copy_count(ctx));
 ctx->delete_vs_state(ctx,vs);ctx->delete_fs_state(ctx,fs);ctx->delete_vertex_elements_state(ctx,ve);
 ctx->delete_blend_state(ctx,bs);ctx->delete_depth_stencil_alpha_state(ctx,ds);ctx->delete_rasterizer_state(ctx,rs);
 ctx->destroy(ctx);
 if(!--vb->reference.count)screen->resource_destroy(screen,vb);
 if(!--target->reference.count)screen->resource_destroy(screen,target);
 screen->destroy(screen);
 /* Candidate request is a copied snapshot, independent of context/shaders. */
 CHECK(!remesa_linux_last_shader_request(&device,&request)&&
  !memcmp(request.data+fragment_offset,expected.record,REMESA_FRAGMENT_CONSTANT_BYTES));
 remesa_linux_close(&device);CHECK(!live);
}
#endif
static void vertex_candidate(void)
{
 unsigned char bytes[32]={0},saved[32];size_t written;
 const unsigned boundary[]={0,127};
 for(unsigned sc=0;sc<3;sc++)for(unsigned dc=0;dc<3;dc++)
  for(unsigned s=0;s<2;s++)for(unsigned d=0;d<2;d++)
   for(unsigned flag=0;flag<2;flag++)for(unsigned flag18=0;flag18<2;flag18++) {
   struct remesa_usse_cpu_move move={.source={sc,boundary[s]},
    .destination={dc,boundary[d]},.control11=flag,.control18=flag18},decoded;
   written=99;CHECK(!remesa_usse_cpu_move_encode(&move,bytes,8,&written)&&written==8);
   CHECK(!remesa_usse_cpu_move_decode(bytes,8,&decoded));
   CHECK(decoded.source.class_id==sc&&decoded.source.number==boundary[s]&&
    decoded.destination.class_id==dc&&decoded.destination.number==boundary[d]&&
    decoded.control11==flag&&decoded.control18==flag18&&
    !decoded.control12&&!decoded.control21&&!decoded.control23);
   /* Independent relative controls in the second little-endian CPU word. */
   CHECK(((bytes[5]>>3)&1)==flag&&((bytes[6]>>2)&1)==flag18);
   unsigned char roundtrip[8];size_t count=99;
   CHECK(!remesa_usse_cpu_move_encode(&decoded,roundtrip,sizeof(roundtrip),&count)&&
    count==8&&!memcmp(bytes,roundtrip,8));
  }
 struct remesa_usse_cpu_move move={.source={0,4},.destination={2,87}},decoded;
 CHECK(!remesa_usse_cpu_move_encode(&move,bytes,8,&written));
 memcpy(saved,bytes,sizeof(bytes));
 /* Exercise the additional numeric CPU controls without embedding a retained
  * program. Independent byte deltas must leave register fields untouched.
  * They do not establish repetition, physical outputs or task termination. */
 for(unsigned field=0;field<16;field++)for(unsigned flags=0;flags<4;flags++) {
  move.control12=field;move.control21=flags&1;move.control23=flags>>1;
  CHECK(!remesa_usse_cpu_move_encode(&move,bytes,8,&written));
  unsigned char expected[8];memcpy(expected,saved,8);
  expected[5]|=field<<4;expected[6]|=((flags&1)<<5)|((flags>>1)<<7);
  CHECK(!memcmp(bytes,expected,8));
  CHECK(!remesa_usse_cpu_move_decode(bytes,8,&decoded)&&
   decoded.source.class_id==move.source.class_id&&decoded.source.number==move.source.number&&
   decoded.destination.class_id==move.destination.class_id&&decoded.destination.number==move.destination.number&&
   decoded.control12==field&&decoded.control21==(flags&1)&&decoded.control23==(flags>>1)&&
   !decoded.control11&&!decoded.control18);
  unsigned char again[8];size_t count=99;
  CHECK(!remesa_usse_cpu_move_encode(&decoded,again,8,&count)&&count==8&&!memcmp(bytes,again,8));
 }
 move.control12=move.control21=move.control23=0;
 memcpy(bytes,saved,sizeof(bytes));
 for(unsigned field=0;field<3;field++) {
  if(field==0)move.control12=16;
  if(field==1)move.control21=2;
  if(field==2)move.control23=2;
  written=99;
  CHECK(remesa_usse_cpu_move_encode(&move,bytes,8,&written)==ENOTSUP&&
   written==99&&!memcmp(bytes,saved,sizeof(bytes)));
  move.control12=move.control21=move.control23=0;
 }
 move.source.number=128;written=99;
 CHECK(remesa_usse_cpu_move_encode(&move,bytes,8,&written)==ENOTSUP&&written==99&&!memcmp(bytes,saved,32));
 move.source.number=4;move.destination.class_id=3;
 CHECK(remesa_usse_cpu_move_encode(&move,bytes,8,&written)==ENOTSUP);
 move.destination.class_id=2;move.control11=2;
 CHECK(remesa_usse_cpu_move_encode(&move,bytes,8,&written)==ENOTSUP);
 move.control11=0;
 move.control18=2;written=99;
 CHECK(remesa_usse_cpu_move_encode(&move,bytes,8,&written)==ENOTSUP&&
  written==99&&!memcmp(bytes,saved,32));
 move.control18=0;
 CHECK(remesa_usse_cpu_move_encode(&move,bytes,7,&written)==ENOSPC);
 CHECK(remesa_usse_cpu_move_encode(&move,bytes,32,(void *)(bytes+8))==EINVAL);
 CHECK(remesa_usse_cpu_move_encode(&move,(void *)&move,8,&written)==EINVAL);
 CHECK(remesa_usse_cpu_move_encode(&move,bytes,8,(void *)&move)==EINVAL);
 CHECK(remesa_usse_cpu_move_decode(bytes,7,&decoded)==ENOTSUP);
 CHECK(remesa_usse_cpu_move_decode(bytes,8,(void *)bytes)==EINVAL);
 /* Selected CPU format rejects unsupported fields rather than masking them. */
 bytes[0]|=1;CHECK(remesa_usse_cpu_move_decode(bytes,8,&decoded)==ENOTSUP);
 memcpy(bytes,saved,8);bytes[6]|=64;
 CHECK(remesa_usse_cpu_move_decode(bytes,8,&decoded)==ENOTSUP);
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned c=0;c<4;c++) {
  mapping.input[c]=(struct remesa_usse_cpu_register){0,16+c};
  mapping.output[c]=(struct remesa_usse_cpu_register){2,80+c};
 }
 CHECK(!remesa_usse_vertex_candidate(&pair.vertex.usse,&mapping,bytes,sizeof(bytes),&written)&&written==32);
 memcpy(saved,bytes,32);
 for(unsigned c=0;c<4;c++) {
  CHECK(!remesa_usse_cpu_move_decode(bytes+c*8,8,&decoded));
  CHECK(decoded.source.class_id==0&&decoded.source.number==16+c&&
   decoded.destination.class_id==2&&decoded.destination.number==80+c&&
   !decoded.control11&&!decoded.control18&&!decoded.control12&&
   !decoded.control21&&!decoded.control23);
 }
 struct remesa_usse_vertex_mapping bad=mapping;bad.output[3]=mapping.input[0];written=99;
 CHECK(remesa_usse_vertex_candidate(&pair.vertex.usse,&bad,bytes,32,&written)==EINVAL&&
  written==99&&!memcmp(bytes,saved,32));
 bad=mapping;bad.output[2]=bad.output[1];
 CHECK(remesa_usse_vertex_candidate(&pair.vertex.usse,&bad,bytes,32,&written)==EINVAL);
 bad=mapping;bad.input[2]=bad.input[1];
 CHECK(remesa_usse_vertex_candidate(&pair.vertex.usse,&bad,bytes,32,&written)==EINVAL);
 bad=mapping;bad.input[0].class_id=3;
 CHECK(remesa_usse_vertex_candidate(&pair.vertex.usse,&bad,bytes,32,&written)==ENOTSUP);
 CHECK(remesa_usse_vertex_candidate(&pair.fragment.usse,&mapping,bytes,32,&written)==ENOTSUP);
 CHECK(remesa_usse_vertex_candidate(&pair.vertex.usse,&mapping,bytes,31,&written)==ENOSPC);
 struct remesa_usse_program alias=pair.vertex.usse;
 memmove(alias.instructions+5,alias.instructions+4,4*sizeof(alias.instructions[0]));
 alias.instructions[4]=(struct remesa_usse_instruction){.op=REMESA_USSE_MOVE,.dst=4,.src=0};
 alias.instructions[5].src=4;alias.count=9;
 CHECK(!remesa_usse_vertex_candidate(&alias,&mapping,bytes,32,&written)&&!memcmp(bytes,saved,32));
 struct remesa_shader_image *image=NULL;struct remesa_shader_image_view view;
 for(unsigned n=1;n<=5;n++) {
  allocation=0;fail_at=n;
  CHECK(remesa_shader_image_prepare_vertex_candidate(&pair,&mapping,&image)==ENOMEM&&!image&&!live);
 }
 fail_at=0;CHECK(!remesa_shader_image_prepare_vertex_candidate(&pair,&mapping,&image));
 CHECK(!remesa_shader_image_inspect(image,&view)&&view.vertex_byte_length==32&&
  view.vertex.stage==REMESA_IR_VERTEX&&view.vertex.status==REMESA_SHADER_STORAGE_CANDIDATE);
 CHECK(!memcmp(view.vertex.bytes,saved,32));
 CHECK(remesa_shader_image_inspect(image,(void *)view.vertex.bytes)==EINVAL);
 CHECK(remesa_shader_image_inspect(image,(void *)view.programs)==EINVAL);
 struct remesa_shader_request request,before;
 CHECK(!remesa_linux_shader_request_build(image,9,10,&request));before=request;
 CHECK(!remesa_shader_image_validate_request(image,&request));
 request.data[32]^=1;CHECK(remesa_shader_image_validate_request(image,&request)==EPROTO);
 request=before;request.data[0]^=1;CHECK(remesa_shader_image_validate_request(image,&request)==EPROTO);
 /* A newly decodable CPU control does not widen the immutable NIR image or
  * authorize a different VS. Rejection precedes any execution interface. */
 const unsigned byte_offset[3]={5,6,6},bit_mask[3]={16,32,128};
 for(unsigned i=0;i<3;i++) {
  request=before;request.data[byte_offset[i]]|=bit_mask[i];
  CHECK(!remesa_usse_cpu_move_decode(request.data,8,&decoded));
  CHECK(remesa_shader_image_validate_request(image,&request)==EPROTO);
 }
 request=before;request.resources[1].offset=0;
 CHECK(remesa_shader_image_validate_request(image,&request)==EPROTO);request=before;
 CHECK(request.resource_mask==3&&request.byte_count==40&&request.resources[0].offset==0&&
  request.resources[0].length==32&&request.resources[1].offset==32&&request.resources[1].length==8);
 CHECK(!memcmp(request.data,saved,32)&&!memcmp(request.data+32,view.fragment.bytes,8));
 CHECK(remesa_linux_shader_request_build(image,9,10,(void *)view.vertex.bytes)==EINVAL);
 CHECK(remesa_shader_image_validate_execution(image)==ENOTSUP);
 struct remesa_use_entry_request entry_request={.allocation_address=0x10000000,
  .allocation_bytes=4096,.program_bytes=32,.bank=3,.data_master=REMESA_USE_VERTEX};
 struct remesa_use_entry_plan entry,entry_before;
 CHECK(!remesa_shader_image_preview_use_entry(image,&entry_request,&entry));entry_before=entry;
 CHECK(entry.entry_address==0x10000000&&entry.program_end==0x10000020);
 entry_request.program_bytes=8;
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,&entry)==EPROTO&&
  !memcmp(&entry,&entry_before,sizeof(entry)));
 entry_request.program_bytes=32;entry_request.data_master=REMESA_USE_EDM;
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,&entry)==ENOTSUP);
 entry_request.data_master=REMESA_USE_VERTEX;
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,(void *)view.vertex.bytes)==EINVAL);
 memset(&pair,0,sizeof(pair));memset(&mapping,0,sizeof(mapping));
 CHECK(!remesa_shader_image_validate(image));
 CHECK(!remesa_linux_shader_request_build(image,9,10,&request)&&!memcmp(&request,&before,sizeof(request)));
 remesa_shader_image_destroy(image);CHECK(!live);
}
int main(void)
{
 tracking=true;
 vertex_candidate();
 struct remesa_shader_draw_programs p=programs(0xff00ff00),original=p;
 struct remesa_shader_image *image=NULL;
 struct remesa_shader_image_view view;
 struct remesa_shader_request request,before;
 for(unsigned n=1;n<=3;n++) {
  allocation=0;fail_at=n;
  CHECK(remesa_shader_image_prepare(&p,&image)==ENOMEM&&!image&&!live);
 }
 fail_at=0;
 CHECK(!remesa_shader_image_prepare(&p,&image));
 CHECK(remesa_shader_image_prepare(&p,&image)==EBUSY);
 CHECK(!remesa_shader_image_inspect(image,&view));
 CHECK(view.programs!=&p&&view.fragment.byte_length==REMESA_FRAGMENT_CONSTANT_BYTES&&view.vertex_byte_length==0&&
  view.fragment.byte_alignment==8&&view.execution_error==ENOTSUP&&
  view.fragment_form==REMESA_SHADER_PACKED_CONSTANT);
 struct remesa_use_entry_request missing_vs={.allocation_address=0x10000000,
  .allocation_bytes=4096,.program_bytes=32,.bank=3,.data_master=REMESA_USE_VERTEX};
 struct remesa_use_entry_plan missing_entry;
 CHECK(remesa_shader_image_preview_use_entry(image,&missing_vs,&missing_entry)==ENOTSUP);
 CHECK(!memcmp(view.programs,&p,sizeof(p)));
 CHECK(!remesa_shader_image_revalidate(image,&p));
 CHECK(remesa_shader_image_validate_execution(image)==ENOTSUP);
 struct remesa_use_entry_request entry_request={
  .allocation_address=0x10000000,.allocation_bytes=4096,.entry_offset=0x140,
  .program_bytes=REMESA_FRAGMENT_CONSTANT_BYTES,.bank=3,.data_master=REMESA_USE_PIXEL
 };
 struct remesa_use_entry_plan entry,entry_before;
 memset(&entry,0xa5,sizeof(entry));entry_before=entry;
 CHECK(!remesa_shader_image_preview_use_entry(image,&entry_request,&entry));
 CHECK(entry.entry_address==0x10000140&&entry.program_end==0x10000148&&
       entry.entry_byte_offset==0x140&&entry.register_word==0x2200000);
 CHECK(remesa_shader_image_validate_execution(image)==ENOTSUP);
 entry_before=entry;
 /* The owned image boundary must reject loss here, not leave it for a
  * later task-word serializer. Eight-byte host storage alignment alone is
  * insufficient. Every failure preserves the last exact entry. */
 for(unsigned low=1;low<16;low++) {
  entry_request.entry_offset=0x140+low;
  CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,&entry)==EINVAL);
  CHECK(!memcmp(&entry,&entry_before,sizeof(entry)));
 }
 /* Alignment constrains the selected address, not the allocation origin. */
 entry_request.allocation_address=0x10000001;entry_request.entry_offset=0x13f;
 CHECK(!remesa_shader_image_preview_use_entry(image,&entry_request,&entry));
 CHECK(!memcmp(&entry,&entry_before,sizeof(entry)));
 entry_request.allocation_address=0x10000000;entry_request.entry_offset=0x140;
 entry_request.program_bytes=2;
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,&entry)==EPROTO);
 CHECK(!memcmp(&entry,&entry_before,sizeof(entry)));
 entry_request.program_bytes=REMESA_FRAGMENT_CONSTANT_BYTES;entry_request.data_master=REMESA_USE_VERTEX;
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,&entry)==ENOTSUP);
 CHECK(!memcmp(&entry,&entry_before,sizeof(entry)));
 entry_request.data_master=REMESA_USE_PIXEL;entry_request.entry_offset=4090;
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,&entry)==ERANGE);
 CHECK(!memcmp(&entry,&entry_before,sizeof(entry)));
 entry_request.entry_offset=0x140;
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,(void *)view.fragment.bytes)==EINVAL);
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,(void *)view.programs)==EINVAL);
 CHECK(remesa_shader_image_preview_use_entry(image,NULL,&entry)==EINVAL);
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,NULL)==EINVAL);
 CHECK(remesa_shader_image_preview_use_entry(image,&entry_request,
       (void *)(UINTPTR_MAX-7))==EINVAL);
 CHECK(remesa_shader_image_inspect(image,(void *)view.fragment.bytes)==EINVAL);
 CHECK(remesa_linux_shader_request_build(image,17,9,(void *)view.programs)==EINVAL);
 CHECK(remesa_linux_shader_request_build(image,17,9,(void *)view.fragment.bytes)==EINVAL);
 CHECK(!remesa_shader_image_validate(image));
 memset(&p,0,sizeof(p));
 CHECK(!remesa_shader_image_validate(image));
 CHECK(!remesa_linux_shader_request_build(image,17,9,&request));before=request;
 CHECK(request.action==REMESA_SHADER_STAGE&&request.resource_mask==2&&request.byte_count==REMESA_FRAGMENT_CONSTANT_BYTES);
 CHECK(!memcmp(request.data,view.fragment.bytes,REMESA_FRAGMENT_CONSTANT_BYTES));
 CHECK(request.resources[1].state==REMESA_SHADER_CPU_CANDIDATE&&request.resources[1].kind==REMESA_SHADER_FRAGMENT_USSE);
 CHECK(remesa_shader_request_validate(&request,17,9)==REMESA_SHADER_REQUEST_OK);
 CHECK(remesa_linux_shader_request_build(image,0,9,&request)==EINVAL&&!memcmp(&request,&before,sizeof(request)));
 request.action=REMESA_SHADER_EXECUTE;
 CHECK(remesa_shader_request_validate(&request,17,9)==REMESA_SHADER_REQUEST_UNSUPPORTED);
 p=programs(0xffff00ff);
 CHECK(remesa_shader_image_revalidate(image,&p)==ESTALE);
 remesa_shader_image_destroy(image);image=NULL;CHECK(!live);
 CHECK(!memcmp(before.data,request.data,REMESA_FRAGMENT_CONSTANT_BYTES));

 p=programs(0xffffff00);
 CHECK(!remesa_shader_image_prepare(&p,&image));
 CHECK(!remesa_shader_image_inspect(image,&view)&&view.fragment.byte_length==REMESA_FRAGMENT_BODY_BYTES&&
  view.fragment_form==REMESA_SHADER_FLOAT_PACK_BODY&&view.execution_error==ENOTSUP);
 remesa_shader_image_destroy(image);image=NULL;
 p=programs(0xffff00ff);

 struct remesa_device d={0},other={0};void *token=NULL,*token2=NULL;
 CHECK(!remesa_linux_open(&d,"INTERPOSED-NO-DEVICE"));
 CHECK(d.ops->prepare_shader_programs&&d.ops->submit_shader_programs&&d.ops->destroy_shader_programs);
 CHECK(remesa_linux_last_shader_request(&d,&request)==EAGAIN);
 unsigned base_live=live,base_calls=calls;
 for(unsigned n=1;n<=6;n++) {
  allocation=0;fail_at=n;
  CHECK(d.ops->prepare_shader_programs(d.priv,&original,&token)==ENOMEM&&!token&&live==base_live&&calls==base_calls);
  CHECK(remesa_linux_last_shader_request(&d,&request)==EAGAIN);
 }
 fail_at=0;
 CHECK(d.ops->prepare_shader_programs(d.priv,&original,&token)==ENOTSUP&&token);
 CHECK(!remesa_linux_last_shader_request(&d,&request));
 CHECK(request.owner&&(request.generation==1)&&request.resource_mask==2&&request.byte_count==REMESA_FRAGMENT_CONSTANT_BYTES);
 CHECK(!memcmp(request.data,before.data,REMESA_FRAGMENT_CONSTANT_BYTES));
 CHECK(remesa_shader_request_validate(&request,request.owner,request.generation)==REMESA_SHADER_REQUEST_OK);
 memset(&original,0,sizeof(original));
 before=request;CHECK(calls==base_calls);
 struct remesa_shader_bindings binding={0},saved_binding=binding;
 CHECK(remesa_linux_shader_bindings(&d,token,&request,&binding)==ENOTSUP);
 CHECK(remesa_linux_shader_verify(&d,token)==ENOTSUP&&calls==base_calls);
 CHECK(!memcmp(&binding,&saved_binding,sizeof(binding))&&!memcmp(&request,&before,sizeof(request)));
 /* No target/input memory is read and no legacy ioctl can be invoked. */
 void *fence=(void *)1;
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence&&calls==base_calls);
 struct remesa_linux_receipt receipt;
 CHECK(!remesa_linux_last_receipt(&d,&receipt)&&!receipt.attempted);
 CHECK(!remesa_linux_open(&other,"INTERPOSED-NO-DEVICE"));
 CHECK(other.ops->submit_shader_programs(other.priv,(void *)1,(void *)1,(void *)1,token,&fence)==EINVAL&&!fence);
 CHECK(remesa_linux_shader_bindings(&other,token,&request,&binding)==EINVAL);
 other.ops->destroy_shader_programs(other.priv,token);
 CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP);
 d.ops->destroy_shader_programs(d.priv,token);token=NULL;
 CHECK(remesa_linux_shader_verify(&d,token)==EINVAL&&calls==base_calls+1);
 CHECK(live==base_live+2); /* Other endpoint plus retired identity cookie. */
 CHECK(!remesa_linux_last_shader_request(&d,&request)&&!memcmp(&before,&request,sizeof(request)));
 CHECK(d.ops->prepare_shader_programs(d.priv,&p,&token2)==ENOTSUP&&token2);
 CHECK(!remesa_linux_last_shader_request(&d,&request)&&request.generation==2&&memcmp(before.data,request.data,REMESA_FRAGMENT_CONSTANT_BYTES));
 struct remesa_usse_vertex_mapping map={0};void *vertex_token=NULL;
 for(unsigned c=0;c<4;c++) {
  map.input[c]=(struct remesa_usse_cpu_register){0,4+c};
  map.output[c]=(struct remesa_usse_cpu_register){2,16+c};
 }
 unsigned vertex_live=live,vertex_calls=calls;
 for(unsigned n=1;n<=8;n++) {
  allocation=0;fail_at=n;
  CHECK(remesa_linux_prepare_vertex_candidate(&d,&p,&map,&vertex_token)==ENOMEM&&
   !vertex_token&&live==vertex_live&&calls==vertex_calls);
  CHECK(!remesa_linux_last_shader_request(&d,&request)&&request.generation==2);
 }
 fail_at=0;
 CHECK(remesa_linux_prepare_vertex_candidate(&d,&p,&map,&vertex_token)==ENOTSUP&&vertex_token);
 CHECK(!remesa_linux_last_shader_request(&d,&request)&&request.generation==3&&request.resource_mask==3);
 CHECK(remesa_linux_prepare_vertex_candidate(&d,&p,&map,&vertex_token)==EBUSY);
 CHECK(remesa_linux_prepare_vertex_candidate(&d,&p,&map,(void *)token2)==EINVAL);
 CHECK(remesa_linux_prepare_vertex_candidate(&d,&p,&map,(void *)&p)==EINVAL);
 CHECK(remesa_linux_prepare_vertex_candidate(&d,&p,&map,(void *)&map)==EINVAL);
 d.ops->destroy_shader_programs(d.priv,vertex_token);
 CHECK(live==vertex_live+1&&calls==vertex_calls); /* Retained identity, freed payload. */
 remesa_linux_close(&other);remesa_linux_close(&d);
 CHECK(!live&&opens==2&&closes==2&&calls==base_calls+1);
#ifdef REMESA_WITH_NIR
 gallium_native_candidate(0xff00ff00,false);gallium_native_candidate(0xffff00ff,false);
 gallium_native_candidate(0xff00ff00,true);
#endif
 printf("PASS: %u native original-shader ownership/staging checks; no draw ioctl or GPU\n",checks);
 tracking=false;
 return 0;
}
