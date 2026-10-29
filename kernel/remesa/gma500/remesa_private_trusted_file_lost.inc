/* SPDX-License-Identifier: GPL-2.0-only */
/* Bridge owns stream_lock. Retain operation/borrower/source/frames, no callbacks
 * or cleanup. source_poison is software-only and preserves its existing lock. */
#include "remesa_private_file_lost.h"
void remesa_private_trusted_file_lost(struct drm_device *dev,struct drm_file *file)
{
 if(!dev||!file)return;
 if(stream.dev==dev&&stream.file==file){
  stream.file=NULL;stream.quarantined=true;
  remesa_stream_source_poison(&stream);
 }
}
void remesa_private_trusted_device_lost(struct drm_device *dev)
{
 /* close_stream leaves old dev/file metadata behind after clearing claimed.
  * Do not poison a closed stream or repeat a completed device revocation. */
 if(!dev||stream.dev!=dev)return;
 if(!stream.claimed){stream.file=NULL;return;}
 if(!stream.file&&stream.quarantined)return;
 stream.file=NULL;stream.quarantined=true;
 remesa_stream_source_poison(&stream);
}
