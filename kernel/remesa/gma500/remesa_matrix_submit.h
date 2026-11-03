/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_MATRIX_SUBMIT_H
#define REMESA_MATRIX_SUBMIT_H
struct drm_device;struct drm_file;
struct remesa_gem_memory;struct remesa_twod_fill_request;struct remesa_twod_retirement;
/* Internal normal-fill backing ownership only, not source admission. Begin
 * runs without endpoint_lock; finish/cancel require it. No userspace token. */
int remesa_matrix_fill_begin(struct drm_device *,struct drm_file *,
 const struct remesa_twod_fill_request *,const void **);
int remesa_matrix_fill_finish(struct remesa_gem_memory *,const void *,
 const struct remesa_twod_fill_request *,const struct remesa_twod_retirement *);
void remesa_matrix_fill_failed_locked(struct remesa_gem_memory *,const void *);
int remesa_matrix_fill_cancel_unissued(const void *);
int remesa_matrix_submit_ioctl(struct drm_device *,void *,struct drm_file *);
#endif
