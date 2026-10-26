/* CPU/mock publication only. Never opens a GPU/X display. */
#include "remesa_xcb_staging.h"
#define xcb_request_check xcb_request_check_legacy
#define xcb_put_image_checked xcb_put_image_checked_legacy
#define xcb_create_gc_checked xcb_create_gc_checked_legacy
#define xcb_free_gc_checked xcb_free_gc_checked_legacy
#define xcb_connection_has_error xcb_connection_has_error_legacy
#define xcb_get_geometry xcb_get_geometry_legacy
#define xcb_get_geometry_reply xcb_get_geometry_reply_legacy
#define xcb_get_window_attributes xcb_get_window_attributes_legacy
#define xcb_get_window_attributes_reply xcb_get_window_attributes_reply_legacy
#define main legacy_xcb_mock_tests
#include "test_xcb.c"
#undef main
#undef xcb_request_check
#undef xcb_put_image_checked
#undef xcb_create_gc_checked
#undef xcb_free_gc_checked
#undef xcb_connection_has_error
#undef xcb_get_geometry
#undef xcb_get_geometry_reply
#undef xcb_get_window_attributes
#undef xcb_get_window_attributes_reply
static unsigned realloc_calls;static int realloc_fail;
void *__real_realloc(void *,size_t);
void *__wrap_realloc(void *old,size_t n){++realloc_calls;if(realloc_fail){errno=ENOMEM;return NULL;}return __real_realloc(old,n);}
int remesa_fast_shm_present(xcb_connection_t *c,xcb_drawable_t w,xcb_gcontext_t gc,unsigned width,unsigned height,const void *p,size_t n,unsigned char *e,bool *sent,bool *ack){(void)c;(void)w;(void)gc;(void)width;(void)height;(void)p;(void)n;(void)e;(void)sent;(void)ack;assert(!"SHM not selected in this PUTIMAGE fixture");return EINVAL;}
int remesa_fast_shm_close(xcb_connection_t *c){(void)c;return 0;}
static int transfer_fault;
static void *bad_map(struct pipe_context *c,struct pipe_resource *r,unsigned level,unsigned usage,const struct pipe_box *box,struct pipe_transfer **out){
 void *p=map(c,r,level,usage,box,out);
 if(p&&transfer_fault==1)(*out)->stride=1;
 if(p&&transfer_fault==2)(*out)->resource=NULL;
 if(p&&transfer_fault==3){free(*out);*out=NULL;}
 return p;
}
int staging_base_tests(void){
 assert(!legacy_xcb_mock_tests());
 struct pipe_screen *s=(void *)pixels;struct pipe_context c={.screen=s,.texture_map=bad_map,.texture_unmap=unmap};
 struct pipe_resource r={.screen=s,.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=1280,.height0=800,.depth0=1,.array_size=1};r.reference.count=1;active=&r;
 xcb_connection_t *connection=(void *)&r;struct remesa_xcb_receipt receipt;struct remesa_xcb_staging staging={0};
 fail=state=0;raw_test=drawable_test=true;drawable_width=1280;drawable_height=800;large_stride=1280*4+32;
 large_pixels=malloc((size_t)large_stride*800);raw_large_expected=malloc(4096000);assert(large_pixels&&raw_large_expected);
 for(unsigned y=0;y<800;y++)for(unsigned x=0;x<large_stride;x++)large_pixels[(size_t)y*large_stride+x]=(unsigned char)(y*7+x*3);
 for(unsigned frame=0;frame<120;frame++){
  enum remesa_xcb_origin origin=frame&1?REMESA_XCB_BOTTOM_LEFT:REMESA_XCB_TOP_LEFT;
  for(unsigned y=0;y<800;y++)memcpy(raw_large_expected+y*5120,large_pixels+(origin==REMESA_XCB_BOTTOM_LEFT?799-y:y)*large_stride,5120);
  unsigned before=realloc_calls;
  assert(!remesa_xcb_present_oriented_staged(&c,&r,origin,connection,42,&staging,&receipt));
  assert(realloc_calls==before+(frame==0));assert(staging.capacity==4096000&&r.reference.count==1&&receipt.image_bytes==4096000&&receipt.snapshot_bytes==4096000&&receipt.source_y_inverted==(origin==REMESA_XCB_BOTTOM_LEFT)&&receipt.image_acknowledged&&receipt.gc_released&&tile_y==800);
 }
 void *saved=staging.pixels;size_t capacity=staging.capacity;
 for(state=1;state<=2;state++){
  unsigned before=queries;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_TOP_LEFT,connection,42,&staging,&receipt)==(state==1?EAGAIN:EIO));assert(queries==before&&r.reference.count==1);
 }state=0;
 for(transfer_fault=1;transfer_fault<=3;transfer_fault++){
  unsigned before=queries;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_TOP_LEFT,connection,42,&staging,&receipt)==EPROTO);assert(queries==before&&r.reference.count==1);
 }transfer_fault=0;
 for(unsigned y=0;y<800;y++)memcpy(raw_large_expected+y*5120,large_pixels+y*large_stride,5120);
 for(fail=1;fail<=12;fail++){
  unsigned before=puts_;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_TOP_LEFT,connection,42,&staging,&receipt));assert(r.reference.count==1);if(fail==7)assert(puts_==before);assert(staging.pixels==saved&&staging.capacity==capacity);
 }fail=0;
 r.width0=2560;realloc_fail=1;unsigned before=queries;
 assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_TOP_LEFT,connection,42,&staging,&receipt)==ENOMEM);assert(staging.pixels==saved&&staging.capacity==capacity&&queries==before&&r.reference.count==1);realloc_fail=0;r.width0=1280;
 r.format=PIPE_FORMAT_Z24X8_UNORM;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_TOP_LEFT,connection,42,&staging,&receipt)==EINVAL);r.format=PIPE_FORMAT_B8G8R8A8_UNORM;
 assert(remesa_xcb_present_oriented_staged(&c,&r,(enum remesa_xcb_origin)99,connection,42,&staging,&receipt)==EINVAL);
 remesa_xcb_staging_release(&staging);assert(!staging.pixels&&!staging.capacity);remesa_xcb_staging_release(&staging);
 free(large_pixels);free(raw_large_expected);large_pixels=raw_large_expected=NULL;
 puts("PASS: production staged 120 padded1280x800 TOP/BOTTOM frames match old pixel contract, one allocation, writer/map failures beforeX,12 X failures, allocation failure transactional, close idempotent; MOCK CPU ONLY GPU0");return 0;
}
