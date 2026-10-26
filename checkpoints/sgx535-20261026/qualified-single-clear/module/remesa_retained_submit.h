/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_RETAINED_SUBMIT_H
#define REMESA_RETAINED_SUBMIT_H
#include "remesa_sequence.h"
#include "remesa_retained_session.h"
#include "remesa_gem_executor_adapter.h"
/* Kernel-only laboratory entry. No user authorization bit and no ioctl ABI.
 * The lender serializes endpoint close until close succeeds. A failed issued
 * operation retains session/backing; this entry provides no reset or retry.
 */
struct remesa_retained_result {
 struct remesa_sequence_fence fence;
 enum remesa_sequence_state state;
 int error;
 bool admitted,attached;
};
struct remesa_retained_submit {
 struct mutex lock;
 struct remesa_sequence sequence;
 struct remesa_sequence_fence fence;
 struct remesa_retained_snapshots snapshots;
 struct remesa_retained_session *session;
 remesa_retained_images images;
 void *profile;
 u32 revision,ticks,samples;
 bool initialized,attempted,closing;
#ifdef REMESA_RETAINED_SUBMIT_TEST
 bool test_admission;
#endif
};
/* Default false; explicit isolated laboratory gate, not user authorization. */
bool remesa_retained_lab_admitted(void);
int remesa_retained_submit_init(struct remesa_retained_submit *,struct remesa_gem_memory *,
 remesa_retained_images,void *,u32,u32,u32);
int remesa_retained_submit_once(struct remesa_retained_submit *,
 const struct remesa_memory_binding *,unsigned,const void *,unsigned,
 unsigned,struct remesa_retained_result *);
int remesa_retained_submit_close(struct remesa_retained_submit *);
#ifdef REMESA_RETAINED_SUBMIT_TEST
/* Host test only; absent from production module. */
int remesa_retained_submit_test_admit(struct remesa_retained_submit *);
#endif
#endif
