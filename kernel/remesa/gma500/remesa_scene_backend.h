/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_SCENE_BACKEND_H
#define REMESA_SCENE_BACKEND_H
/* The legacy branch retains its real owner. The general branch uses the real
 * retained job/session, never a fabricated legacy owner or descriptor array. */
#ifdef REMESA_GENERAL_SCENE
#include "remesa_gem_scene_job.h"
static inline struct drm_device *remesa_backend_device(struct sgx535_gma500_fixed_backend *b)
{return b->general_scene?b->general_scene->snapshot.dev:(b->owner?b->owner->dev:NULL);}
static inline struct sgx535_frozen_session *remesa_backend_session(struct sgx535_gma500_fixed_backend *b)
{return b->general_scene?&b->general_scene->session:(b->owner?&b->owner->session:NULL);}
static inline const void *remesa_backend_owner(struct sgx535_gma500_fixed_backend *b)
{return b->general_scene?(const void *)b->general_scene:(const void *)b->owner;}
static inline const void *remesa_backend_service(struct sgx535_gma500_fixed_backend *b)
{return b->general_scene?(const void *)&b->general_scene->service:(const void *)&b->owner->fixed_service;}
static inline int remesa_backend_may_release(struct sgx535_gma500_fixed_backend *b)
{return b->general_scene?remesa_scene_service_may_release(&b->general_scene->service):sgx535_fixed_service_may_release(&b->owner->fixed_service);}
#else
#define remesa_backend_device(b) ((b)->owner->dev)
#define remesa_backend_session(b) (&(b)->owner->session)
#define remesa_backend_owner(b) ((b)->owner)
#define remesa_backend_service(b) (&(b)->owner->fixed_service)
#define remesa_backend_may_release(b) sgx535_fixed_service_may_release(&(b)->owner->fixed_service)
#endif
#endif
