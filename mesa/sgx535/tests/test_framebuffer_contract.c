/* SPDX-License-Identifier: MIT; recording backend only, no GPU pixels.
 * Validate explicit preserve/replace admission, not hardware load semantics. */
int existing_driver_tests(void);
#define main existing_driver_tests
#include "test_driver.c"
#undef main
static unsigned maps_seen,waits_seen;
static int counted_map(void *p,void *b,bool w,void **o){++maps_seen;return map_bo(p,b,w,o);}
static int counted_wait(void *p,void *f,uint64_t n,bool v){++waits_seen;return wait_fence(p,f,n,v);}
static const struct remesa_device_ops counted_ops={.allocate=alloc,.destroy_bo=drop,.map=counted_map,.unmap=unmap_bo,.submit=submit,.wait=counted_wait,.destroy_fence=destroy_fence};
static void complete(struct recorder *rec)
{ assert(rec->last);rec->last->retired=true;rec->last->target->published=true;rec->last->target->pending=false; }
static void setup(struct pipe_context *ctx,struct pipe_resource *vb,struct pipe_resource *target)
{
 struct pipe_framebuffer_state fb={0};fb.width=fb.height=32;fb.nr_cbufs=1;fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;ctx->set_framebuffer_state(ctx,&fb);
 struct pipe_vertex_buffer v={0};v.buffer.resource=vb;++vb->reference.count;ctx->set_vertex_buffers(ctx,1,&v);
 struct pipe_vertex_element e={0};e.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT;e.src_stride=16;void *h=ctx->create_vertex_elements_state(ctx,1,&e);assert(h);ctx->bind_vertex_elements_state(ctx,h);
 assert(!remesa_bind_diagnostic_pipeline(ctx,0xffff00ff));
 /* Explicitly opt back into ordinary framebuffer LOAD semantics. */
 assert(!remesa_context_set_framebuffer_load(ctx,true));
}
static void issue(struct pipe_context *ctx)
{
 struct pipe_draw_info i={0};struct pipe_draw_start_count_bias d={0};i.mode=MESA_PRIM_TRIANGLES;i.instance_count=1;d.count=3;ctx->draw_vbo(ctx,&i,0,NULL,&d,1);
}
static unsigned surface_samples(void)
{
 struct recorder rec={0};struct remesa_device dev={.ops=&counted_ops,.priv=&rec,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&dev);assert(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);assert(ctx);
 struct pipe_resource tmpl={0};tmpl.target=PIPE_TEXTURE_2D;tmpl.format=PIPE_FORMAT_B8G8R8A8_UNORM;
 tmpl.width0=tmpl.height0=32;tmpl.depth0=tmpl.array_size=1;tmpl.bind=PIPE_BIND_RENDER_TARGET;
 struct pipe_resource *target=screen->resource_create(screen,&tmpl);assert(target);
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};
 fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;
 unsigned cases=0;
 /* Mesa embeds surface state in framebuffer_state: create_surface need not
  * have validated it. Reject view multisampling even over single-sample BOs. */
 const unsigned samples[]={0,1,2,4,65535};
 for(unsigned i=0;i<sizeof(samples)/sizeof(samples[0]);i++) {
  fb.cbufs[0].nr_samples=samples[i];ctx->set_framebuffer_state(ctx,&fb);
  assert(remesa_context_error(ctx)==(samples[i]>1?ENOTSUP:0));
  assert(target->reference.count==(samples[i]>1?1:2));++cases;
 }
 fb.cbufs[0].nr_samples=1;ctx->set_framebuffer_state(ctx,&fb);
 assert(!remesa_context_error(ctx)&&target->reference.count==2);++cases;
 assert(!rec.draws&&!rec.maps);
 ctx->destroy(ctx);resource_drop(target);assert(rec.allocs==rec.frees);screen->destroy(screen);
 return cases;
}
int main(void)
{
 unsigned cases=surface_samples();
 struct remesa_device invalid={.ops=&counted_ops,.offline_test_backend=true,.framebuffer_mode=(enum remesa_framebuffer_mode)99};
 assert(!remesa_screen_create(&invalid));++cases;
 invalid.framebuffer_mode=REMESA_FRAMEBUFFER_PRESERVE;invalid.offline_test_backend=false;invalid.qualified_hardware_backend=true;
 assert(!remesa_screen_create(&invalid));++cases;
 /* A qualified hardware owner is currently REPLACE-only.  Preserve must be
  * rejected at the state setter, before a draw can reach the backend. */
 {
  struct recorder rec={0};
  struct remesa_device dev={.ops=&counted_ops,.priv=&rec,.qualified_hardware_backend=true,
                            .framebuffer_mode=REMESA_FRAMEBUFFER_REPLACE};
  struct pipe_screen *screen=remesa_screen_create(&dev);
  assert(screen);struct pipe_context *ctx=screen->context_create(screen,NULL,0);assert(ctx);
  assert(remesa_context_set_framebuffer_load(ctx,true)==ENOTSUP);++cases;
  assert(!remesa_context_set_framebuffer_load(ctx,false));++cases;
  ctx->destroy(ctx);screen->destroy(screen);
 }
 for(unsigned mode=0;mode<2;mode++) {
  struct recorder rec={0};struct remesa_device dev={.ops=&counted_ops,.priv=&rec,.offline_test_backend=true,.framebuffer_mode=mode?REMESA_FRAMEBUFFER_PRESERVE:REMESA_FRAMEBUFFER_REPLACE};
  struct pipe_screen *screen=remesa_screen_create(&dev);assert(screen);
  struct pipe_context *ctx=screen->context_create(screen,NULL,0),*other=screen->context_create(screen,NULL,0);assert(ctx&&other);
  struct pipe_resource *vb=buffer(screen,48,PIPE_BIND_VERTEX_BUFFER);assert(vb);
  struct pipe_box box={.width=48,.height=1,.depth=1};struct pipe_transfer *t=NULL;
  void *data=ctx->buffer_map(ctx,vb,0,PIPE_MAP_WRITE,&box,&t);assert(data&&t);
  const float vertices[3][4]={{8,8,.5,1},{24,8,.5,1},{8,24,.5,1}};memcpy(data,vertices,sizeof(vertices));ctx->buffer_unmap(ctx,t);
  struct pipe_resource tmpl={0};tmpl.target=PIPE_TEXTURE_2D;tmpl.format=PIPE_FORMAT_B8G8R8A8_UNORM;tmpl.width0=tmpl.height0=32;tmpl.depth0=tmpl.array_size=1;tmpl.bind=PIPE_BIND_RENDER_TARGET;
  struct pipe_resource *target=screen->resource_create(screen,&tmpl);assert(target);setup(ctx,vb,target);
  issue(ctx);assert(!remesa_context_error(ctx)&&rec.draws==1);++cases;
  unsigned maps=maps_seen,waits=waits_seen;
  if(!mode){issue(ctx);assert(remesa_context_error(ctx)==ENOTSUP&&rec.draws==1&&maps_seen==maps&&waits_seen==waits);++cases;}
  complete(&rec);
  if(mode){issue(ctx);assert(!remesa_context_error(ctx)&&rec.draws==2);complete(&rec);++cases;}
  else {
   issue(ctx);assert(remesa_context_error(ctx)==ENOTSUP&&rec.draws==1);++cases;
   assert(!remesa_context_set_framebuffer_load(ctx,false));issue(ctx);assert(!remesa_context_error(ctx)&&rec.draws==2);complete(&rec);++cases;
   assert(!remesa_context_set_framebuffer_load(ctx,true));
  }
  /* CPU-assisted clear is a host operation. It must not submit another draw. */
  union pipe_color_union red={.f={1,0,0,1}};unsigned before=rec.draws;
  ctx->clear(ctx,PIPE_CLEAR_COLOR0,NULL,&red,0,0);assert(!remesa_context_error(ctx)&&rec.draws==before);++cases;
  box=(struct pipe_box){.width=32,.height=32,.depth=1};data=other->texture_map(other,target,0,PIPE_MAP_READ,&box,&t);assert(data&&t);
  for(unsigned i=0;i<1024;i++)assert(((uint32_t *)data)[i]==0xffff0000);
  other->texture_unmap(other,t);++cases;
  /* A non-NULL pointer transfers the mapping lease even on backend failure.
   * Failed CPU clears must release it once, leave pixels unchanged and never
   * claim a completed clear. Zero with NULL also requires lease cleanup. */
  for(unsigned map_mode=1;map_mode<=2;map_mode++) {
   rec.map_mode=map_mode;unsigned before_maps=rec.maps,before_unmaps=rec.unmaps;
   uint64_t clears=remesa_context_cpu_clear_count(ctx);
   union pipe_color_union blue={.f={0,0,1,1}};
   ctx->clear(ctx,PIPE_CLEAR_COLOR0,NULL,&blue,0,0);
   assert(remesa_context_error(ctx)==(map_mode==1?EPROTO:EIO));
   assert(rec.maps==before_maps+1&&rec.unmaps==before_unmaps+1);
   assert(remesa_context_cpu_clear_count(ctx)==clears&&rec.draws==before);
   rec.map_mode=0;data=other->texture_map(other,target,0,PIPE_MAP_READ,&box,&t);assert(data&&t);
   for(unsigned i=0;i<1024;i++)assert(((uint32_t *)data)[i]==0xffff0000);
   other->texture_unmap(other,t);++cases;
  }
  if(!mode){maps=maps_seen;waits=waits_seen;issue(ctx);assert(remesa_context_error(ctx)==ENOTSUP&&rec.draws==before&&maps_seen==maps&&waits_seen==waits);++cases;}
  else{issue(ctx);assert(!remesa_context_error(ctx)&&rec.draws==before+1);complete(&rec);++cases;}
  /* A write made through another context is resource-wide LOAD content. */
  box=(struct pipe_box){.x=1,.y=2,.width=1,.height=1,.depth=1};data=other->texture_map(other,target,0,PIPE_MAP_WRITE,&box,&t);assert(data);*(uint32_t *)data=0xff00ff00;other->texture_unmap(other,t);
  before=rec.draws;issue(ctx);
  if(!mode)assert(remesa_context_error(ctx)==ENOTSUP&&rec.draws==before);
  else{assert(!remesa_context_error(ctx)&&rec.draws==before+1);complete(&rec);}++cases;
  /* Explicit diagnostic discard remains a separate, visible opt-in. */
  assert(!remesa_context_set_framebuffer_load(ctx,false));rec.submit_error=EIO;rec.error_fence=true;
  before=rec.draws;issue(ctx);assert(remesa_context_error(ctx)==EIO&&rec.draws==before);++cases;
  rec.submit_error=0;issue(ctx);assert(remesa_context_error(ctx)==EIO&&rec.draws==before);++cases;
  box=(struct pipe_box){.width=32,.height=32,.depth=1};assert(!ctx->texture_map(ctx,target,0,PIPE_MAP_READ,&box,&t));++cases;
  ctx->destroy(ctx);other->destroy(other);resource_drop(vb);resource_drop(target);assert(rec.allocs==rec.frees);screen->destroy(screen);++cases;
 }
 printf("PASS %u framebuffer load/replace contract scenarios (mocked submission, no SGX)\n",cases);return 0;
}
