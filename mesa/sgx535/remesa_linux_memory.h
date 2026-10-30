/* SPDX-License-Identifier: MIT */
#ifndef REMESA_LINUX_MEMORY_H
#define REMESA_LINUX_MEMORY_H
#include "remesa_persistent.h"
#include "remesa/memory_uapi.h"
#include "remesa/twod_copy_uapi.h"
#include "remesa/twod_fill_uapi.h"
#include "remesa_retained_transport.h"
#include "remesa/private_session_lab_uapi.h"
/* Borrow fd/context already bound by the future kernel per-file integration.
 * Never discovers/installs a driver or opens a live node. Current stock/native
 * installations do not provide this resource service. Archived experimental
 * modules register a default-disabled endpoint; its availability and exclusive
 * ownership must be qualified separately. No draw support is provided here.
 * Externally serialized, close only after persistent resources are destroyed. */
struct remesa_linux_memory;
/* Begin a kernel-issued per-file resource session; no GPU submission. */
struct remesa_memory_recorder {
 int (*record)(void *,const struct remesa_memory_request *,const struct remesa_memory_request *,int,int,bool);
 void *priv;
};
int remesa_linux_memory_begin_recorded(int,struct remesa_linux_memory **,const struct remesa_memory_recorder *);
int remesa_linux_memory_begin(int,struct remesa_linux_memory **);
int remesa_linux_memory_attach(int,uint64_t,struct remesa_linux_memory **);
int remesa_linux_memory_info(struct remesa_linux_memory *,uint32_t,struct remesa_memory_info *);
/* Exact retained ID/size/VA/pin/PTE ownership admission for a future executor.
 * Performs INFO only, never submission. This proves CPU PTE ownership, not
 * GPU TLB visibility/completion. An uncertain response poisons the object. */
int remesa_linux_memory_validate_backing(struct remesa_linux_memory *,uint32_t,
                                        uint64_t,size_t);
uint64_t remesa_linux_memory_context(const struct remesa_linux_memory *);
/* Reuse this endpoint's exact fd/drm_file, not a second native-draw path.
 * Kernel admission/borrowed ownership and recorder remain mandatory. */
int remesa_linux_memory_submit_retained(struct remesa_linux_memory *,
 struct remesa_retained_request *,bool *,remesa_retained_recorder,void *);
/* Explicit bounded LAB session on this same fd/context; never enabled by
 * ordinary dispatch or advertised as general GL support. OPEN validates owned
 * mappings once. SUBMIT does one synchronous ioctl with no INFO, allocation,
 * hashes, readback, logs or retry; its exact receipt includes retirement and
 * CPU publication. Raw responses are returned even on uncertain failure, for
 * a separate LAB collector. They still require independent pixel validation.
 * On any entered-ioctl failure all this endpoint's BOs are retained/poisoned;
 * close/free is forbidden through it. forget drops host metadata only.
 * Callers must externally serialize every operation on the borrowed fd and
 * retain resources until acknowledged CLOSE. At most one OPEN attempt; no
 * rearm after CLOSE. Memory-only diagnostic recorders are refused.
 */
int remesa_linux_memory_private_lab_open(struct remesa_linux_memory *,
 const uint32_t ids[11],uint32_t maximum,struct remesa_private_session_lab_request *);
int remesa_linux_memory_private_lab_submit(struct remesa_linux_memory *,
 uint32_t budget_ms,struct remesa_private_session_lab_request *);
int remesa_linux_memory_private_lab_close(struct remesa_linux_memory *,
 struct remesa_private_session_lab_request *);
/* Synchronous typed native copy on this exact owned fd/context. Caller starts
 * version/context/response fields at zero; local owned IDs/mappings and leases
 * are checked before one ioctl. No INFO, hashes, CPU maps/readbacks or retry.
 * Success requires a new monotonic operation and exact SGX535/rev121
 * completion/visibility/retirement. An
 * ambiguous transport/response poisons both allocations; they cannot be
 * unpinned/destroyed through this object. Kernel quarantine survives FD close.
 * Memory-only diagnostic recorders do not record this ABI and are refused.
 * Requires the separately enabled privileged copy endpoint; not GL admission.
 */
int remesa_linux_memory_native_copy(void *,const struct remesa_gpu_copy *);
/* Destination-only variant of the same authenticated synchronous contract. */
int remesa_linux_memory_fill_2d(struct remesa_linux_memory *,struct remesa_twod_fill_request *);
int remesa_linux_memory_native_fill(void *,const struct remesa_gpu_fill *);
int remesa_linux_memory_copy_2d(struct remesa_linux_memory *,struct remesa_twod_copy_request *);
/* Drop ONLY host staging/metadata after all consumers stop. No kernel cleanup,
 * no ioctl, no claim of retirement; caller still owns and must close the fd. */
void remesa_linux_memory_forget(struct remesa_linux_memory *);
int remesa_linux_memory_detach(struct remesa_linux_memory *);
const struct remesa_persistent_ops *remesa_linux_memory_ops(void);
#endif
