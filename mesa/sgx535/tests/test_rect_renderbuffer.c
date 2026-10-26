/* SPDX-License-Identifier: MIT
 * Mesa uses RECT storage for renderbuffers when NPOT sampling is unavailable.
 * Resource/transfer/attachment test only: no GPU or sampler qualification.
 */
int inherited_driver_fixture(void);
#define main inherited_driver_fixture
#include "test_driver_nir.c"
#undef main
#include "remesa_resource_ref.h"
#include "remesa_present.h"
int main(void)
{
 struct recorder rec={0};struct remesa_device device={.ops=&ops,.priv=&rec,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&device);assert(screen);
 struct pipe_context *ctx=screen->context_create(screen,NULL,0);assert(ctx);
 assert(!screen->caps.npot_textures);
 assert(screen->is_format_supported(screen,PIPE_FORMAT_B8G8R8A8_UNORM,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_RENDER_TARGET));
 const unsigned bad_binds[]={0,PIPE_BIND_SAMPLER_VIEW,PIPE_BIND_RENDER_TARGET|PIPE_BIND_SAMPLER_VIEW};
 for(unsigned i=0;i<3;i++)assert(!screen->is_format_supported(screen,PIPE_FORMAT_B8G8R8A8_UNORM,PIPE_TEXTURE_RECT,0,0,bad_binds[i]));
 assert(!screen->is_format_supported(screen,PIPE_FORMAT_R8G8B8A8_UNORM,PIPE_TEXTURE_RECT,0,0,PIPE_BIND_RENDER_TARGET));
 struct pipe_resource t={.target=PIPE_TEXTURE_RECT,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *color=screen->resource_create(screen,&t);assert(color&&color->target==PIPE_TEXTURE_RECT);
 for(unsigned mode=0;mode<6;mode++) {
  struct pipe_resource invalid=t;
  switch(mode){case 0:invalid.width0=31;break;case 1:invalid.last_level=1;break;case 2:invalid.array_size=2;break;case 3:invalid.depth0=2;break;case 4:invalid.nr_samples=2;break;case 5:invalid.bind|=PIPE_BIND_SAMPLER_VIEW;break;}
  assert(!screen->resource_create(screen,&invalid));
 }
 struct pipe_surface st={.format=t.format};struct pipe_surface *surface=ctx->create_surface(ctx,color,&st);assert(surface);
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};fb.cbufs[0]=*surface;
 ctx->set_framebuffer_state(ctx,&fb);assert(!remesa_context_error(ctx));
 union pipe_color_union green={.f={0,1,0,1}};
 ctx->clear(ctx,PIPE_CLEAR_COLOR0,NULL,&green,0,0);assert(!remesa_context_error(ctx));
 assert(remesa_context_cpu_clear_count(ctx)==1);
 assert(screen->caps.clear_scissored);
 union pipe_color_union red={.f={1,0,0,1}};
 struct pipe_scissor_state scissor={.minx=3,.miny=5,.maxx=10,.maxy=14};
 ctx->clear(ctx,PIPE_CLEAR_COLOR0,&scissor,&red,0,0);assert(!remesa_context_error(ctx));
 assert(remesa_context_cpu_clear_count(ctx)==2);
 struct pipe_scissor_state bad={.minx=10,.miny=5,.maxx=3,.maxy=14};
 ctx->clear(ctx,PIPE_CLEAR_COLOR0,&bad,&red,0,0);assert(remesa_context_error(ctx)==EINVAL);
 bad=(struct pipe_scissor_state){.maxx=33,.maxy=32};
 ctx->clear(ctx,PIPE_CLEAR_COLOR0,&bad,&red,0,0);assert(remesa_context_error(ctx)==EINVAL);
 assert(remesa_context_cpu_clear_count(ctx)==2);
 struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *tr=NULL;
 const unsigned char *data=ctx->texture_map(ctx,color,0,PIPE_MAP_READ,&box,&tr);assert(data&&tr);
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++){uint32_t v;memcpy(&v,data+y*tr->stride+4*x,4);assert(v==(x>=3&&x<10&&y>=5&&y<14?UINT32_C(0xffff0000):UINT32_C(0xff00ff00)));}
 ctx->texture_unmap(ctx,tr);
 const uint32_t magenta=UINT32_C(0xffff00ff);box=(struct pipe_box){.x=7,.y=9,.width=1,.height=1,.depth=1};
 ctx->texture_subdata(ctx,color,0,PIPE_MAP_WRITE,&box,&magenta,4,4);assert(!remesa_context_error(ctx));
 data=ctx->texture_map(ctx,color,0,PIPE_MAP_READ,&box,&tr);assert(data&&tr);uint32_t v;memcpy(&v,data,4);assert(v==magenta);ctx->texture_unmap(ctx,tr);
 unsigned char snapshot[32*144];memset(snapshot,0xa5,sizeof(snapshot));
 assert(remesa_present_snapshot(ctx,color,snapshot,127,sizeof(snapshot))==EINVAL);
 assert(remesa_present_snapshot(ctx,color,snapshot,144,1)==ENOSPC);
 for(unsigned i=0;i<sizeof(snapshot);i++)assert(snapshot[i]==0xa5);
 assert(!remesa_present_snapshot(ctx,color,snapshot,144,sizeof(snapshot)));
 for(unsigned y=0;y<32;y++) {
  for(unsigned x=0;x<32;x++) {uint32_t pixel;memcpy(&pixel,snapshot+y*144+x*4,4);
   assert(pixel==(x==7&&y==9?magenta:x>=3&&x<10&&y>=5&&y<14?UINT32_C(0xffff0000):UINT32_C(0xff00ff00)));}
  for(unsigned x=128;x<144;x++)assert(snapshot[y*144+x]==0xa5);
 }
 struct remesa_draw_rejection_status status={0};
 assert(!remesa_context_draw_rejections(ctx,&status)&&!status.failures);
 ctx->draw_vbo(ctx,NULL,0,NULL,NULL,0);assert(remesa_context_error(ctx)==ENOTSUP);
 ctx->set_sample_mask(ctx,1);assert(!remesa_context_error(ctx));
 assert(!remesa_context_draw_rejections(ctx,&status)&&status.failures==1&&status.error==ENOTSUP);
 struct pipe_draw_info empty_info={.mode=MESA_PRIM_TRIANGLES,.instance_count=1};
 struct pipe_draw_start_count_bias empty_draw={0};
 ctx->draw_vbo(ctx,&empty_info,0,NULL,&empty_draw,1);assert(!remesa_context_error(ctx));
 assert(!remesa_context_draw_rejections(ctx,&status)&&status.failures==1&&status.error==ENOTSUP);
 ctx->surface_destroy(ctx,surface);ctx->destroy(ctx);remesa_resource_reference(&color,NULL);screen->destroy(screen);
 assert(rec.allocs==rec.frees&&!rec.draws&&!rec.shader_prepares);
 puts("PASS RECT renderbuffer storage/clear/transfers/snapshot+padding; sampler/shape/bounds negatives; GPU submissions0");return 0;
}
