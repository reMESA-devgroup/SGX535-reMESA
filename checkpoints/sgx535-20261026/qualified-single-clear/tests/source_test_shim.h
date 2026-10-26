#ifndef SOURCE_TEST_SHIM_H
#define SOURCE_TEST_SHIM_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
typedef uint32_t u32;typedef uint64_t u64;
static inline u64 ktime_get_ns(void){static u64 ticks;return ++ticks;}
#include "../module/frozen_capsule.h"
#include "../module/frozen_source_guard.h"
#include "../module/frozen_fixed_service.h"
struct module {int dummy;};struct task_struct {int id;};
extern struct task_struct *current;
struct drm_device {void *dev_private;};
struct sgx535_gma500_owner {
 struct sgx535_frozen_session session;struct sgx535_fixed_service fixed_service;
 struct drm_device *dev;bool initialized,device_claimed;
};
struct sgx535_gma500_fixed_backend {
 struct sgx535_gma500_owner *owner;bool active,power_held,fire_possible;
 u32 pending_status1,pending_status2;
};
typedef void (*sgx535_source_reader)(struct drm_device *);
typedef void (*sgx535_source_snapshot_fn)(struct drm_device *,struct sgx535_source_facts *);
#define SGX535_GMA500_CAPSULE_OBSERVER_H
#include "../module/remesa_stream_source.h"
#define DEFINE_SPINLOCK(name) int name
#define spin_lock_irqsave(lock,flags) ((void)(lock),(flags)=0)
#define spin_unlock_irqrestore(lock,flags) ((void)(lock),(void)(flags))
static inline unsigned long sgx535_gma500_fixed_irq_lock(void) {return 0;}
static inline void sgx535_gma500_fixed_irq_unlock(unsigned long f) {(void)f;}
static inline void sgx535_gma500_source_snapshot(void *p,struct sgx535_source_facts *f) {*f=*(struct sgx535_source_facts*)p;}
int sgx535_provenance_source_begin(struct sgx535_gma500_fixed_backend *,
    struct drm_device *);
int sgx535_provenance_source_boundary(struct sgx535_gma500_fixed_backend *,
    const struct sgx535_source_facts *);
void sgx535_provenance_source_producer(int);
void sgx535_provenance_source_attach(struct drm_device *, struct module *,
    sgx535_source_reader);
void sgx535_provenance_source_observe(struct drm_device *, sgx535_source_snapshot_fn);
void sgx535_provenance_source_startup(struct drm_device *,
    const struct sgx535_source_facts *);
void sgx535_provenance_source_reset_begin(struct drm_device *);
void sgx535_provenance_source_reset_assert(struct drm_device *, u32);
void sgx535_provenance_source_reset_end(struct drm_device *,
    const struct sgx535_source_facts *);
void sgx535_provenance_source_lifecycle(struct drm_device *);
void sgx535_provenance_source_pending(struct drm_device *, u32, u32);
int sgx535_provenance_source_write(struct sgx535_gma500_fixed_backend *);
void sgx535_provenance_admit(struct sgx535_gma500_fixed_backend *);
void sgx535_provenance_service(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int);
void sgx535_provenance_issued(struct sgx535_gma500_fixed_backend *);
void sgx535_provenance_sample(struct sgx535_gma500_fixed_backend *, u32,
    u32, u32, int);
void sgx535_provenance_before(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, u32, u32, u32, int);
void sgx535_provenance_after(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *);
void sgx535_provenance_terminal(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int);
#endif
