/* SPDX-License-Identifier: MIT */
/* Real Mesa upload managers -> production Gallium/NIR callbacks.
 * Explicit CPU recorder and simulated retirement; no device or pixels. */
#include "uniform_nir_fixture.h"
#include "gallium/auxiliary/util/u_upload_mgr.h"
int inherited_upload_draw_main(void);
#define main inherited_upload_draw_main
#include "test_driver_nir.c"
#undef main
#undef remesa_bind_diagnostic_pipeline
#ifdef NDEBUG
#error Upload tests require enabled assertions
#endif
static unsigned creations,destructions,fail_creation;
struct u_upload_mgr *__real_u_upload_create(struct pipe_context *,unsigned,
 unsigned,enum pipe_resource_usage,unsigned);
struct u_upload_mgr *__wrap_u_upload_create(struct pipe_context *,unsigned,
 unsigned,enum pipe_resource_usage,unsigned);
void __real_u_upload_destroy(struct u_upload_mgr *);
void __wrap_u_upload_destroy(struct u_upload_mgr *);
struct u_upload_mgr *__wrap_u_upload_create(struct pipe_context *p,unsigned n,
 unsigned bind,enum pipe_resource_usage usage,unsigned flags)
{
 if(++creations==fail_creation)return NULL;
 return __real_u_upload_create(p,n,bind,usage,flags);
}
void __wrap_u_upload_destroy(struct u_upload_mgr *upload)
{ assert(upload);++destructions;__real_u_upload_destroy(upload); }

static void read_bytes(struct pipe_context *p,struct pipe_resource *b,
 unsigned offset,const void *expected,unsigned bytes)
{
 struct pipe_box box={.x=offset,.width=bytes,.height=1,.depth=1};
 struct pipe_transfer *tr=NULL;void *data=p->buffer_map(p,b,0,PIPE_MAP_READ,&box,&tr);
 assert(data&&tr&&!memcmp(data,expected,bytes));p->buffer_unmap(p,tr);
}
int main(void)
{
 struct recorder rec={0};struct remesa_device dev={.ops=&ops,.priv=&rec,.offline_test_backend=true};
 struct pipe_screen *s=remesa_screen_create(&dev);assert(s);
 for(unsigned fail=1;fail<=2;fail++) {
  creations=destructions=0;fail_creation=fail;
  assert(!s->context_create(s,NULL,0));
  assert(creations==fail&&destructions==fail-1&&!rec.allocs);
 }
 creations=destructions=fail_creation=0;
 struct pipe_context *p=s->context_create(s,NULL,0);assert(p);
 assert(p->stream_uploader&&p->const_uploader&&p->stream_uploader!=p->const_uploader);
 assert(creations==2&&!s->caps.buffer_map_persistent_coherent);
 assert(s->caps.constant_buffer_offset_alignment==16);
 const float vertices[3][4]={{-.5f,-.5f,0,1},{.5f,-.5f,0,1},{-.5f,.5f,0,1}};
 const uint16_t indices[]={0,1,2};
 const uint32_t green[]={0,0x3f800000,0,0x3f800000};
 struct pipe_resource *vb=NULL,*ib=NULL,*cb=NULL;unsigned vo,io,co;
 u_upload_data(p->stream_uploader,16,sizeof(vertices),16,vertices,&vo,&vb);
 u_upload_data(p->stream_uploader,0,sizeof(indices),16,indices,&io,&ib);
 u_upload_data(p->const_uploader,32,sizeof(green),16,green,&co,&cb);
 assert(vb&&ib&&cb&&vb==ib&&vb!=cb&&vo==16&&!(io&15)&&co==32);
 assert(vb->flags==PIPE_RESOURCE_FLAG_SINGLE_THREAD_USE&&cb->bind==PIPE_BIND_CONSTANT_BUFFER);
 u_upload_unmap(p->stream_uploader);u_upload_unmap(p->const_uploader);
 read_bytes(p,vb,vo,vertices,sizeof(vertices));read_bytes(p,ib,io,indices,sizeof(indices));
 read_bytes(p,cb,co,green,sizeof(green));
 struct pipe_vertex_buffer v={.buffer_offset=vo,.buffer.resource=vb};++vb->reference.count;
 p->set_vertex_buffers(p,1,&v);
 struct pipe_vertex_element e={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};
 void *ve=p->create_vertex_elements_state(p,1,&e);assert(ve);p->bind_vertex_elements_state(p,ve);
 assert(!bind_nir(p,0xff00ff00));
 struct fixture uniform=make(0);struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=uniform.s};
 void *fs=p->create_fs_state(p,&sh);assert(fs);p->bind_fs_state(p,fs);
 struct pipe_constant_buffer cbuf={.buffer=cb,.buffer_offset=co,.buffer_size=16};
 p->set_constant_buffer(p,PIPE_SHADER_FRAGMENT,0,false,&cbuf);assert(!remesa_context_error(p));
 struct pipe_resource rt={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *target=s->resource_create(s,&rt);assert(target);
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};
 fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;p->set_framebuffer_state(p,&fb);
 assert(!remesa_context_set_framebuffer_load(p,true)); /* ordinary first draw */
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1,
  .index_size=2,.index.resource=ib};struct pipe_draw_start_count_bias draw={.start=io/2,.count=3};
 p->draw_vbo(p,&info,0,NULL,&draw,1);
 assert(!remesa_context_error(p)&&rec.draws==1&&rec.shader_submits==1&&rec.packet.argb==0xff00ff00);
 struct recorded_programs *programs=rec.last->programs;assert(programs);
 assert(programs->inputs.vertex_offset==vo&&programs->inputs.start==io/2);
 assert(!memcmp((const unsigned char *)programs->inputs.vertex_data+vo,vertices,sizeof(vertices)));
 assert(!memcmp((const unsigned char *)programs->inputs.index_data+io,indices,sizeof(indices)));
 rec.last->retired=true;rec.last->target->published=true; /* CPU oracle only */
 /* Rollover cannot invalidate an old BO still retained by a binding/caller. */
 unsigned char payload[4096];memset(payload,0xa5,sizeof(payload));
 struct pipe_resource *rolled=NULL;unsigned ro;
 u_upload_data(p->stream_uploader,0,sizeof(payload),16,payload,&ro,&rolled);
 assert(rolled&&rolled!=vb);u_upload_unmap(p->stream_uploader);
 read_bytes(p,vb,vo,vertices,sizeof(vertices));read_bytes(p,rolled,ro,payload,sizeof(payload));
 /* Allocation failure releases a partial backend BO and returns no subrange. */
 struct pipe_resource *failed=NULL;unsigned fo;void *ptr=(void *)1;
 rec.allocation_error=true;u_upload_alloc(p->stream_uploader,0,8192,16,&fo,&failed,&ptr);
 assert(!failed&&!ptr&&fo==~0u);rec.allocation_error=false;
 /* A new upload after failure works; context teardown flushes its live lease. */
 u_upload_data(p->stream_uploader,0,16,16,green,&ro,&failed);assert(failed);
 resource_drop(failed);resource_drop(rolled);resource_drop(vb);resource_drop(ib);resource_drop(cb);
 p->destroy(p);resource_drop(target);
 assert(destructions==2&&rec.maps==rec.unmaps&&rec.allocs==rec.frees&&rec.shader_allocs==rec.shader_frees);
 s->destroy(s);
 puts("PASS: real context uploaders, partial-init rollback, aligned VB/IB/constants -> NIR draw handoff, rollover, allocation failure and mapped teardown; CPU recorder, NO SGX");
 return 0;
}
