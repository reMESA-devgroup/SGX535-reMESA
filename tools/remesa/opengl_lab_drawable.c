/* SPDX-License-Identifier: MIT
 * Minimal real state-tracker attachment frontend. No admission/capability
 * override, renderer, X server connection, submission or implicit copy.
 */
#include "opengl_lab_drawable.h"
#include "remesa_screen.h"
#include "remesa_device.h"
#include "remesa_resource_ref.h"
#include "state_tracker/st_context.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
struct remesa_lab_drawable {
 struct pipe_frontend_drawable base;
 struct st_visual visual;
 struct remesa_drawable_buffers buffers;
 pthread_mutex_t lock;
 remesa_lab_publish publish;
 void *user;
 int error;
 bool double_buffer;
};
static bool valid_context(struct remesa_lab_drawable *d, struct st_context *st)
{ return st && st->pipe && st->pipe->screen==d->base.fscreen->screen; }
static bool validate(struct st_context *st, struct pipe_frontend_drawable *base,
 const enum st_attachment_type *atts, unsigned count, struct pipe_resource **out,
 struct pipe_resource **resolve)
{
 struct remesa_lab_drawable *d=(struct remesa_lab_drawable *)base;
 pthread_mutex_lock(&d->lock);
 int error=0;
 if(!valid_context(d,st)||!count||count>2||!atts||!out)error=EINVAL;
 /* Validate the complete request before changing any caller output. */
 for(unsigned i=0;!error&&i<count;i++) {
  if(atts[i]!=ST_ATTACHMENT_FRONT_LEFT &&
     !(atts[i]==ST_ATTACHMENT_BACK_LEFT&&d->double_buffer))error=ENOTSUP;
  for(unsigned j=0;j<i;j++)if(atts[i]==atts[j])error=EINVAL;
 }
 if(!error) {
  for(unsigned i=0;i<count;i++) {
   out[i]=NULL;remesa_resource_reference(&out[i],
    atts[i]==ST_ATTACHMENT_FRONT_LEFT?d->buffers.front:d->buffers.back);
  }
  if(resolve)*resolve=NULL;
 }
 d->error=error;pthread_mutex_unlock(&d->lock);return !error;
}
static bool present(struct st_context *st, struct remesa_lab_drawable *d, bool swap)
{
 pthread_mutex_lock(&d->lock);
 int error=0;
 if(!valid_context(d,st))error=EINVAL;
 else if(swap&&!d->double_buffer)error=ENOTSUP;
 else if(swap&&(d->buffers.swaps==UINT64_MAX||d->base.stamp==INT32_MAX))error=EOVERFLOW;
 else if(remesa_context_error(st->pipe))error=remesa_context_error(st->pipe);
 else error=d->publish(d->user,st->pipe,swap?d->buffers.back:d->buffers.front);
 if(!error&&swap) {
  struct pipe_resource *old=d->buffers.front;
  d->buffers.front=d->buffers.back;d->buffers.back=old;d->buffers.swaps++;
  __atomic_add_fetch(&d->base.stamp,1,__ATOMIC_RELEASE);
 }
 d->error=error;pthread_mutex_unlock(&d->lock);return !error;
}
static bool flush_front(struct st_context *st, struct pipe_frontend_drawable *base,
 enum st_attachment_type att)
{
 struct remesa_lab_drawable *d=(struct remesa_lab_drawable *)base;
 if(att!=ST_ATTACHMENT_FRONT_LEFT) {
  pthread_mutex_lock(&d->lock);d->error=ENOTSUP;pthread_mutex_unlock(&d->lock);return false;
 }
 return present(st,d,false);
}
static bool swap_buffers(struct st_context *st,struct pipe_frontend_drawable *base)
{ return present(st,(struct remesa_lab_drawable *)base,true); }
int remesa_lab_drawable_create(struct pipe_frontend_screen *frontend,uint32_t id,
 uint32_t width,uint32_t height,bool double_buffer,remesa_lab_publish publish,
 void *user,struct remesa_lab_drawable **out)
{
 if(!frontend||!frontend->screen||!id||!publish||!out||*out)return EINVAL;
 struct remesa_lab_drawable *d=calloc(1,sizeof(*d));if(!d)return ENOMEM;
 int error=pthread_mutex_init(&d->lock,NULL);if(error){free(d);return error;}
 error=remesa_drawable_buffers_replace(frontend->screen,width,height,double_buffer,false,&d->buffers);
 if(error){pthread_mutex_destroy(&d->lock);free(d);return error;}
 d->publish=publish;d->user=user;d->double_buffer=double_buffer;
 d->visual=(struct st_visual){.buffer_mask=ST_ATTACHMENT_FRONT_LEFT_MASK|
  (double_buffer?ST_ATTACHMENT_BACK_LEFT_MASK:0),
  .color_format=PIPE_FORMAT_B8G8R8A8_UNORM,.depth_stencil_format=PIPE_FORMAT_NONE};
 d->base=(struct pipe_frontend_drawable){.stamp=1,.ID=id,.fscreen=frontend,
  .visual=&d->visual,.validate=validate,.flush_front=flush_front,.flush_swapbuffers=swap_buffers};
 *out=d;return 0;
}
struct pipe_frontend_drawable *remesa_lab_drawable_base(struct remesa_lab_drawable *d)
{ return d?&d->base:NULL; }
int remesa_lab_drawable_resize(struct remesa_lab_drawable *d,uint32_t width,uint32_t height)
{
 if(!d)return EINVAL;
 pthread_mutex_lock(&d->lock);
 int error=d->base.stamp==INT32_MAX?EOVERFLOW:remesa_drawable_buffers_replace(
  d->base.fscreen->screen,width,height,d->double_buffer,false,&d->buffers);
 if(!error)__atomic_add_fetch(&d->base.stamp,1,__ATOMIC_RELEASE);
 d->error=error;pthread_mutex_unlock(&d->lock);return error;
}
int remesa_lab_drawable_error(struct remesa_lab_drawable *d)
{
 if(!d)return EINVAL;
 pthread_mutex_lock(&d->lock);int error=d->error;pthread_mutex_unlock(&d->lock);return error;
}
void remesa_lab_drawable_destroy(struct remesa_lab_drawable *d)
{
 if(!d)return;
 remesa_drawable_buffers_release(&d->buffers);pthread_mutex_destroy(&d->lock);free(d);
}
