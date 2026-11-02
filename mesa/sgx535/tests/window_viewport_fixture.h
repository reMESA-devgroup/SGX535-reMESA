/* SPDX-License-Identifier: MIT. Shared actual Gallium state fixture; GPU0. */
#include "remesa_persistent.h"
static int reject_viewport_fill(void *priv,const struct remesa_fill_region *fill)
{(void)priv;(void)fill;assert(!"viewport encoding must not issue a fill");return ENOTSUP;}
static void check_window_viewports(void)
{
 struct recorder rec={0};struct remesa_device dev={.ops=&ops,.priv=&rec,.resource_only_backend=true,
  .resource_limits={4096,4096,4,32u*1024u*1024u}};
 struct pipe_screen *s=remesa_screen_create(&dev);assert(s);
 assert(!remesa_screen_enable_native_fill(s,reject_viewport_fill)); /* existing large native2D storage route */
 struct pipe_context *c=s->context_create(s,NULL,0);assert(c);
 unsigned char out[28],saved[28],expected[28];memset(out,0xa5,sizeof(out));memcpy(saved,out,sizeof(out));
 struct remesa_ta_window_viewport_request typed,saved_typed;memset(&typed,0xa5,sizeof typed);saved_typed=typed;
 struct remesa_vertex_parameter_values parameters,saved_parameters;
 memset(&parameters,0xa5,sizeof parameters);saved_parameters=parameters;
 assert(remesa_context_window_viewport_encode(c,24,out)==ENODATA&&!memcmp(out,saved,28));
 assert(remesa_context_window_viewport_snapshot(c,24,&typed)==ENODATA&&!memcmp(&typed,&saved_typed,sizeof typed));
 assert(remesa_context_window_viewport_parameters_snapshot(c,&parameters)==ENODATA&&!memcmp(&parameters,&saved_parameters,sizeof parameters));
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 for(unsigned k=0;k<4;k++){
  unsigned w=sizes[k][0],h=sizes[k][1];
  struct pipe_resource t={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
   .width0=w,.height0=h,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
  struct pipe_resource *target=s->resource_create(s,&t);assert(target);
  struct pipe_framebuffer_state fb={.width=w,.height=h,.nr_cbufs=1,.cbufs={{.texture=target,.format=t.format}}};
  c->set_framebuffer_state(c,&fb);assert(!remesa_context_error(c));
  for(unsigned sub=0;sub<2;sub++)for(unsigned z=0;z<3;z++){
   struct remesa_ta_window_viewport_request q={.framebuffer_width=w,.framebuffer_height=h,
    .x=sub?7:0,.y=sub?9:0,.width=sub?w-14:w,.height=sub?h-18:h,.depth_bits=z==2?16:24,
    .near_z=z==1?.75f:0,.far_z=z==1?.25f:1,.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW};
   struct pipe_viewport_state v={.scale={q.width*.5f,-(float)q.height*.5f,(q.far_z-q.near_z)*.5f},
    .translate={q.x+q.width*.5f,h-(q.y+q.height*.5f),(q.far_z+q.near_z)*.5f}};
   c->set_viewport_states(c,0,1,&v);assert(!remesa_context_error(c));
   unsigned maps=rec.maps,allocs=rec.allocs;
   assert(!remesa_context_window_viewport_encode(c,q.depth_bits,out));
   assert(!remesa_ta_window_viewport_encode(&q,expected)&&!memcmp(out,expected,28));
   assert(!remesa_context_window_viewport_snapshot(c,q.depth_bits,&typed));
   assert(typed.framebuffer_width==q.framebuffer_width&&typed.framebuffer_height==q.framebuffer_height&&
    typed.x==q.x&&typed.y==q.y&&typed.width==q.width&&typed.height==q.height&&
    typed.depth_bits==q.depth_bits&&typed.near_z==q.near_z&&typed.far_z==q.far_z&&typed.path==q.path);
   assert(!remesa_ta_window_viewport_encode(&typed,expected)&&!memcmp(out,expected,28));saved_typed=typed;
   assert(!remesa_context_window_viewport_parameters_snapshot(c,&parameters));
   for(unsigned axis=0;axis<3;axis++){
    assert(!memcmp(&parameters.coefficient_bits[axis],&v.scale[axis],4));
    assert(!memcmp(&parameters.coefficient_bits[4+axis],&v.translate[axis],4));
   }
   assert(parameters.coefficient_bits[3]==0x3f800000);
   for(unsigned axis=7;axis<16;axis++)assert(!parameters.coefficient_bits[axis]);
   saved_parameters=parameters;
   assert(rec.maps==maps&&rec.allocs==allocs&&!remesa_context_error(c));
   memcpy(saved,out,28);
   assert(remesa_context_window_viewport_encode(c,32,out)==ENOTSUP&&!memcmp(out,saved,28));
   assert(remesa_context_window_viewport_snapshot(c,32,&typed)==ENOTSUP&&!memcmp(&typed,&saved_typed,sizeof typed));
   assert(remesa_context_window_viewport_encode(c,24,(void *)target)==EINVAL);
   assert(remesa_context_window_viewport_snapshot(c,24,(void *)target)==EINVAL);
   assert(remesa_context_window_viewport_snapshot(c,24,(void *)c)==EINVAL);
   assert(remesa_context_window_viewport_snapshot(c,24,(void *)((unsigned char *)&typed+1))==EINVAL);
   assert(remesa_context_window_viewport_snapshot(c,24,(void *)(UINTPTR_MAX-3))==EINVAL);
   assert(remesa_context_window_viewport_parameters_snapshot(c,(void *)target)==EINVAL);
   assert(remesa_context_window_viewport_parameters_snapshot(c,(void *)c)==EINVAL);
   assert(remesa_context_window_viewport_parameters_snapshot(c,(void *)((unsigned char *)&parameters+1))==EINVAL);
   assert(remesa_context_window_viewport_parameters_snapshot(c,(void *)(UINTPTR_MAX-3))==EINVAL);
   const struct pipe_viewport_state good=v;
   for(unsigned bad=0;bad<9;bad++){
    v=good;
    switch(bad){
    case 0:v.scale[0]=-v.scale[0];break;
    case 1:v.scale[1]=-v.scale[1];break;
    case 2:v.scale[0]=0;break;
    case 3:v.translate[0]=-1;break;
    case 4:v.scale[0]=.3f;break;
    case 5:v.translate[2]=2;break;
    case 6:v.translate[0]=INFINITY;break;
    case 7:v.scale[1]=NAN;break;
    case 8:v.scale[2]=.1f;v.translate[2]=.5f;break;
    }
    c->set_viewport_states(c,0,1,&v);
    assert(remesa_context_window_viewport_encode(c,24,out)!=0&&!memcmp(out,saved,28));
    assert(remesa_context_window_viewport_snapshot(c,24,&typed)!=0&&!memcmp(&typed,&saved_typed,sizeof typed));
    assert(remesa_context_window_viewport_parameters_snapshot(c,&parameters)!=0&&!memcmp(&parameters,&saved_parameters,sizeof parameters));
   }
   c->set_viewport_states(c,0,1,&good);assert(!remesa_context_error(c));
  }
  struct pipe_framebuffer_state empty={0};c->set_framebuffer_state(c,&empty);
  assert(remesa_context_window_viewport_encode(c,24,out)==ENODATA);resource_drop(target);
  assert(remesa_context_window_viewport_snapshot(c,24,&typed)==ENODATA);
 }
 c->destroy(c);s->destroy(s);assert(rec.allocs==rec.frees&&!rec.maps&&!rec.draws);
 struct remesa_device legacy={.ops=&ops,.priv=&rec,.offline_test_backend=true};s=remesa_screen_create(&legacy);assert(s);
 c=s->context_create(s,NULL,0);assert(c);
 struct pipe_viewport_state large={.scale={640,-400,.5},.translate={640,400,.5}};
 c->set_viewport_states(c,0,1,&large);assert(remesa_context_error(c)==ENOTSUP);
 assert(remesa_context_window_viewport_encode(c,24,out)==ENOTSUP);
 assert(remesa_context_window_viewport_snapshot(c,24,&typed)==ENOTSUP);
 c->destroy(c);s->destroy(s);
}
