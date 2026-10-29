# Private MMU maintenance under an already-held write semaphore

The private staging profile now provides a checked maintenance operation that
does not recursively acquire driver->sem. This removes a concrete prerequisite
for holding MMU writer exclusion throughout a future synchronous execution
interval. It does not yet wrap the executor or publish a root from userspace.

Inspection found two blockers in the retained fixed backend: its translation
publication stage requires the default PD and calls a read-locking default-PD
getter; its DEVICE_MAINTAIN stage calls psb_mmu_flush, which takes driver->sem
for writing. Simply holding that semaphore around the existing executor would
therefore reject the private context or deadlock. No such wrapper was enabled.

`stage_twod_reservation.py` factors the original pinned psb_mmu_flush hardware
body into `remesa_mmu_original_flush_locked` for the private profile. The public
wrapper still acquires/releases the write semaphore once around the same body.
Both original FLUSH/INVALDC branches, barriers, control readback, pending-flush
reset and video invalidation side effect are preserved. Old nonprivate staged
source profiles do not use this factoring.

`remesa_mmu_private_maintain_locked` requires the caller to already hold that
write semaphore and the retained device/producer/IRQ/power contract. It checks
active/nonuncertain owner state, default/private context bookkeeping, exact
SGX535/rev121 identity, root, control and the existing pre-issue quiet predicate.
It then invokes the factored original hardware body and checks root/control
again. An active failure marks the owner uncertain and retains it. It performs
no root publication, command issue, ACK, reset, restore or retirement. It is
available only in the opt-in private integration profile; other builds return
EOPNOTSUPP. No data-buffer hashing, readback or redundant clearing was added.

Evidence: `/home/gama/sgx535-offline/private-mmu-locked-maintain-20261029/`.
Authoritative current-source run is `v3/RESULT.json`. Native, UBSan and i386
rerun the existing root transaction/failure suite and pass50025 total shared
checks each, including288 maintenance additions. The intermediate existing
suite reports49737 (it includes the outer invocation check). Counts are shared,
not independent tests to sum. The public wrapper is exercised for both pending
translation states, proving one acquisition/release and the original two
control writes. The private helper runs while the write semaphore is already
held; lock counters prove it never reacquires/releases it internally. Wrong
root/revision/events/banks and corrupted control retain an uncertain owner.
No additional CPU page maintenance or root write occurs in this operation.

The complete original antiX MMU translation unit compiles without warnings
with the new private helper and factored body. Confinement regression remains
40459 checks. These are instrumented CPU MMIO tests and kernel compilation,
not physical cache coherency or GPU isolation approval. v1's main-macro compile
failure is retained; v2 passed the288 added cases, v3 also invokes the entire
existing suite. All31 protected files and original qualified sources remain
unchanged.

Next, translation publication must explicitly validate the authenticated bound
private PD and retained job views instead of asking whether the default PD is
bound. Only then can the actual executor use already-locked maintenance inside
one MMU writer-exclusion interval. Unknown/failed execution must preserve the
root/IRQ/producer/resource retention contracts. Full general GL/GLES2, desktop
integration, VA-API hardware decoding and installability remain unfinished.
