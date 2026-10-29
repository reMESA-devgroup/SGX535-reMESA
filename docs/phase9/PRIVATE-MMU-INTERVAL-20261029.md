# One MMU writer interval and deferred ordinary flushes

The private integration profile now has `remesa_mmu_private_bind_locked()` and
`remesa_mmu_private_restore_locked()`. Their caller already owns the MMU write
semaphore. Neither helper acquires or releases that semaphore, including on
failure. This permits a synchronous kernel executor to retain one writer interval
across bind, pre-issue publication, execution, owned completion/visibility/
retirement and restore. There is no permission to carry the lock across ioctls or
between tasks.

The complete unbound-directory audit and table cache maintenance also operate
under that interval. The original public auditor still manages its own lock;
private locked publication uses its shared implementation without reacquisition.
The public table-sync and exclusive bind/restore entry points remain wrappers
that acquire/release the semaphore. The root/control MMIO sequences and original
BIF invalidation body are preserved. Bind still refuses dirty state and invalid
closures; restore still requires a caller-proven retired job or proof of no issue.
Idle registers alone do not authorize restore.

## Deferred default flush after uncertain failure

Original MMU insertion/removal may release the read semaphore before calling the
public `psb_mmu_flush()`. Such a flush can already be queued when a private root
is bound. Holding the write semaphore during a successful transaction excludes
it. An uncertain transaction also needs protection after the caller returns and
releases that semaphore.

The opt-in profile appends a retained private-owner pointer to `psb_mmu_driver`,
initialized explicitly because the original constructor uses kmalloc. The writer
semaphore protects it. Bind stores the owner before the first possibly effective
root write. Successful checked restoration clears it only after root/control
readback and context bookkeeping succeed. Uncertain bind/restore retains it.
Private maintenance and bound validation corroborate the same owner. Release
refuses the retained owner independently of the owner's root flags.

The public locking flush now checks that pointer under the writer semaphore. If
an owner is retained, it sets the existing pending-invalidation flag and returns
without BIF MMIO, barriers, video notification or a guessed reset. The original
hardware body remains available to the authenticated already-locked private
maintenance stage. Successful restore performs its original forced invalidation;
subsequent ordinary flushes resume the original path. No invalidation request is
silently reclassified as completed while the owner remains retained.

This addresses queued ordinary flushes; it is not a complete quarantine or
recovery protocol. The caller still retains device/module/power/default-directory
lifetimes and the producer/IRQ reservation, excludes context setters and other
device operations, and preserves every GEM/private-root hold on uncertainty.
Context setters in the inspected retained source are called during initialization;
that observation does not justify removing the lifecycle exclusion contract.
No new executor registration, ioctl, GPU submission or retirement proof is added.

## Evidence

`/home/gama/sgx535-offline/private-mmu-interval-20261029/v3/RESULT.json` records
native, UBSan and i386 runs, each with 50,278 shared MMU checks including the prior
root and maintenance suites. The new interval tests exercise one acquire/release
pair, unchanged cache/root sequencing, preflight/allocation refusals, uncertain
bind/restore retention, deferred ordinary flushes without hardware-model writes,
and original flush resumption after checked restoration. Successful cycles are
explicitly UNISSUED CPU models; they do not prove GPU retirement or hardware cache
coherence. The separate auditor regression has 40,459 checks.

`v3/bound-v2/RESULT.json` records 49,918 shared bound-translation regression checks
per variant using the private profile and original factored maintenance body.
These counts overlap existing suites and must not be summed as unique coverage.
The first bound harness omitted that required body and failed compilation; its
logs remain in v3. The corrected harness includes the original extracted body.

`v3/integration/GUARD_RESULT.json` records the existing adapter/IRQ/native producer
regressions, six actual antiX kernel objects and a partial link with all new driver
symbols resolved. `LEGACY_STAGING.json` confirms five non-private generated source
replacement profiles remain byte-identical to bdf3807. This does not assert full
module identity, installation or physical qualification.

v1 records the initial locked-interval change before the durable-owner extension.
v2 preserves a test-only signedness warning; it was fixed without relaxing
-Werror. The final implementation is v3. No physical GPU operations, boot changes
or Mini12 writes occurred. General userspace 3D/2D, VA-API, installable driver
integration and physical qualification are still incomplete.
