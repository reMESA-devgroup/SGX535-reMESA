/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_GEM_EXECUTOR_ADAPTER_H
#define REMESA_GEM_EXECUTOR_ADAPTER_H
#include "remesa_gem_memory.h"
struct sgx535_gma500_owner;
struct remesa_gem_executor_adapter;
struct remesa_retained_snapshots {
 u32 request_bytes,response_bytes;
 unsigned char request[640],response[4352];
};
int remesa_gem_executor_snapshots(struct remesa_gem_executor_adapter *,struct remesa_retained_snapshots *);
/* The lender keeps page arrays, private CPU vmaps, owner, device and power
 * alive until successful adapter disposal (including acknowledgement). No GEM/VA/device ownership is transferred. The revalidate callback must not
 * re-enter the endpoint APIs: executor callbacks run under its mutex.
 * A role uses exactly one held resource; LOCAL roles need backing but their
 * descriptor VA remains zero. This is not a userspace packet format. */
#ifdef REMESA_LAB_DEPTH1280_PROFILE
#define REMESA_ADAPTER_ROLES 11u
#else
#define REMESA_ADAPTER_ROLES 10u
#endif
#define REMESA_ADAPTER_COMMAND_BYTES 264u
struct remesa_executor_view {
 struct remesa_memory_binding binding;
 struct page **pages;
 unsigned page_count;
 void *cpu;
};
struct remesa_executor_platform {
 int (*validate_owner)(void *,const struct remesa_executor_view *,unsigned,
                       const void *,unsigned);
 int (*run)(void *,u32,u32,u32,u32,u32 *);
 int (*visible)(void *);
 int (*acknowledge)(void *);
};
struct remesa_gem_executor_adapter {
 struct remesa_memory_ticket ticket;
 struct remesa_executor_view views[REMESA_ADAPTER_ROLES];
 unsigned char commands[REMESA_ADAPTER_COMMAND_BYTES];
 int (*revalidate)(void *,struct remesa_memory_ticket,
                   const struct remesa_executor_view *,unsigned);
 void *lender,*platform_context,*allocated_platform;
 const struct remesa_executor_platform *platform;
 u32 revision,timeout_ticks,max_samples,lifecycle;
 bool bound,attempted,completed,polled,visible,acknowledged,quarantined;
#ifdef REMESA_GEM_ADAPTER_TEST
 bool test_execution;
#endif
};
/* Bind a pre-existing borrowed owner, never construct a second scene owner.
 * The lender must initialize its hardware descriptors/session using held pages,
 * with gem_live false and CPU views NULL; bind creates private CPU aliases.
 * Production start is unconditionally disabled. The adapter makes separate owner CPU
 * aliases because the proven publisher consumes them; endpoint maps stay held. There is no registration or
 * authorization API. A future owner initializer, provenance admission, and
 * reusable lifecycle need independent qualification before enabling this. */
int remesa_gem_executor_bind(struct remesa_gem_executor_adapter *,
 struct sgx535_gma500_owner *,struct remesa_memory_ticket,
 const struct remesa_executor_view *,unsigned,
 int (*)(void *,struct remesa_memory_ticket,const struct remesa_executor_view *,unsigned),
 void *,u32,u32,u32);
extern const struct remesa_job_executor remesa_gem_executor_ops;
/* Lender keeps mappings independently; disposal never releases its resources. */
int remesa_gem_executor_dispose(struct remesa_gem_executor_adapter *);
#ifdef REMESA_GEM_ADAPTER_TEST
/* Test-only injected operations cannot be installed in a production build. */
int remesa_gem_executor_test_bind(struct remesa_gem_executor_adapter *,
 const struct remesa_executor_platform *,void *,struct remesa_memory_ticket,
 const struct remesa_executor_view *,unsigned,const void *,unsigned,
 int (*)(void *,struct remesa_memory_ticket,const struct remesa_executor_view *,unsigned),
 void *,u32,u32,u32);
#endif
#endif
