/* SPDX-License-Identifier: MIT */
#include "remesa_loader.h"
#include "remesa_screen.h"
#include <errno.h>
#include <stddef.h>

#ifdef REMESA_WITH_MESA_HEADERS
#include "remesa_persistent.h"
#include "remesa_linux_memory.h"
#include "remesa_dri.h"
#include "pipe/p_screen.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
struct remesa_native_2d_failure {
 struct remesa_device device;
 struct remesa_linux_memory *memory;
 int fd,cleanup_error;
 bool provider_owned,uncertain_open;bool *release_completed;
};
static int native_2d_release(void *data)
{
 struct remesa_native_2d_failure *owner=data;int error;
 if(owner->cleanup_error)return owner->cleanup_error;
 if(owner->uncertain_open)return EOWNERDEAD;
 if(owner->memory){
  error=remesa_linux_memory_detach(owner->memory);
  if(error){owner->cleanup_error=error;return error;}
  owner->memory=NULL;
 }
 /* Linux close is never retried, including EINTR: the descriptor may already
  * have been closed. No later cleanup may act on a reused numeric fd. */
 if(owner->fd>=0){int fd=owner->fd;owner->fd=-1;
  if(close(fd)){owner->cleanup_error=EOWNERDEAD;return EOWNERDEAD;}}
 if(owner->release_completed)*owner->release_completed=true;
 free(owner);return 0;
}
static int native_2d_cleanup(struct remesa_native_2d_failure *owner)
{
 if(owner->device.priv){
  /* release_provider may free owner. Close a stack copy, never then memset
   * an embedded device whose containing owner has just been released. */
  struct remesa_device device=owner->device;
  bool owned=owner->provider_owned;int error=remesa_persistent_close(&device);
  if(error||owned)return error;
  owner->device=(struct remesa_device){0};
 }
 return native_2d_release(owner);
}
struct pipe_screen *remesa_loader_native_2d_screen(int borrowed_fd,
 struct remesa_native_2d_report *report)
{
 struct remesa_native_2d_failure *owner;struct pipe_screen *screen=NULL;
 struct remesa_dri_identity identity;int error,flags;
 if(!report){errno=EINVAL;return NULL;}
 *report=(struct remesa_native_2d_report){0};
 flags=fcntl(borrowed_fd,F_GETFL);
 if(flags<0){report->error=errno;return NULL;}
 if((flags&O_ACCMODE)!=O_RDWR){report->error=EACCES;errno=EACCES;return NULL;}
 owner=calloc(1,sizeof(*owner));if(!owner){report->error=ENOMEM;return NULL;}
 owner->fd=fcntl(borrowed_fd,F_DUPFD_CLOEXEC,0);
 if(owner->fd<0){error=errno;free(owner);report->error=error;return NULL;}
 flags=fcntl(owner->fd,F_GETFL);
 if(flags<0){error=errno;goto fail;}
 if((flags&O_ACCMODE)!=O_RDWR){error=EACCES;goto fail;}
 error=remesa_dri_probe_fd(owner->fd,&identity);if(error)goto fail;
 error=remesa_linux_memory_begin(owner->fd,&owner->memory);
 if(error){owner->uncertain_open=error==EOWNERDEAD;goto fail;}
 const struct remesa_persistent_config cfg={.ops=remesa_linux_memory_ops(),.priv=owner->memory,
  .context=remesa_linux_memory_context(owner->memory),.va_start=0x40001000,.va_end=0x42000000,
  .byte_budget=32u*1024u*1024u,.resource_only_backend=true,
  .resource_limits={4096,4096,4,16u*1024u*1024u}};
 error=remesa_persistent_open(&owner->device,&cfg);if(error)goto fail;
 error=remesa_persistent_set_native_copy(&owner->device,remesa_linux_memory_native_copy);if(error)goto fail;
 error=remesa_persistent_set_native_fill(&owner->device,remesa_linux_memory_native_fill);if(error)goto fail;
 error=remesa_persistent_take_provider(&owner->device,native_2d_release,owner);if(error)goto fail;
 owner->provider_owned=true;
 screen=remesa_screen_create_owned(&owner->device,remesa_persistent_close);
 if(!screen){error=ENOMEM;goto fail;}
 error=remesa_screen_enable_native_copy(screen,remesa_persistent_copy_region);
 if(!error)error=remesa_screen_enable_native_fill(screen,remesa_persistent_fill_region);
 if(error){
  /* No children yet; screen owns the provider. These internal setters cannot
   * fail for the validated empty screen. Never independently close its fd. */
  bool released=false;owner->release_completed=&released;
  screen->destroy(screen);
  if(!released){owner->release_completed=NULL;report->retained=owner;
   report->cleanup_error=owner->cleanup_error?owner->cleanup_error:EOWNERDEAD;}
  report->error=error;errno=error;return NULL;
 }
 return screen;
fail:
 report->error=error;report->cleanup_error=native_2d_cleanup(owner);
 if(report->cleanup_error)report->retained=owner;
 errno=error;return NULL;
}
#else
struct pipe_screen *remesa_loader_native_2d_screen(int fd,struct remesa_native_2d_report *report)
{(void)fd;if(report)*report=(struct remesa_native_2d_report){.error=ENOTSUP};errno=ENOTSUP;return NULL;}
#endif
