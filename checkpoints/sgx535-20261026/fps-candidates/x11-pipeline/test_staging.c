/* Actual publisher; simulated XCB/Gallium only. No GPU or X server access. */
#include "test_staging_base.inc"
static bool batch_mode,serial_guard,fatal_state,release_attempted,release_checked;
static unsigned geometry_sent,attributes_sent,geometry_replies,attributes_replies,geometry_error,attributes_error;
static bool geometry_null,attributes_null,release_zero;
static unsigned serial_image_checks,queued_count,drained_count,send_attempts,implicit_syncs;
static uint32_t next_sequence,create_cookie,free_cookie,queued[64];
static bool drained[64];static unsigned char image_errors[64];
static int zero_at=-1,fatal_after_send=-1,fatal_check=-1;
static unsigned expected_queued,create_error,gc_error;
static void *error_allocations[128];static unsigned error_allocated,error_freed;
static struct remesa_xcb_receipt *current_receipt;
void __real_free(void *);
void __wrap_free(void *p){if(p)for(unsigned i=0;i<error_allocated;i++)if(error_allocations[i]==p){error_allocations[i]=NULL;++error_freed;break;}__real_free(p);}
static xcb_generic_error_t *error_result(unsigned code){
 if(!code)return NULL;xcb_generic_error_t *p=calloc(1,sizeof(*p));assert(p&&error_allocated<128);p->error_code=code;error_allocations[error_allocated++]=p;return p;
}
static uint32_t sequence(void){++next_sequence;if(!next_sequence)++next_sequence;return next_sequence;}
xcb_get_geometry_cookie_t xcb_get_geometry(xcb_connection_t *c,xcb_drawable_t w){
 if(batch_mode){assert(!geometry_sent&&!attributes_sent);++geometry_sent;}
 return xcb_get_geometry_legacy(c,w);
}
xcb_get_window_attributes_cookie_t xcb_get_window_attributes(xcb_connection_t *c,xcb_window_t w){
 if(batch_mode){assert(geometry_sent==1&&!geometry_replies&&!attributes_sent);++attributes_sent;}
 return xcb_get_window_attributes_legacy(c,w);
}
xcb_get_geometry_reply_t *xcb_get_geometry_reply(xcb_connection_t *c,xcb_get_geometry_cookie_t cookie,xcb_generic_error_t **e){
 if(batch_mode){assert(geometry_sent==1&&attributes_sent==1&&!geometry_replies);++geometry_replies;if(geometry_error||geometry_null){*e=error_result(geometry_error);return NULL;}}
 return xcb_get_geometry_reply_legacy(c,cookie,e);
}
xcb_get_window_attributes_reply_t *xcb_get_window_attributes_reply(xcb_connection_t *c,xcb_get_window_attributes_cookie_t cookie,xcb_generic_error_t **e){
 if(batch_mode){assert(geometry_sent==1&&attributes_sent==1&&geometry_replies==1&&!attributes_replies);++attributes_replies;if(attributes_error||attributes_null){*e=error_result(attributes_error);return NULL;}}
 return xcb_get_window_attributes_reply_legacy(c,cookie,e);
}
int xcb_connection_has_error(xcb_connection_t *c){return batch_mode?fatal_state:xcb_connection_has_error_legacy(c);}
xcb_void_cookie_t xcb_create_gc_checked(xcb_connection_t *c,xcb_gcontext_t gc,xcb_drawable_t window,uint32_t mask,const void *values){
 xcb_void_cookie_t old=xcb_create_gc_checked_legacy(c,gc,window,mask,values);if(!batch_mode)return old;create_cookie=sequence();return (xcb_void_cookie_t){create_cookie};
}
xcb_void_cookie_t xcb_put_image_checked(xcb_connection_t *c,uint8_t f,xcb_drawable_t d,xcb_gcontext_t g,uint16_t w,uint16_t h,int16_t x,int16_t y,uint8_t pad,uint8_t dep,uint32_t n,const uint8_t *p){
 xcb_void_cookie_t old=xcb_put_image_checked_legacy(c,f,d,g,w,h,x,y,pad,dep,n,p);if(!batch_mode)return old;
 assert(!drained_count&&!current_receipt->image_acknowledged&&queued_count<64);
 unsigned attempt=send_attempts++;if((int)attempt==zero_at)return (xcb_void_cookie_t){0};
 queued[queued_count++]=sequence();if((int)attempt==fatal_after_send)fatal_state=true;
 return (xcb_void_cookie_t){queued[queued_count-1]};
}
xcb_void_cookie_t xcb_free_gc_checked(xcb_connection_t *c,xcb_gcontext_t gc){
 xcb_void_cookie_t old=xcb_free_gc_checked_legacy(c,gc);if(!batch_mode)return old;
 assert(!drained_count&&!release_attempted&&queued_count==expected_queued);release_attempted=true;
 if(fatal_state||release_zero)return (xcb_void_cookie_t){0};free_cookie=sequence();return (xcb_void_cookie_t){free_cookie};
}
xcb_generic_error_t *xcb_request_check(xcb_connection_t *c,xcb_void_cookie_t cookie){
 if(!batch_mode){if(serial_guard&&cookie.sequence==4){assert(puts_==serial_image_checks+1);++serial_image_checks;}return xcb_request_check_legacy(c,cookie);}
 assert(active->reference.count==2);
 if(cookie.sequence==create_cookie){assert(!send_attempts);return error_result(create_error);}
 if(cookie.sequence==free_cookie){assert(release_attempted&&!release_checked&&!drained_count);release_checked=true;++implicit_syncs;return error_result(gc_error);}
 unsigned index=0;while(index<queued_count&&queued[index]!=cookie.sequence)++index;assert(index<queued_count&&!drained[index]);
 assert(queued_count==expected_queued&&!current_receipt->image_acknowledged);
 assert(release_attempted&&(!free_cookie||release_checked)&&index==drained_count);
 if((int)drained_count==fatal_check)fatal_state=true;
 drained[index]=true;++drained_count;
 if(fatal_state)return NULL;return error_result(image_errors[index]);
}
static void reset_case(unsigned expected){
 batch_mode=true;serial_guard=fatal_state=release_attempted=release_checked=false;
 geometry_sent=attributes_sent=geometry_replies=attributes_replies=geometry_error=attributes_error=0;geometry_null=attributes_null=release_zero=false;serial_image_checks=queued_count=drained_count=send_attempts=implicit_syncs=0;create_cookie=free_cookie=0;next_sequence=100;
 memset(queued,0,sizeof(queued));memset(drained,0,sizeof(drained));memset(image_errors,0,sizeof(image_errors));zero_at=fatal_after_send=fatal_check=-1;expected_queued=expected;create_error=gc_error=0;error_allocated=error_freed=0;memset(error_allocations,0,sizeof(error_allocations));puts_=creates=frees=checks=queries=0;fail=state=transfer_fault=0;
}
static void expected_pixels(unsigned width,unsigned height,unsigned frame){
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width*4;x++)large_pixels[(size_t)y*large_stride+x]=(unsigned char)(frame+y*7+x*3);
 for(unsigned y=0;y<height;y++)memcpy(raw_large_expected+(size_t)y*width*4,large_pixels+(size_t)(height-1-y)*large_stride,width*4);
}
int main(void){
 unsetenv("REMESA_GEARS_PROFILE");unsetenv("REMESA_GEARS_PRESENT");assert(!staging_base_tests());
 struct pipe_screen *s=(void *)pixels;struct pipe_context c={.screen=s,.texture_map=bad_map,.texture_unmap=unmap};
 struct pipe_resource r={.screen=s,.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=1280,.height0=800,.depth0=1,.array_size=1};r.reference.count=1;active=&r;
 xcb_connection_t *connection=(void *)&r;struct remesa_xcb_receipt receipt;struct remesa_xcb_staging staging={0};current_receipt=&receipt;
 raw_test=drawable_test=true;drawable_width=1280;drawable_height=800;large_stride=1280*4+32;setup.maximum_request_length=65535;
 large_pixels=malloc((size_t)large_stride*800);raw_large_expected=malloc(4096000);assert(large_pixels&&raw_large_expected);
 setenv("REMESA_GEARS_PROFILE","FAST",1);setenv("REMESA_GEARS_PRESENT","PUTIMAGE",1);unsigned before_realloc=realloc_calls;
 for(unsigned frame=0;frame<120;frame++){
  reset_case(16);expected_pixels(1280,800,frame);
  assert(!remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt));
  assert(queued_count==16&&drained_count==16&&implicit_syncs==1&&receipt.image_requests==16&&receipt.image_acknowledged&&receipt.gc_released&&r.reference.count==1&&realloc_calls==before_realloc+1);
 }
 /* Both mandatory replies must be drained even when either/both fail. */
 for(unsigned which=0;which<5;which++){
  reset_case(0);
  if(which==0)geometry_error=XCB_WINDOW;
  if(which==1)attributes_error=XCB_MATCH;
  if(which==2){geometry_error=XCB_WINDOW;attributes_error=XCB_MATCH;}
  if(which==3)geometry_null=true;
  if(which==4)attributes_null=true;
  assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EIO);
  assert(geometry_replies==1&&attributes_replies==1&&!creates&&!puts_&&!frees&&error_allocated==error_freed&&r.reference.count==1);
  if(geometry_error)assert(receipt.x_error==XCB_WINDOW);
  else if(attributes_error)assert(receipt.x_error==XCB_MATCH);
 }
 /* Every position can report an error: all16 cookies still consumed. */
 for(unsigned bad=0;bad<16;bad++){
  reset_case(16);image_errors[bad]=XCB_WINDOW;
  assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EIO);
  assert(drained_count==16&&error_allocated==1&&error_freed==1&&!receipt.image_acknowledged&&receipt.gc_released&&receipt.x_error==XCB_WINDOW&&r.reference.count==1);
 }
 reset_case(16);image_errors[0]=XCB_WINDOW;image_errors[8]=XCB_G_CONTEXT;image_errors[15]=XCB_MATCH;
 assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EIO);assert(drained_count==16&&error_allocated==3&&error_freed==3&&receipt.x_error==XCB_WINDOW&&!receipt.image_acknowledged&&receipt.gc_released);
 for(unsigned bad=0;bad<16;bad+=bad?14:1){
  reset_case(bad);zero_at=bad;
  assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EPROTO);
  assert(drained_count==bad&&queued_count==bad&&send_attempts==bad+1&&receipt.image_requests==bad&&!receipt.image_acknowledged&&receipt.gc_released&&r.reference.count==1);
 }
 reset_case(4);fatal_after_send=3;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EPIPE);assert(drained_count==4&&!receipt.image_acknowledged&&!receipt.gc_released&&r.reference.count==1);
 reset_case(16);fatal_check=0;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EPIPE);assert(drained_count==16&&!receipt.image_acknowledged&&receipt.gc_released&&r.reference.count==1);
 reset_case(0);create_error=XCB_G_CONTEXT;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EIO);assert(!queued_count&&!frees&&error_allocated==error_freed&&r.reference.count==1);
 reset_case(16);gc_error=XCB_G_CONTEXT;assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EIO);assert(drained_count==16&&receipt.image_acknowledged&&!receipt.gc_released&&error_allocated==error_freed&&r.reference.count==1);
 /* FreeGC error must not hide the earlier image error; consume both. */
 reset_case(16);image_errors[7]=XCB_WINDOW;gc_error=XCB_G_CONTEXT;
 assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EIO);
 assert(drained_count==16&&release_checked&&!receipt.image_acknowledged&&!receipt.gc_released&&receipt.x_error==XCB_WINDOW&&error_allocated==2&&error_freed==2&&r.reference.count==1);
 reset_case(16);next_sequence=UINT32_MAX-8;assert(!remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt));assert(drained_count==16&&receipt.image_acknowledged&&r.reference.count==1);
 /* FreeGC zero cookie cannot be retried or mistaken for release. All images
  * are still drained, while the held framebuffer survives every check. */
 reset_case(16);release_zero=true;
 assert(remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt)==EPROTO);
 assert(drained_count==16&&frees==1&&receipt.image_acknowledged&&!receipt.gc_released&&r.reference.count==1);
 /* This offset wraps exactly at the queued FreeGC, after16 images. */
 reset_case(16);next_sequence=UINT32_MAX-17;
 assert(!remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt));
 assert(free_cookie==1&&release_checked&&drained_count==16&&receipt.gc_released&&r.reference.count==1);
 /* Batch cap64 and fallback65: no heap cookie allocation; staging reused. */
 r.width0=drawable_width=32;r.height0=drawable_height=64;setup.maximum_request_length=(sizeof(xcb_put_image_request_t)+128)/4;expected_pixels(32,64,0);reset_case(64);
 assert(!remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt));assert(queued_count==64&&drained_count==64&&implicit_syncs==1&&receipt.image_requests==64);
 r.height0=drawable_height=65;expected_pixels(32,65,0);reset_case(0);batch_mode=false;serial_guard=true;
 assert(!remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt));assert(puts_==65&&serial_image_checks==65&&receipt.image_requests==65&&receipt.image_acknowledged&&receipt.gc_released);
 /* LAB keeps send/check interleaving even for a batch-sized normal frame. */
 unsetenv("REMESA_GEARS_PROFILE");r.width0=drawable_width=1280;r.height0=drawable_height=800;setup.maximum_request_length=65535;expected_pixels(1280,800,1);reset_case(0);batch_mode=false;serial_guard=true;
 assert(!remesa_xcb_present_oriented_staged(&c,&r,REMESA_XCB_BOTTOM_LEFT,connection,42,&staging,&receipt));assert(puts_==16&&serial_image_checks==16&&receipt.image_acknowledged&&receipt.gc_released);
 assert(realloc_calls==before_realloc+1);remesa_xcb_staging_release(&staging);assert(!staging.pixels&&!staging.capacity);free(large_pixels);free(raw_large_expected);large_pixels=raw_large_expected=NULL;
 puts("PASS checked X11 batch: legacy/staging regressions;120x16 chunks queued before checks,FreeGC final barrier then ALL image cookies in wire order,16 error positions,multierror freed,partialzero/fatalconnection/create/free errors,sequencewrap,cap64/fallback65,LAB serial,one stagingallocation/resource lifetime; paired-query errors drained; MOCK CPU/GPU0");return 0;
}
