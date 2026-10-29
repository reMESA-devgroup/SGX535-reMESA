# Local kernel source audit, 2026-10-29

Reviewed HEAD `6f841fb57b7fc7e52c30e86a200857929c4525dd`. No runtime code
changed, builds or GPU tests run, files removed, commits made, or messages sent
externally. The parent's untracked handler test was left untouched. The review
also inspected retained native sources and the v3 staged source, not proprietary
blobs. No applicable AGENTS.md was found in the worktree or its ancestors.

## P1 / bring-up blocker: removal bypasses retained lifetime

**Static integration defect; runtime consequences not reproduced.**
`remesa_gem_private_execute.inc:71-76` leaves uncertain work and roots retained
then releases the writer semaphore. `remesa_mmu_switch.inc:113-118` stores raw
pointers to the default directory and MMU driver. The endpoint's device, module,
and power references prevent normal endpoint destruction/unload, but do not
intercept explicit PCI driver removal.

In the built v3 module's `psb_drv.c:529-535`, `psb_pci_remove()` unregisters the
DRM device then directly invokes `psb_driver_unload()`. At `:229-240`, unload
removes default mappings and calls `psb_mmu_driver_takedown()`. That function in
v3 `mmu.c:437-444` writes BIF_CTRL and frees the default directory and MMU driver
without checking `remesa_private_owner`. The source lifecycle notification at
`psb_drv.c:203` does not gate the following teardown. Thus explicit remove/unbind
does not honor private retention; the retained private owner can contain stale
driver/default-directory pointers, and the register mapping is later removed at
`:252-254`. Removal can also begin while the ioctl holds the endpoint mutex,
which is not acquired by remove.

The corresponding native teardown exists in
`checkpoints/sgx535-20261026/qualified-single-clear/module/psb_drv.c`; private
staging adds no removal protocol. This is a gap in the whole-driver ownership
claim, not proof of an observed GPU access to freed memory. Fix requires a
coherent remove/device-loss protocol, not merely another object reference.

The available guards do not close this gap. Native
`gma500_bo_owner.c:25-46` serializes ownership claim/unclaim only; unload never
consults that lock. `remesa_stream_source.c:153-157` poisons source evidence but
returns void, without stopping or joining execution. `drm_dev_unregister()` is
not an endpoint-lifetime join, and this remove path does not use a shared
admission/unplug guard around the experimental ioctls.

The smallest useful synchronization boundary is before unload's first lifecycle,
IRQ, MMU or register teardown, shared with endpoint admission and any in-flight
entry. Mark removal/device loss so no new entry can begin, then join or cancel
the finite in-flight interval according to a documented lock order. Taking the
existing endpoint mutex could serialize this one controller, but is not a full
fix: after it returns quarantined there is no pending mutex holder to wait for
and no retirement guarantee. Removal must establish hardware stop/isolation
before reclaiming roots/backing, or transfer retained resources to a valid
device-loss lifetime. It must not wait indefinitely for a quarantine receipt
that the implementation deliberately never generates. No such hardware stop
guarantee was established by this audit; adding a lock or `pci_clear_master()`
alone must not be represented as that proof. Current kernel unplug helpers and
driver-wide ownership must be evaluated on the eventual forward-port target.

## P1 / design blocker: moving unload to release is insufficient

**Static follow-up assessment of a proposed fix, not an implemented change.**
The locally inspected Linux 5.10.240 source provides `drm_driver.release`
(`include/drm/drm_drv.h:261-270`), documented there as deprecated in favor of
DRM-managed allocations. `drivers/gpu/drm/drm_drv.c:741-750` invokes it on the
last device reference, before `drm_managed_release()`. The v3 gma500 driver
initializer (`psb_drv.c:566-591`) defines neither release nor unload; remove
currently calls its unload function directly. Deferring that function wholesale
would change timing without making its operations safe after detach.

| Priority | Concrete path in v3 staged sources | Why deferred unload alone is unsafe |
| --- | --- | --- |
| P1 | `psb_drv.c:220-242`; `mmu.c:437-444`; `gtt.c:388-403` | Chip/MMU/GTT teardown writes registers and PCI configuration. An old instance could modify hardware after a new bind. |
| P1 | `gem.c:21-30,126-151`; `gtt.c:290-298,370-378` | Late GEM faults pin GTT state; object free can unpin, remove default-MMU/GTT mappings, and release pages. These paths outlive ioctl admission. |
| P1 | `power.c:72-75,292-300` | Late PM teardown disables runtime PM on the same PCI device; outstanding old power references later call pm_runtime_put. Device-reference retention is not ownership of a future binding's PM state. |
| P1 | `psb_drv.c:203,213-214`; `remesa_stream_source.c:153-157`; `remesa_twod_irq.inc:6-9` | IRQ handling must be synchronized while hardware ownership is valid. Source/IRQ ownership includes module-global state; old cleanup must not invalidate a new instance's state. |
| P2 | `gtt.c:329,370-378,461`; `power.c:59-60` | GTT ranges belong to the PCI resource tree; PM locks are module-global and initialized on load. Rebind with retained old state needs an explicit ownership policy. |
| P2 | `psb_drv.c:510-525`; existing load error path | Adding release must cover partially initialized devices and existing explicit error cleanup without double teardown. |

These are source-established hazards in the proposed refactoring; a late teardown
collision or double teardown has not been exercised. P1 here means blocking this
private bring-up/design, not a claim of an independently reproduced exploit.

The useful existing primitive is `drm_dev_unplug()`: in this 5.10 tree,
`drm_drv.c:460-471` sets unplugged, waits for the unplug SRCU readers, and then
unregisters. `drm_dev_enter/exit` (`:424-446`) protect only the paths that actually
use them. New ioctl protection alone does not include legacy GEM faults,
destructors, console/KMS callbacks, PM, or private postclose. Do not hold the
endpoint mutex across the unplug wait if a protected reader may need that mutex;
doing so can deadlock. Use a documented driver-wide lock and callback ordering.

A coherent direction separates active detach from deferred software destruction:

1. Block new hardware admission and join protected in-flight accesses before
   lifecycle/IRQ/MMU/register teardown. Bound or cancel legitimate in-flight work
   without waiting forever for quarantined retirement.
2. Quiesce callbacks/IRQ and establish hardware stop/isolation while the old
   instance still owns the device. Merely masking IRQ does not stop GPU memory
   accesses. No sufficient hardware isolation protocol was proved here.
3. Retain uncertain GPU backing/tables until safety is established. Define what
   happens on rebind and actual device disappearance, not just module unload.
4. Give late GEM/VMA/postclose cleanup device-loss semantics and ensure deferred
   destruction is software-only: no stale MMIO, PM, GTT or global-owner operations.

This requires guards or alternate cleanup paths across `psb_drv.c`, memory/probe
entry and postclose, `gem.c`, `gtt.c`, MMU destructors, PM, IRQ/backend/source
ownership, and active console/KMS callbacks. It is not a recommendation to add a
single release callback as a partial fix. The private probe stays disabled and
private hardware tests remain blocked pending an adequate lifecycle design.

Subsequent implementation adds a [private CPU entry/unplug join](../private-entry-guards.md)
for ioctl, mmap and fault callbacks. It closes their overlap with teardown but
does not supply the retained-GPU-resource, postclose, late GEM or rebind policy
required above. The original audit remains a record of the reviewed baseline.

## P3 / documentation: private staging adds an ioctl

At the reviewed HEAD, `tools/remesa/stage_twod_reservation.py:341-342` describes private mode as
preparation with no new ioctl/admission. The same option appends the actual
probe controller at `:263` and registers PRIVATE_PROBE at `:264-268`.
Its module docstring at `:2-5` also understates the private option's execution
scope. The coordinating agent has corrected these descriptions in the working
tree separately; this finding describes the reviewed HEAD. Defaults N do not make
a compiled ioctl absent. No runtime code change was needed for this finding.

## Risks and missing evidence, not demonstrated defects

- **Quarantine exclusion after return.** During execution the MMU writer spans
  bind to restore (`remesa_gem_private_execute.inc:37-70`). Failure releases it
  (`:74-76`). Staging defers ordinary default flushes while private-owned
  (`stage_twod_reservation.py:52-59`), but original context setters, directory
  destructors and default mapping mutation need a complete lifetime audit.
  Producer exclusion is not automatically exclusion of all MMU writers.
- **No reusable fence contract.** The reviewed private/reservation code has no
  dma_fence/dma_resv/drm_sched/syncobj integration. Its state/cookie/ledger receipts
  support its retained synchronous experiment; they do not demonstrate safe
  asynchronous, imported-buffer, multi-client, or desktop synchronization.
- **Conservative retention is intentional.** After successful source bind,
  `execution_attempted` is set before root binding (`execute.inc:34-38`). Every
  subsequent failure retains resources even if issue never happened. The probe
  consumes a module-global no-rearm flag (`private_probe.inc:26-27,88,132`).
  This is a bounded laboratory policy, not a retry/recovery implementation.
- **No integrated hardware proof.** Controller model dependencies include crypto,
  GEM, source and execution; neither its successful retirement nor the full ELF
  build demonstrates physical completion, correct output or private-root restore.
- **PM is not asserted broken.** The endpoint takes `gma_power_begin()` at native
  `remesa_gem_memory.c:102`, retains it to close (`:312`), and native suspend
  rejects a nonzero display_count. A power-is-on check alone would be weaker, but
  this path has a persistent reference. Removal remains the distinct gap above.

## Positive checks from actual source

`private_probe.inc:83-106` enforces privilege, defaults, request validation,
file/context identity, hardware/profile bindings and idle endpoint state.
`remesa_private_probe_request.h:8-15` rejects nonzero responses/reserved bits and
duplicate IDs. Profile authentication precedes private execution (`:115-134`).
Job holds and private copied page references preserve backing during the loan;
the confinement auditor rejects data/control and data/data PFN aliasing.

Success cleanup preserves source release before loan/job hold retirement
(`private_probe.inc:141-147`). The explicit execution_consumed flag survives loan
zeroing (`:135,151`), preventing a later cleanup error from being mistaken for
pre-attempt cancellation. Cancellation releases loan, reservation and borrower
before job cancellation (`:64-73`). Postclose refuses attached/unretired jobs and
clears a dying file pointer while retaining the endpoint (native
`remesa_gem_memory.c:299-312,929-948`). These checks support the intended normal
endpoint lifetime; they do not cure the separate PCI removal path.

No security bypass, normal-path use-after-free, or fence/retirement fabrication
was established in the reviewed private controller. This statement is limited
to source inspection and is not a hardware or exhaustive concurrency guarantee.

The [local evidence index](sgx535-kernel-local-evidence-20261029.md) maps offline
reports and build-tree source locations. It is intentionally separate from the
architecture RFC so the latter contains no machine-specific absolute paths.
