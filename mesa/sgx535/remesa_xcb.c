/* SPDX-License-Identifier: MIT */
#include "remesa_xcb.h"
#include "remesa_present.h"
#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "remesa_resource_ref.h"
#include "remesa_screen.h"
#include <xcb/xcb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
static int checked(xcb_connection_t *connection,xcb_void_cookie_t cookie,struct remesa_xcb_receipt *r)
{
 if(!cookie.sequence) return EPROTO;
 xcb_generic_error_t *error=xcb_request_check(connection,cookie);
 if(error) {r->x_error=error->error_code;free(error);return EIO;}
 return xcb_connection_has_error(connection) ? EPIPE : 0;
}
static bool compatible(const xcb_setup_t *setup,const xcb_get_geometry_reply_t *geometry,const xcb_get_window_attributes_reply_t *attributes)
{
 bool pixels=false;
 if(!setup || setup->image_byte_order!=XCB_IMAGE_ORDER_LSB_FIRST || geometry->depth!=24 || attributes->_class!=XCB_WINDOW_CLASS_INPUT_OUTPUT) return false;
 for(xcb_format_iterator_t f=xcb_setup_pixmap_formats_iterator(setup);f.rem;xcb_format_next(&f))
  if(f.data->depth==24 && f.data->bits_per_pixel==32 && f.data->scanline_pad==32) pixels=true;
 if(!pixels) return false;
 for(xcb_screen_iterator_t s=xcb_setup_roots_iterator(setup);s.rem;xcb_screen_next(&s)) {
  if(s.data->root!=geometry->root) continue;
  for(xcb_depth_iterator_t d=xcb_screen_allowed_depths_iterator(s.data);d.rem;xcb_depth_next(&d)) {
   if(d.data->depth!=24) continue;
   for(xcb_visualtype_iterator_t v=xcb_depth_visuals_iterator(d.data);v.rem;xcb_visualtype_next(&v))
    if(v.data->visual_id==attributes->visual && v.data->_class==XCB_VISUAL_CLASS_TRUE_COLOR &&
       v.data->red_mask==0xff0000 && v.data->green_mask==0xff00 && v.data->blue_mask==0xff) return true;
  }
 }
 return false;
}
static int publish(xcb_connection_t *connection,uint32_t window,
 uint16_t width,uint16_t height,bool exact,const unsigned char *pixels,
 struct remesa_xcb_receipt *receipt)
{
 size_t stride=(size_t)width*4;int error;uint32_t gc=0;
 xcb_get_geometry_reply_t *geometry=NULL;xcb_get_window_attributes_reply_t *attributes=NULL;xcb_generic_error_t *xerror=NULL;
 if(xcb_connection_has_error(connection)) {error=EPIPE;goto done;}
 geometry=xcb_get_geometry_reply(connection,xcb_get_geometry(connection,window),&xerror);
 if(!geometry || xerror) {if(xerror)receipt->x_error=xerror->error_code;error=EIO;goto done;}
 attributes=xcb_get_window_attributes_reply(connection,xcb_get_window_attributes(connection,window),&xerror);
 if(!attributes || xerror) {if(xerror)receipt->x_error=xerror->error_code;error=EIO;goto done;}
 if(window==geometry->root || (exact?(geometry->width!=width||geometry->height!=height):
    (geometry->width<width||geometry->height<height)) ||
    !compatible(xcb_get_setup(connection),geometry,attributes)) {error=ENOTSUP;goto done;}
 /* Avoid implicit big-request setup/extra operations: use server's base limit. */
 const xcb_setup_t *setup=xcb_get_setup(connection);
 /* Accept only the caller connection's initial XID namespace. Imported or
  * XC-MISC-reassigned windows need a separately designed ownership contract. */
 if(!setup->resource_id_mask || (window & ~setup->resource_id_mask)!=setup->resource_id_base) {error=EACCES;goto done;}
 size_t limit=(size_t)setup->maximum_request_length*4;
 if(limit<=sizeof(xcb_put_image_request_t)||stride>limit-sizeof(xcb_put_image_request_t)) {error=E2BIG;goto done;}
 /* Direct frames need bounded requests just like scaled frames. A 320x240
  * BGRA snapshot already exceeds the common base request limit. Splitting
  * rows changes only CPU publication, never SGX rendering or scanout. */
 size_t tile_rows=(limit-sizeof(xcb_put_image_request_t))/stride;
 gc=xcb_generate_id(connection);if(gc==UINT32_MAX || !gc) {gc=0;error=ENOSPC;goto done;}
 error=checked(connection,xcb_create_gc_checked(connection,gc,window,0,NULL),receipt);
 if(error) {gc=0;goto done;}
 for(unsigned y=0;y<height;){
  unsigned rows=height-y;if(rows>tile_rows)rows=tile_rows;
  xcb_void_cookie_t image=xcb_put_image_checked(connection,XCB_IMAGE_FORMAT_Z_PIXMAP,window,gc,
      width,rows,0,y,0,24,rows*stride,pixels+y*stride);
  if(image.sequence){receipt->image_request_sent=true;++receipt->image_requests;}
  error=checked(connection,image,receipt);if(error)break;
  y+=rows;
 }
 if(!error) receipt->image_acknowledged=true;
 done:
 if(gc) {
  int cleanup=checked(connection,xcb_free_gc_checked(connection,gc),receipt);
  if(!cleanup) receipt->gc_released=true;
  if(!error) error=cleanup;
 }
 free(xerror);free(attributes);free(geometry);return error;
}
static int present(struct pipe_context *ctx,struct pipe_resource *resource,xcb_connection_t *connection,uint32_t window,
 uint16_t width,uint16_t height,bool scaled,enum remesa_present_scale_mode mode,
 enum remesa_xcb_origin origin,bool exact,struct remesa_xcb_receipt *receipt)
{
 unsigned char *pixels;size_t bytes,stride;int error;
 struct pipe_resource *held=NULL;
 if(!receipt) return EINVAL;
 memset(receipt,0,sizeof(*receipt));
 if(!ctx || !resource || !connection || !window || !resource->width0 || !resource->height0 ||
    resource->width0>UINT16_MAX||!width||!height||height>INT16_MAX) return EINVAL;
 if((uint64_t)resource->width0*resource->height0*4>UINT32_MAX)return EOVERFLOW;
 stride=(size_t)width*4;bytes=stride*height;
 if(bytes>UINT32_MAX || bytes/height!=stride) return EOVERFLOW;
 pixels=malloc(bytes);if(!pixels) return ENOMEM;
 /* The caller serializes the context/connection. Hold the framebuffer through
  * checked server acknowledgement and GC cleanup, including error paths. */
 remesa_resource_reference(&held,resource);
 /* Poll the Gallium writer before any server operation. Never bypass readback. */
 error=scaled?remesa_present_scaled_snapshot(ctx,resource,pixels,stride,bytes,width,height,mode,&receipt->scale):
  remesa_present_snapshot(ctx,resource,pixels,stride,bytes);if(error) goto done;
 receipt->snapshot_bytes=scaled?receipt->scale.source_bytes:bytes;
 receipt->image_bytes=bytes;
 if(origin==REMESA_XCB_BOTTOM_LEFT) {
  for(unsigned y=0;y<height/2;y++)for(size_t x=0;x<stride;x++) {
   unsigned char *a=pixels+y*stride+x,*b=pixels+(height-1-y)*stride+x;
   unsigned char tmp=*a;*a=*b;*b=tmp;
  }
  receipt->source_y_inverted=true;
 }
 error=publish(connection,window,width,height,exact,pixels,receipt);
 done:
 free(pixels);remesa_resource_reference(&held,NULL);return error;
}

int remesa_xcb_present_bgra(const void *source,size_t source_stride,size_t capacity,
 uint32_t width,uint32_t height,enum remesa_xcb_origin origin,
 xcb_connection_t *connection,uint32_t window,struct remesa_xcb_receipt *receipt)
{
 /* Reject malformed input before touching receipt, source or X connection. */
 if(!receipt||!source||!connection||!window||!width||width>UINT16_MAX||
    !height||height>INT16_MAX||
    (origin!=REMESA_XCB_TOP_LEFT&&origin!=REMESA_XCB_BOTTOM_LEFT))return EINVAL;
 size_t row=(size_t)width*4;
 if(source_stride<row)return EINVAL;
 if(height>1&&source_stride>(SIZE_MAX-row)/(height-1))return EOVERFLOW;
 size_t span=(height-1)*source_stride+row;
 if(capacity<span)return ENOSPC;
 if(row>UINT32_MAX/height)return EOVERFLOW;
 uintptr_t begin=(uintptr_t)source,r=(uintptr_t)receipt;
 if(begin>UINTPTR_MAX-span||r>UINTPTR_MAX-sizeof(*receipt)||
    (begin<r+sizeof(*receipt)&&r<begin+span))return EINVAL;
 memset(receipt,0,sizeof(*receipt));
 size_t bytes=row*height;unsigned char *pixels=malloc(bytes);
 if(!pixels)return ENOMEM;
 for(unsigned y=0;y<height;y++) {
  unsigned sy=origin==REMESA_XCB_BOTTOM_LEFT?height-1-y:y;
  memcpy(pixels+y*row,(const unsigned char *)source+sy*source_stride,row);
 }
 receipt->source_y_inverted=origin==REMESA_XCB_BOTTOM_LEFT;
 receipt->snapshot_bytes=bytes;receipt->image_bytes=bytes;
 int error=publish(connection,window,width,height,true,pixels,receipt);
 free(pixels);return error;
}

int remesa_xcb_present(struct pipe_context *ctx,struct pipe_resource *resource,xcb_connection_t *connection,uint32_t window,struct remesa_xcb_receipt *receipt)
{
 if(!resource){if(receipt)memset(receipt,0,sizeof(*receipt));return EINVAL;}
 if(resource->width0>UINT16_MAX||resource->height0>INT16_MAX){if(receipt)memset(receipt,0,sizeof(*receipt));return EINVAL;}
 return present(ctx,resource,connection,window,resource->width0,resource->height0,false,REMESA_PRESENT_STRETCH,REMESA_XCB_TOP_LEFT,false,receipt);
}
int remesa_xcb_present_scaled(struct pipe_context *ctx,struct pipe_resource *resource,xcb_connection_t *connection,
 uint32_t window,uint16_t width,uint16_t height,enum remesa_present_scale_mode mode,struct remesa_xcb_receipt *receipt)
{ return present(ctx,resource,connection,window,width,height,true,mode,REMESA_XCB_TOP_LEFT,true,receipt); }
int remesa_xcb_present_drawable(struct pipe_context *ctx,struct remesa_drawable_buffers *buffers,
 enum remesa_xcb_origin origin,xcb_connection_t *connection,uint32_t window,struct remesa_xcb_receipt *receipt)
{
 if(!ctx||!buffers||!buffers->front||!buffers->generation||!receipt||
    buffers->front==buffers->back||
    (origin!=REMESA_XCB_TOP_LEFT&&origin!=REMESA_XCB_BOTTOM_LEFT))return EINVAL;
 if(buffers->swaps==UINT64_MAX)return EOVERFLOW;
 struct pipe_resource *source=buffers->back?buffers->back:buffers->front;
 if(buffers->front->screen!=ctx->screen||source->screen!=ctx->screen||
    buffers->front->width0!=buffers->width||buffers->front->height0!=buffers->height||
    source->width0!=buffers->width||source->height0!=buffers->height||
    !buffers->width||buffers->width>UINT16_MAX||!buffers->height||buffers->height>INT16_MAX)return EINVAL;
 uintptr_t b=(uintptr_t)buffers,r=(uintptr_t)receipt;
 if(b>UINTPTR_MAX-sizeof(*buffers)||r>UINTPTR_MAX-sizeof(*receipt)||
    (b<r+sizeof(*receipt)&&r<b+sizeof(*buffers)))return EINVAL;
 int error=present(ctx,source,connection,window,buffers->width,buffers->height,
  false,REMESA_PRESENT_STRETCH,origin,true,receipt);
 if(error)return error;
 if(buffers->back){buffers->back=buffers->front;buffers->front=source;}
 ++buffers->swaps;return 0;
}
int remesa_xcb_present_oriented(struct pipe_context *ctx,struct pipe_resource *resource,
 enum remesa_xcb_origin origin,xcb_connection_t *connection,uint32_t window,struct remesa_xcb_receipt *receipt)
{
 if(!resource||!resource->width0||resource->width0>UINT16_MAX||
    !resource->height0||resource->height0>INT16_MAX||
    (origin!=REMESA_XCB_TOP_LEFT&&origin!=REMESA_XCB_BOTTOM_LEFT))return EINVAL;
 return present(ctx,resource,connection,window,resource->width0,resource->height0,
  false,REMESA_PRESENT_STRETCH,origin,true,receipt);
}
int remesa_xcb_request_fullscreen(xcb_connection_t *connection,uint32_t window,bool enabled,struct remesa_xcb_receipt *receipt)
{
 xcb_generic_error_t *xerror=NULL;xcb_get_geometry_reply_t *geometry=NULL;
 xcb_get_window_attributes_reply_t *attributes=NULL;
 xcb_intern_atom_reply_t *state=NULL,*fullscreen=NULL;int error=0;
 if(!receipt)return EINVAL;
 memset(receipt,0,sizeof(*receipt));
 if(!connection||!window)return EINVAL;
 if(xcb_connection_has_error(connection))return EPIPE;
 const xcb_setup_t *setup=xcb_get_setup(connection);
 if(!setup||!setup->resource_id_mask||(window&~setup->resource_id_mask)!=setup->resource_id_base)return EACCES;
 geometry=xcb_get_geometry_reply(connection,xcb_get_geometry(connection,window),&xerror);
 if(!geometry||xerror){error=EIO;goto done;}
 if(window==geometry->root){error=ENOTSUP;goto done;}
 attributes=xcb_get_window_attributes_reply(connection,xcb_get_window_attributes(connection,window),&xerror);
 if(!attributes||xerror){error=EIO;goto done;}
 if(attributes->_class!=XCB_WINDOW_CLASS_INPUT_OUTPUT){error=ENOTSUP;goto done;}
 state=xcb_intern_atom_reply(connection,xcb_intern_atom(connection,0,sizeof("_NET_WM_STATE")-1,"_NET_WM_STATE"),&xerror);
 if(!state||!state->atom||xerror){error=EIO;goto done;}
 fullscreen=xcb_intern_atom_reply(connection,xcb_intern_atom(connection,0,sizeof("_NET_WM_STATE_FULLSCREEN")-1,"_NET_WM_STATE_FULLSCREEN"),&xerror);
 if(!fullscreen||!fullscreen->atom||xerror){error=EIO;goto done;}
 xcb_client_message_event_t event={0};event.response_type=XCB_CLIENT_MESSAGE;event.format=32;
 event.window=window;event.type=state->atom;event.data.data32[0]=enabled?1:0;
 event.data.data32[1]=fullscreen->atom;event.data.data32[3]=1;
 xcb_void_cookie_t request=xcb_send_event_checked(connection,0,geometry->root,
  XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,(const char *)&event);
 receipt->fullscreen_request_sent=request.sequence!=0;error=checked(connection,request,receipt);
 receipt->fullscreen_request_acknowledged=!error;
 done:
 if(xerror)receipt->x_error=xerror->error_code;
 free(xerror);free(attributes);free(geometry);free(state);free(fullscreen);return error;
}
