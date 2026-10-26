/* SPDX-License-Identifier: MIT */
/* Explicit laboratory caller, evolving the original retained triangle caller.
 * Generates NIR itself; external qualified VS/PDS carrier is data, never a
 * source for the new FS. No automatic gate enabling, retry, recovery or reboot.
 */
#include "retained_fragment_capture.h"
#include "remesa_screen.h"
#include "remesa_usse.h"
#include "nir.h"
#include "util/ralloc.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef REMESA_GLX_LAB_FRONTEND
#include "glx_lab_triangle.h"
#endif

static nir_shader *shader(bool vertex)
{
 /* Same small, original lowered-IO NIR representation already exercised by
  * the driver tests. No vendor instruction bytes or shader input are read. */
 nir_shader *s=rzalloc(NULL,nir_shader);if(!s)return NULL;
 exec_list_make_empty(&s->variables);exec_list_make_empty(&s->functions);
 s->info.stage=vertex?MESA_SHADER_VERTEX:MESA_SHADER_FRAGMENT;s->info.io_lowered=true;
 nir_function *fn=rzalloc(s,nir_function);
 nir_function_impl *impl=rzalloc(s,nir_function_impl);
 nir_block *end=rzalloc(s,nir_block),*b=rzalloc(s,nir_block);
 nir_load_const_instr *zero=rzalloc_size(s,sizeof(*zero)+sizeof(nir_const_value));
 nir_intrinsic_instr *store=rzalloc_size(s,sizeof(*store)+2*sizeof(nir_src));
 if(!fn||!impl||!end||!b||!zero||!store)goto failed;
 fn->shader=s;fn->is_entrypoint=true;exec_list_push_tail(&s->functions,&fn->node);
 fn->impl=impl;impl->function=fn;impl->structured=true;impl->end_block=end;
 exec_list_make_empty(&impl->body);exec_list_make_empty(&impl->locals);exec_list_make_empty(&end->instr_list);
 b->cf_node.type=nir_cf_node_block;b->cf_node.parent=&impl->cf_node;
 exec_list_push_tail(&impl->body,&b->cf_node.node);exec_list_make_empty(&b->instr_list);
 zero->instr.type=nir_instr_type_load_const;zero->instr.block=b;zero->def.parent_instr=&zero->instr;
 zero->def.bit_size=32;zero->def.num_components=1;exec_list_push_tail(&b->instr_list,&zero->instr.node);
 nir_def *value;
 if(vertex) {
  nir_intrinsic_instr *input=rzalloc_size(s,sizeof(*input)+sizeof(nir_src));if(!input)goto failed;
  input->instr.type=nir_instr_type_intrinsic;input->instr.block=b;input->intrinsic=nir_intrinsic_load_input;
  input->num_components=4;input->src[0].ssa=&zero->def;input->def.parent_instr=&input->instr;
  input->def.bit_size=32;input->def.num_components=4;
  nir_intrinsic_set_dest_type(input,nir_type_float32);
  nir_intrinsic_set_io_semantics(input,(nir_io_semantics){.location=VERT_ATTRIB_GENERIC0,.num_slots=1});
  exec_list_push_tail(&b->instr_list,&input->instr.node);value=&input->def;
 } else {
  nir_load_const_instr *color=rzalloc_size(s,sizeof(*color)+4*sizeof(nir_const_value));if(!color)goto failed;
  color->instr.type=nir_instr_type_load_const;color->instr.block=b;color->def.parent_instr=&color->instr;
  color->def.bit_size=32;color->def.num_components=4;color->value[1].u32=color->value[3].u32=0x3f800000;
  exec_list_push_tail(&b->instr_list,&color->instr.node);value=&color->def;
 }
 store->instr.type=nir_instr_type_intrinsic;store->instr.block=b;store->intrinsic=nir_intrinsic_store_output;
 store->num_components=4;store->src[0].ssa=value;store->src[1].ssa=&zero->def;
 nir_intrinsic_set_write_mask(store,15);nir_intrinsic_set_src_type(store,nir_type_float32);
 nir_intrinsic_set_io_semantics(store,(nir_io_semantics){.location=vertex?VARYING_SLOT_POS:FRAG_RESULT_DATA0,.num_slots=1});
 exec_list_push_tail(&b->instr_list,&store->instr.node);return s;
failed:
 ralloc_free(s);return NULL;
}
static void drop(struct pipe_resource *r)
{if(r&&!__atomic_sub_fetch(&r->reference.count,1,__ATOMIC_ACQ_REL))r->screen->resource_destroy(r->screen,r);}
static struct pipe_resource *buffer(struct pipe_screen *s,unsigned n,unsigned bind)
{struct pipe_resource t={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.width0=n,.height0=1,.depth0=1,.array_size=1,.bind=bind};return s->resource_create(s,&t);}
static int upload(struct pipe_context *c,struct pipe_resource *r,const void *bytes,unsigned n)
{
 struct pipe_box box={.width=(int)n,.height=1,.depth=1};struct pipe_transfer *t=NULL;
 void *p=c->buffer_map(c,r,0,PIPE_MAP_WRITE,&box,&t);if(!p)return EIO;
 memcpy(p,bytes,n);c->buffer_unmap(c,t);return 0;
}
static int draw(struct remesa_device *device,const char *directory)
{
 int e=ENOMEM;struct pipe_screen *s=remesa_screen_create(device);if(!s)return e;
 struct pipe_context *c=s->context_create(s,NULL,0);if(!c){s->destroy(s);return e;}
 struct pipe_resource *vb=buffer(s,48,PIPE_BIND_VERTEX_BUFFER),*ib=buffer(s,6,PIPE_BIND_INDEX_BUFFER),*target=NULL;
 void *vs=NULL,*fs=NULL,*ve=NULL,*bs=NULL,*ds=NULL,*rs=NULL;
 struct pipe_fence_handle *f=NULL;
 if(!vb||!ib)goto done;
 const float pos[3][4]={{-.5f,-.5f,0,1},{.5f,-.5f,0,1},{-.5f,.5f,0,1}};
 const uint16_t indices[3]={0,1,2};
 e=upload(c,vb,pos,sizeof(pos));if(e)goto done;
 e=upload(c,ib,indices,sizeof(indices));if(e)goto done;
 struct pipe_resource rt={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 target=s->resource_create(s,&rt);if(!target){e=ENOMEM;goto done;}
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;c->set_framebuffer_state(c,&fb);
 struct pipe_vertex_buffer binding={.buffer.resource=vb};++vb->reference.count;c->set_vertex_buffers(c,1,&binding);
 struct pipe_vertex_element element={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};
 ve=c->create_vertex_elements_state(c,1,&element);if(!ve){e=ENOMEM;goto done;}c->bind_vertex_elements_state(c,ve);
 struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR,.ir.nir=shader(true)};
 if(!state.ir.nir){e=ENOMEM;goto done;}
 vs=c->create_vs_state(c,&state);if(!vs){e=ENOTSUP;goto done;}c->bind_vs_state(c,vs);
 state.ir.nir=shader(false);if(!state.ir.nir){e=ENOMEM;goto done;}
 fs=c->create_fs_state(c,&state);if(!fs){e=ENOTSUP;goto done;}c->bind_fs_state(c,fs);
 struct pipe_blend_state blend={0};blend.rt[0].colormask=PIPE_MASK_RGBA;
 struct pipe_depth_stencil_alpha_state depth={0};struct pipe_rasterizer_state raster={.fill_front=PIPE_POLYGON_MODE_FILL,.fill_back=PIPE_POLYGON_MODE_FILL};
 bs=c->create_blend_state(c,&blend);ds=c->create_depth_stencil_alpha_state(c,&depth);rs=c->create_rasterizer_state(c,&raster);
 if(!bs||!ds||!rs){e=ENOMEM;goto done;}
 c->bind_blend_state(c,bs);c->bind_depth_stencil_alpha_state(c,ds);c->bind_rasterizer_state(c,rs);
 struct pipe_viewport_state viewport={.scale={16,16,.5f},.translate={16,16,.5f}};c->set_viewport_states(c,0,1,&viewport);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.index_size=2,.instance_count=1};info.index.resource=ib;
 struct pipe_draw_start_count_bias start={.count=3};c->draw_vbo(c,&info,0,NULL,&start,1);
 e=remesa_context_error(c);if(e)goto done;
 c->flush(c,&f,0);if(!f||!s->fence_finish(s,c,f,0)){e=EIO;goto done;}
 struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *t=NULL;
 const unsigned char *p=c->texture_map(c,target,0,PIPE_MAP_READ,&box,&t);
 if(!p){e=EIO;goto done;}
 unsigned char pixels[4096];
 for(unsigned y=0;y<32;y++)memcpy(pixels+y*128,p+y*t->stride,128);
 c->texture_unmap(c,t);e=remesa_fragment_save(directory,"gallium-readback.bin",pixels,sizeof(pixels));
done:
 if(f)s->fence_reference(s,&f,NULL);
 c->bind_fs_state(c,NULL);if(fs)c->delete_fs_state(c,fs);
 c->bind_vs_state(c,NULL);if(vs)c->delete_vs_state(c,vs);
 c->bind_vertex_elements_state(c,NULL);if(ve)c->delete_vertex_elements_state(c,ve);
 c->bind_blend_state(c,NULL);if(bs)c->delete_blend_state(c,bs);
 c->bind_depth_stencil_alpha_state(c,NULL);if(ds)c->delete_depth_stencil_alpha_state(c,ds);
 c->bind_rasterizer_state(c,NULL);if(rs)c->delete_rasterizer_state(c,rs);
 c->destroy(c);drop(vb);drop(ib);drop(target);s->destroy(s);return e;
}
int main(int argc,char **argv)
{
 bool glx_mode=false;
#ifdef REMESA_GLX_LAB_FRONTEND
 glx_mode=argc==7&&!strcmp(argv[1],"--execute-qualified-glx-triangle-once");
#endif
 if(argc!=7||(!glx_mode&&strcmp(argv[1],"--execute-qualified-green-once")&&strcmp(argv[1],"--execute-qualified-green-vertex-once"))) {
  fprintf(stderr,"Usage: %s --execute-qualified-green-once DEVICE QUALIFIED_CARRIER NEW_EVIDENCE_DIRECTORY PROTECTED_BINDING AUTHENTICATOR\nSeparate exact authorization and qualified module required; does not enable any gate.\n",argv[0]);return 2;
 }
 int e=0,fd=-1;struct remesa_device device={0};struct remesa_linux_memory *memory=NULL;
 struct remesa_retained_request job={0};unsigned char *images[6]={0};unsigned mapped=0,pinned=0,allocated=0;
 struct remesa_linux_fragment_receipt receipt={0};
 struct remesa_fragment_recorder capture={.directory=argv[4],.binding=argv[5],.authenticator=argv[6],.memory_fd=-1,.retained_fd=-1};
 e=remesa_fragment_load(argv[3],"requesttemplate.bin",&job,sizeof(job));if(e)goto done;
 /* Template contains layout/commands only: IDs/context are assigned by the
  * current endpoint. Never inherit old operation/generation or observer data. */
 if(job.version!=1||job.flags||job.context){e=EINVAL;goto done;}
 for(unsigned i=0;i<10;i++) {
  const struct remesa_retained_binding *b=&job.bindings[i];
  if(b->id||b->reserved||!b->bytes||b->bytes%4096||b->bytes>32u*1024u*1024u||
     !b->va||b->va%4096||b->va>=UINT64_C(0x100000000)||b->bytes>UINT64_C(0x100000000)-b->va){e=EINVAL;goto done;}
  for(unsigned j=0;j<i;j++)if(b->va<job.bindings[j].va+job.bindings[j].bytes&&job.bindings[j].va<b->va+b->bytes){e=EINVAL;goto done;}
  if(i<6) {images[i]=malloc((size_t)b->bytes);if(!images[i]){e=ENOMEM;goto done;}
   char name[32];snprintf(name,sizeof(name),"role%u.bin",i);e=remesa_fragment_load(argv[3],name,images[i],(size_t)b->bytes);if(e)goto done;}
 }
 if(job.bindings[5].bytes!=4096){e=EINVAL;goto done;}
 for(unsigned i=0;i<4096;i++)if(images[5][i]){e=EINVAL;goto done;}
 unsigned char tail[sizeof(job)-520];memcpy(tail,(unsigned char *)&job+520,sizeof(tail));
 for(unsigned i=0;i<sizeof(tail);i++)if(tail[i]){e=EINVAL;goto done;}
 if(mkdir(argv[4],0700)){e=errno;goto done;}
 e=remesa_fragment_copy("/proc/sgx535_source_guard",argv[4],"source-before.raw");if(e)goto done;
 e=remesa_fragment_copy("/proc/sys/kernel/random/boot_id",argv[4],"bootid.txt");if(e)goto done;
 /* Preserve actual inputs before the helper checks the external immutable
  * artifact bindings. Incomplete protection fails before device open. */
 e=remesa_fragment_save(argv[4],"requesttemplate.bin",&job,sizeof(job));if(e)goto done;
 for(unsigned i=0;i<6;i++){char name[32];snprintf(name,sizeof(name),"role%u.bin",i);e=remesa_fragment_save(argv[4],name,images[i],(size_t)job.bindings[i].bytes);if(e)goto done;}
 unsigned char reference[4096];e=remesa_fragment_load(argv[3],"expectedcolor.bin",reference,sizeof(reference));if(e)goto done;
 e=remesa_fragment_save(argv[4],"reference.bin",reference,sizeof(reference));if(e)goto done;
 e=remesa_fragment_auth_preflight(&capture);if(e)goto done;
 char file[4096];
 int n=snprintf(file,sizeof(file),"%s/memory.records",argv[4]);if(n<0||n>=4096){e=ENAMETOOLONG;goto done;}
 capture.memory_fd=open(file,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(capture.memory_fd<0){e=errno;goto done;}
 n=snprintf(file,sizeof(file),"%s/retained.records",argv[4]);if(n<0||n>=4096){e=ENAMETOOLONG;goto done;}
 capture.retained_fd=open(file,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(capture.retained_fd<0){e=errno;goto done;}
 fd=open(argv[2],O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(fd<0){e=errno;goto done;}
 e=remesa_linux_open_fd(&device,fd);if(e)goto done;
 const struct remesa_memory_recorder recorder={remesa_fragment_record_memory,&capture};
 e=remesa_linux_memory_begin_recorded(fd,&memory,&recorder);if(e)goto done;
 const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();
 struct remesa_memory_info empty;
 e=remesa_linux_memory_info(memory,0,&empty);if(e)goto done;
 if(empty.objects||empty.allocated_bytes||empty.pinned_objects||empty.mapped_pages||empty.leases||empty.held_jobs||empty.quarantined){e=ESTALE;goto done;}
 for(unsigned i=0;i<10;i++) {
  uint32_t id=0;e=ops->create(memory,(size_t)job.bindings[i].bytes,&id);if(e)goto done;
  job.bindings[i].id=id;allocated++;
  e=ops->pin(memory,id);if(e)goto done;pinned++;
  e=ops->gpu_map(memory,id,job.bindings[i].va,(size_t)job.bindings[i].bytes);if(e)goto done;mapped++;
  if(i<6) {
   void *p=NULL;e=ops->cpu_map(memory,id,true,&p);if(e||!p){if(!e)e=EOWNERDEAD;goto done;}
   memcpy(p,images[i],(size_t)job.bindings[i].bytes);e=ops->cpu_unmap(memory,id,true);if(e)goto done;
   e=ops->cpu_map(memory,id,false,&p);if(e||!p){if(!e)e=EOWNERDEAD;goto done;}
   bool equal=!memcmp(p,images[i],(size_t)job.bindings[i].bytes);
   e=ops->cpu_unmap(memory,id,false);if(e)goto done;if(!equal){e=EILSEQ;goto done;}
  }
  e=ops->sync_for_device(memory,id);if(e)goto done;
 }
 job.context=remesa_linux_memory_context(memory);
 e=remesa_retained_request_validate(&job);if(e)goto done;
 struct remesa_persistent_config cfg={.ops=ops,.priv=memory,.context=job.context,
  .va_start=0x23000000,.va_end=0x23010000,.byte_budget=16384,.resource_only_backend=true};
 e=remesa_linux_enable_shader_staging(&device,&cfg);if(e)goto done;
 struct remesa_linux_fragment_carrier carrier={.memory=memory,.job=&job,
  .record=remesa_fragment_record_retained,.authenticate_fragment=remesa_fragment_authenticate,
  .priv=&capture,.require_program_change=true,.fragment_argb=UINT32_C(0xff00ff00)};
 if(glx_mode||!strcmp(argv[1],"--execute-qualified-green-vertex-once")) {
  struct remesa_usse_vertex_mapping mapping={0};
  for(unsigned c=0;c<4;c++) {
   mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
   mapping.output[c]=(struct remesa_usse_cpu_register){1,c};
  }
  e=remesa_linux_set_vertex_candidate_mapping(&device,&mapping);if(e)goto done;
  carrier.independent_vertex=true;
 }
 if(glx_mode)carrier.gl_fbo_half_pixel_center=true;
 e=remesa_linux_enable_retained_fragment(&device,&carrier);if(e)goto done;
#ifdef REMESA_GLX_LAB_FRONTEND
 if(glx_mode)e=remesa_glx_lab_triangle(&device,argv[4]);
 else
#endif
 e=draw(&device,argv[4]);
done:
 if(device.priv) {
  (void)remesa_linux_last_retained_fragment(&device,&receipt);
  int close_error=remesa_linux_close_checked(&device);if(close_error&&!e)e=close_error;
 }
 /* Uncertain post-issue resources belong to kernel quarantine. A CPU timeout
  * never licenses unmap/unpin/free, reset, retry or another submission. */
 if(memory&&!receipt.quarantined&&!device.priv&&e!=EOWNERDEAD&&e!=EPROTO) {
  const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();int cleanup=0;
  for(unsigned i=allocated;i&&!cleanup;i--) {unsigned j=i-1;uint32_t id=job.bindings[j].id;
   if(j<mapped)cleanup=ops->gpu_unmap(memory,id,job.bindings[j].va,(size_t)job.bindings[j].bytes);
   if(!cleanup&&j<pinned)cleanup=ops->unpin(memory,id);
   if(!cleanup)cleanup=ops->destroy(memory,id);
  }
  if(!cleanup){cleanup=remesa_linux_memory_detach(memory);if(!cleanup)memory=NULL;}
  if(cleanup&&!e)e=cleanup;
 }
 if(fd>=0)close(fd);
 if(capture.memory_fd>=0)close(capture.memory_fd);
 if(capture.retained_fd>=0)close(capture.retained_fd);
 for(unsigned i=0;i<6;i++)free(images[i]);
 if(memory)remesa_linux_memory_forget(memory);
 printf("error=%d attempted=%u authenticated=%u quarantined=%u; capture origin requires protected controller validation\n",e,receipt.attempted,receipt.authenticated,receipt.quarantined);
 return e?1:0;
}
