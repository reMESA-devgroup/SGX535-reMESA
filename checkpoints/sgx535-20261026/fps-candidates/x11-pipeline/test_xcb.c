/* SPDX-License-Identifier: MIT */
/* Simulated publication/fence fixtures; no X server, desktop or GPU access. */
#include "remesa_xcb.h"
#include "remesa_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include <xcb/xcb.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned state,queries,creates,puts_,frees,checks,map_count,unmaps;
static int fail,mock_context_error;static unsigned char pixels[4096];
static unsigned char *large_pixels;static unsigned large_stride;
static unsigned drawable_width=32,drawable_height=32,tile_y,scaled_test,wm_requests,wm_action;
static struct pipe_resource *active;
static bool raw_test,drawable_test;static unsigned char raw_expected[4096];
static unsigned char *raw_large_expected;
static xcb_setup_t setup={.image_byte_order=XCB_IMAGE_ORDER_LSB_FIRST,.maximum_request_length=65535,.resource_id_mask=0xff};
static xcb_screen_t screen={.root=1};
static xcb_format_t format={.depth=24,.bits_per_pixel=32,.scanline_pad=32};
static xcb_depth_t depth={.depth=24};
static xcb_visualtype_t visual={.visual_id=9,._class=XCB_VISUAL_CLASS_TRUE_COLOR,.red_mask=0xff0000,.green_mask=0xff00,.blue_mask=0xff};
int xcb_connection_has_error(xcb_connection_t *c) {(void)c;return fail==1 || (fail==12 && puts_);}
const xcb_setup_t *xcb_get_setup(xcb_connection_t *c) {(void)c;return &setup;}
xcb_format_iterator_t xcb_setup_pixmap_formats_iterator(const xcb_setup_t *s) {(void)s;return (xcb_format_iterator_t){&format,1,0};}
xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *s) {(void)s;return (xcb_screen_iterator_t){&screen,1,0};}
xcb_depth_iterator_t xcb_screen_allowed_depths_iterator(const xcb_screen_t *s) {(void)s;return (xcb_depth_iterator_t){&depth,1,0};}
xcb_visualtype_iterator_t xcb_depth_visuals_iterator(const xcb_depth_t *d) {(void)d;return (xcb_visualtype_iterator_t){&visual,1,0};}
#define NEXT(type) void xcb_##type##_next(xcb_##type##_iterator_t *i) {--i->rem;}
NEXT(format) NEXT(screen) NEXT(depth) NEXT(visualtype)
xcb_get_geometry_cookie_t xcb_get_geometry(xcb_connection_t *c,xcb_drawable_t w) {(void)c;(void)w;++queries;return (xcb_get_geometry_cookie_t){1};}
xcb_get_geometry_reply_t *xcb_get_geometry_reply(xcb_connection_t *c,xcb_get_geometry_cookie_t cookie,xcb_generic_error_t **e)
{(void)c;(void)cookie;*e=NULL;if(fail==2)return NULL;xcb_get_geometry_reply_t *g=calloc(1,sizeof(*g));g->root=1;g->width=fail==3 ? 1 : drawable_width;g->height=drawable_height;g->depth=24;return g;}
xcb_get_window_attributes_cookie_t xcb_get_window_attributes(xcb_connection_t *c,xcb_window_t w) {(void)c;(void)w;++queries;return (xcb_get_window_attributes_cookie_t){2};}
xcb_get_window_attributes_reply_t *xcb_get_window_attributes_reply(xcb_connection_t *c,xcb_get_window_attributes_cookie_t cookie,xcb_generic_error_t **e)
{(void)c;(void)cookie;*e=NULL;xcb_get_window_attributes_reply_t *a=calloc(1,sizeof(*a));a->visual=9;a->_class=XCB_WINDOW_CLASS_INPUT_OUTPUT;return a;}
uint32_t xcb_generate_id(xcb_connection_t *c) {(void)c;return fail==8 ? UINT32_MAX : 77;}
xcb_void_cookie_t xcb_create_gc_checked(xcb_connection_t *c,xcb_gcontext_t gc,xcb_drawable_t window,uint32_t mask,const void *values)
{(void)c;(void)mask;(void)values;assert(gc==77&&window==42);assert(active->reference.count==(raw_test&&!drawable_test?1:2));++creates;if(raw_test)tile_y=0;return (xcb_void_cookie_t){fail==10 ? 0 : 3};}
xcb_void_cookie_t xcb_put_image_checked(xcb_connection_t *c,uint8_t f,xcb_drawable_t d,xcb_gcontext_t g,uint16_t w,uint16_t h,int16_t x,int16_t y,uint8_t pad,uint8_t dep,uint32_t n,const uint8_t *p)
{(void)c;assert(f==XCB_IMAGE_FORMAT_Z_PIXMAP&&d==42&&g==77&&!x&&!pad&&dep==24);
 if(raw_test){
  assert(w==drawable_width&&y==(int)tile_y&&n==(unsigned)w*h*4);
  const unsigned char *expected=raw_large_expected?raw_large_expected:raw_expected;
  assert(!memcmp(p,expected+(size_t)tile_y*w*4,n));tile_y+=h;
 }
 else if(large_pixels){assert(w==drawable_width&&y==(int)tile_y&&n==(unsigned)w*h*4);assert(n+sizeof(xcb_put_image_request_t)<=(unsigned)setup.maximum_request_length*4);assert(!memcmp(p,large_pixels+(size_t)tile_y*large_stride,n));tile_y+=h;}
 else if(scaled_test){assert(w==drawable_width&&y==(int)tile_y&&n==(unsigned)w*h*4);assert(n+sizeof(xcb_put_image_request_t)<=(unsigned)setup.maximum_request_length*4);for(unsigned i=0;i<n;i++)assert(p[i]==pixels[0]);tile_y+=h;}
 else assert(w==32&&h==32&&!y&&n==4096&&!memcmp(p,pixels,n));
 ++puts_;return (xcb_void_cookie_t){fail==9 ? 0 : 4};}
xcb_void_cookie_t xcb_free_gc_checked(xcb_connection_t *c,xcb_gcontext_t g) {(void)c;assert(g==77);++frees;return (xcb_void_cookie_t){5};}
xcb_generic_error_t *xcb_request_check(xcb_connection_t *c,xcb_void_cookie_t cookie)
{(void)c;++checks;if((fail==4&&cookie.sequence==3)||(fail==5&&cookie.sequence==4)||(fail==6&&cookie.sequence==5)||(fail==11&&cookie.sequence==4)){xcb_generic_error_t *e=calloc(1,sizeof(*e));e->error_code=fail==11 ? XCB_WINDOW : 8;return e;}return NULL;}
xcb_intern_atom_cookie_t xcb_intern_atom(xcb_connection_t *c,uint8_t only,uint16_t n,const char *name)
{(void)c;assert(!only);assert((n==13&&!memcmp(name,"_NET_WM_STATE",n))||(n==24&&!memcmp(name,"_NET_WM_STATE_FULLSCREEN",n)));return (xcb_intern_atom_cookie_t){n};}
xcb_intern_atom_reply_t *xcb_intern_atom_reply(xcb_connection_t *c,xcb_intern_atom_cookie_t cookie,xcb_generic_error_t **e)
{(void)c;*e=NULL;if(fail==13)return NULL;xcb_intern_atom_reply_t *r=calloc(1,sizeof(*r));r->atom=cookie.sequence;return r;}
xcb_void_cookie_t xcb_send_event_checked(xcb_connection_t *c,uint8_t propagate,xcb_window_t destination,uint32_t mask,const char *data)
{(void)c;const xcb_client_message_event_t *e=(const void *)data;assert(!propagate&&destination==1&&mask==(XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY));assert(e->window==42&&e->type==13&&e->format==32&&e->data.data32[1]==24&&e->data.data32[3]==1);wm_action=e->data.data32[0];++wm_requests;return (xcb_void_cookie_t){fail==14?0:6};}
/* Isolate the real presentation adapter/snapshot from an explicitly simulated
 * writer and Gallium transfer. The full driver gate is separately regression tested. */
/* Exercise both the current sticky-map error and its EIO fallback. */
int remesa_context_error(const struct pipe_context *c){(void)c;return mock_context_error;}
int remesa_resource_wait_writer(struct pipe_context *c,struct pipe_resource *r,uint64_t t)
{(void)c;(void)r;assert(!t);return state==1 ? EAGAIN : state==2 ? EIO : 0;}
static void *map(struct pipe_context *c,struct pipe_resource *r,unsigned level,unsigned usage,const struct pipe_box *box,struct pipe_transfer **out)
{(void)c;assert(!level&&usage==(PIPE_MAP_READ|PIPE_MAP_DONTBLOCK)&&box->width==(int)r->width0&&box->height==(int)r->height0);++map_count;
 if(fail==7)return NULL;
 struct pipe_transfer *t=calloc(1,sizeof(*t));t->resource=r;t->stride=large_pixels?large_stride:128;*out=t;return large_pixels?large_pixels:pixels;}
static void unmap(struct pipe_context *c,struct pipe_transfer *t) {(void)c;++unmaps;free(t);}
int main(void)
{
 struct pipe_screen *s=(void *)pixels;struct pipe_context c={.screen=s,.texture_map=map,.texture_unmap=unmap};
 struct pipe_resource r={.screen=s,.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=32,.height0=32,.depth0=1,.array_size=1};
 r.reference.count=1;active=&r;
 xcb_connection_t *connection=(void *)&r;struct remesa_xcb_receipt receipt;
 memset(pixels,0x5a,sizeof(pixels));
 for(state=1;state<=2;state++){assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==(state==1?EAGAIN:EIO));assert(!queries&&!map_count&&!receipt.image_request_sent);}state=0;
 fail=7;mock_context_error=EAGAIN;
 assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==EAGAIN);
 assert(!queries&&!receipt.image_request_sent);fail=0;mock_context_error=0;
 assert(!remesa_xcb_present(&c,&r,connection,42,&receipt));assert(receipt.snapshot_bytes==4096&&receipt.image_acknowledged&&receipt.gc_released&&puts_==1);
 assert(remesa_xcb_present(&c,&r,connection,1,&receipt)==ENOTSUP);assert(!receipt.image_request_sent);
 assert(remesa_xcb_present(&c,&r,connection,300,&receipt)==EACCES&&!receipt.image_request_sent);
 format.bits_per_pixel=24;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==ENOTSUP);format.bits_per_pixel=32;
 visual.red_mask=0xff;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==ENOTSUP);visual.red_mask=0xff0000;
 setup.image_byte_order=XCB_IMAGE_ORDER_MSB_FIRST;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==ENOTSUP);setup.image_byte_order=XCB_IMAGE_ORDER_LSB_FIRST;
 setup.maximum_request_length=10;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==E2BIG);setup.maximum_request_length=65535;
 for(fail=1;fail<=10;fail++) {
  unsigned before=puts_,gc_before=frees;int err=remesa_xcb_present(&c,&r,connection,42,&receipt);assert(err);
  assert(puts_==before+(fail==5||fail==6||fail==9));
  if(fail==5)assert(receipt.gc_released&&frees==gc_before+1&&!receipt.image_acknowledged);
  if(fail==6)assert(receipt.image_acknowledged&&!receipt.gc_released);
 }
 fail=0;
 for(unsigned n=2;n<=100;n=n==2?10:100) {
  unsigned before=puts_;
  for(unsigned frame=0;frame<n;frame++) {
   memset(pixels,frame,sizeof(pixels));
   assert(!remesa_xcb_present(&c,&r,connection,42,&receipt));
   assert(receipt.image_acknowledged&&receipt.gc_released&&r.reference.count==1);
  }
  assert(puts_==before+n);
  if(n==100)break;
 }
 fail=11;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==EIO);
 assert(receipt.x_error==XCB_WINDOW&&!receipt.image_acknowledged&&receipt.gc_released&&r.reference.count==1);
 /* Loss after the image was sent is uncertain, never reported as acknowledged. */
 fail=12;puts_=0;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==EPIPE);
 assert(receipt.image_request_sent&&!receipt.image_acknowledged&&!receipt.gc_released&&r.reference.count==1);
 fail=0;scaled_test=1;drawable_width=48;drawable_height=40;setup.maximum_request_length=102;tile_y=0;memset(pixels,0x5a,sizeof(pixels));
 unsigned before=puts_;
 assert(!remesa_xcb_present_scaled(&c,&r,connection,42,48,40,REMESA_PRESENT_STRETCH,&receipt));
 assert(tile_y==40&&puts_==before+20&&receipt.image_requests==20&&receipt.image_bytes==48*40*4&&receipt.snapshot_bytes==4096&&receipt.scale.cpu_output_bytes==48*40*4&&receipt.gc_released&&r.reference.count==1);
 tile_y=0;assert(remesa_xcb_present_scaled(&c,&r,connection,42,47,40,REMESA_PRESENT_STRETCH,&receipt)==ENOTSUP&&!receipt.image_request_sent);
 fail=5;tile_y=0;assert(remesa_xcb_present_scaled(&c,&r,connection,42,48,40,REMESA_PRESENT_STRETCH,&receipt)==EIO&&receipt.image_requests==1&&!receipt.image_acknowledged&&receipt.gc_released);
 fail=12;puts_=0;tile_y=0;assert(remesa_xcb_present_scaled(&c,&r,connection,42,48,40,REMESA_PRESENT_STRETCH,&receipt)==EPIPE&&!receipt.image_acknowledged);
 fail=0;assert(!remesa_xcb_request_fullscreen(connection,42,true,&receipt)&&receipt.fullscreen_request_acknowledged&&wm_action==1);
 assert(!remesa_xcb_request_fullscreen(connection,42,false,&receipt)&&wm_action==0&&wm_requests==2);
 assert(remesa_xcb_request_fullscreen(connection,1,true,&receipt)==ENOTSUP);
 assert(remesa_xcb_request_fullscreen(connection,300,true,&receipt)==EACCES);
 fail=13;assert(remesa_xcb_request_fullscreen(connection,42,true,&receipt)==EIO&&!receipt.fullscreen_request_sent);
 fail=14;assert(remesa_xcb_request_fullscreen(connection,42,true,&receipt)==EPROTO&&!receipt.fullscreen_request_acknowledged);
 fail=1;assert(remesa_xcb_request_fullscreen(connection,42,true,&receipt)==EPIPE);
 fail=0;
 const unsigned sizes[][2]={{32,32},{320,240},{640,480},{800,600},{1024,768},{1280,720},{1280,800},{1366,768},{1600,900},{1920,1080}};
 scaled_test=0;setup.maximum_request_length=65535;
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) {
  r.width0=drawable_width=sizes[i][0];r.height0=drawable_height=sizes[i][1];large_stride=r.width0*4;
  size_t bytes=(size_t)large_stride*r.height0;large_pixels=malloc(bytes);assert(large_pixels);
  for(unsigned y=0;y<r.height0;y++)for(unsigned x=0;x<large_stride;x++)large_pixels[(size_t)y*large_stride+x]=(unsigned char)(x*3+y*7);
  tile_y=0;unsigned images=puts_;
  assert(!remesa_xcb_present(&c,&r,connection,42,&receipt));
  unsigned rows=((unsigned)setup.maximum_request_length*4-sizeof(xcb_put_image_request_t))/large_stride;
  assert(receipt.image_requests==(r.height0+rows-1)/rows&&puts_==images+receipt.image_requests&&tile_y==r.height0);
  assert(receipt.snapshot_bytes==bytes&&receipt.image_bytes==bytes&&receipt.image_acknowledged&&receipt.gc_released&&r.reference.count==1);
  /* Mid-stream loss preserves uncertain publication, releases ownership, and
   * does not retry either snapshots or image requests. */
  if(i==1){fail=12;puts_=0;tile_y=0;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==EPIPE&&receipt.image_requests==1&&!receipt.image_acknowledged&&r.reference.count==1);fail=0;}
  free(large_pixels);large_pixels=NULL;
 }
 r.height0=32768;assert(remesa_xcb_present(&c,&r,connection,42,&receipt)==EINVAL&&!receipt.image_request_sent);
 r.height0=r.width0=32;
 drawable_width=drawable_height=32;scaled_test=0;setup.maximum_request_length=65535;
 raw_test=true;unsigned maps_before=map_count;
 unsigned char raw[32*132],original[sizeof(raw)];
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<132;x++)raw[y*132+x]=(unsigned char)(y*3+x);
 memcpy(original,raw,sizeof(raw));
 for(unsigned y=0;y<32;y++)memcpy(raw_expected+y*128,raw+y*132,128);
 assert(!remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt));
 assert(!receipt.source_y_inverted&&receipt.snapshot_bytes==4096&&receipt.image_acknowledged&&receipt.gc_released);
 for(unsigned y=0;y<32;y++)memcpy(raw_expected+y*128,raw+(31-y)*132,128);
 assert(!remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt));
 assert(receipt.source_y_inverted&&receipt.image_requests==1&&!memcmp(raw,original,sizeof(raw)));
 unsigned before_raw_queries=queries;
 assert(remesa_xcb_present_bgra(raw,127,sizeof(raw),32,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==EINVAL);
 assert(remesa_xcb_present_bgra(raw,132,4096,32,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==ENOSPC);
 assert(remesa_xcb_present_bgra(raw,SIZE_MAX,sizeof(raw),32,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==EOVERFLOW);
 assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),0,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==EINVAL);
 assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32768,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==EINVAL);
 assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,99,connection,42,&receipt)==EINVAL);
 assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,REMESA_XCB_TOP_LEFT,connection,42,(void *)raw)==EINVAL);
 assert(remesa_xcb_present_bgra((void *)(UINTPTR_MAX-4),128,4096,32,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==EINVAL);
 assert(queries==before_raw_queries&&!memcmp(raw,original,sizeof(raw)));
 assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,REMESA_XCB_TOP_LEFT,connection,1,&receipt)==ENOTSUP);
 assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,REMESA_XCB_TOP_LEFT,connection,300,&receipt)==EACCES);
 drawable_width=33;assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==ENOTSUP);drawable_width=32;
 fail=5;assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),32,32,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt)==EIO);
 assert(!receipt.image_acknowledged&&receipt.gc_released);fail=0;
 assert(map_count==maps_before&&!memcmp(raw,original,sizeof(raw))&&r.reference.count==1);
 drawable_width=drawable_height=1;memcpy(raw_expected,raw,4);
 assert(!remesa_xcb_present_bgra(raw,SIZE_MAX,4,1,1,REMESA_XCB_TOP_LEFT,connection,42,&receipt));
 assert(receipt.snapshot_bytes==4&&receipt.image_bytes==4&&receipt.image_requests==1);
 assert(remesa_xcb_present_bgra(raw,132,sizeof(raw),65536,32,REMESA_XCB_TOP_LEFT,connection,42,&receipt)==EINVAL);
 /* Full1280x800 publication, never a claim of SGX render resolution. */
 drawable_width=1280;drawable_height=800;const size_t pitch=1280*4+8;
 unsigned char *padded=malloc(pitch*800);raw_large_expected=malloc(1280*800*4);
 assert(padded&&raw_large_expected);
 for(unsigned y=0;y<800;y++)for(unsigned x=0;x<pitch;x++)padded[y*pitch+x]=(unsigned char)(y*7+x*3);
 for(unsigned y=0;y<800;y++)memcpy(raw_large_expected+(size_t)y*1280*4,padded+(799-y)*pitch,1280*4);
 assert(!remesa_xcb_present_bgra(padded,pitch,pitch*800,1280,800,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt));
 unsigned rows=((unsigned)setup.maximum_request_length*4-sizeof(xcb_put_image_request_t))/(1280*4);
 assert(tile_y==800&&receipt.image_requests==(800+rows-1)/rows&&receipt.image_bytes==1280*800*4);
 assert(receipt.source_y_inverted&&receipt.image_acknowledged&&receipt.gc_released&&map_count==maps_before);
 free(raw_large_expected);raw_large_expected=NULL;free(padded);
 raw_test=false;
 /* Real resource snapshot/writer gate -> exact X drawable -> reference swap.
  * X server/fences remain simulated; these are not GLX or SGX frames. */
 drawable_width=drawable_height=32;raw_test=drawable_test=true;
 struct pipe_resource back=r;back.reference.count=1;
 struct remesa_drawable_buffers buffers={.front=&r,.back=&back,.width=32,.height=32,.generation=1};
 for(unsigned frame=0;frame<4;frame++) {
  active=buffers.back;
  for(unsigned y=0;y<32;y++)for(unsigned x=0;x<128;x++)pixels[y*128+x]=(unsigned char)(frame+y*5+x);
  for(unsigned y=0;y<32;y++)memcpy(raw_expected+y*128,pixels+(31-y)*128,128);
  struct pipe_resource *previous_front=buffers.front,*previous_back=buffers.back;
  state=1;unsigned before_queries=queries;
  assert(remesa_xcb_present_drawable(&c,&buffers,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt)==EAGAIN);
  assert(queries==before_queries&&buffers.front==previous_front&&buffers.back==previous_back&&buffers.swaps==frame);
  state=0;
  fail=6;assert(remesa_xcb_present_drawable(&c,&buffers,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt)==EIO);
  assert(receipt.image_acknowledged&&!receipt.gc_released&&buffers.front==previous_front&&buffers.swaps==frame);
  fail=0;
  assert(!remesa_xcb_present_drawable(&c,&buffers,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt));
  assert(buffers.front==previous_back&&buffers.back==previous_front&&buffers.swaps==frame+1);
  assert(receipt.source_y_inverted&&r.reference.count==1&&back.reference.count==1);
 }
 buffers.swaps=UINT64_MAX;unsigned before_queries=queries;
 assert(remesa_xcb_present_drawable(&c,&buffers,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt)==EOVERFLOW&&queries==before_queries);
 buffers.swaps=0;
 assert(remesa_xcb_present_drawable(&c,&buffers,REMESA_XCB_BOTTOM_LEFT,connection,42,(void *)&buffers)==EINVAL);
 drawable_width=64;active=buffers.back;
 assert(remesa_xcb_present_drawable(&c,&buffers,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt)==ENOTSUP&&!buffers.swaps);
 drawable_test=raw_test=false;
 /* A frontend owns its own exchange: this publisher must never swap it. */
 drawable_width=32;raw_test=drawable_test=true;active=buffers.back;
 struct remesa_drawable_buffers unchanged=buffers;
 assert(!remesa_xcb_present_oriented(&c,buffers.back,REMESA_XCB_BOTTOM_LEFT,connection,42,&receipt));
 assert(receipt.source_y_inverted&&!memcmp(&unchanged,&buffers,sizeof(buffers)));
 assert(remesa_xcb_present_oriented(&c,buffers.back,(enum remesa_xcb_origin)99,connection,42,&receipt)==EINVAL);
 raw_test=drawable_test=false;
 assert(map_count==unmaps+2); /* one sticky EAGAIN and one EIO-fallback map */
 puts("PASS: fence-before-X ordering, checked PutImage, 12 failures, 2/10/100 publications, all10dimensions through1920x1080 tiled without scaling, connection-loss cleanup; raw BGRA32/1280x800 origin/padding/tiling/bounds/alias/ownership negatives; MOCK XCB ONLY");
 return 0;
}
