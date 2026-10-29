# Private root transaction — CPU qualification, 2026-10-28

The new `remesa_mmu_switch.h/.inc` implement trusted internal bind/restore
primitives. They are NOT connected to a job path, ioctl or production admission.
The caller contract requires full device/producer/IRQ/power exclusion and a
retained idle lifecycle throughout bind, job and restore; clean register samples
alone cannot establish that lifecycle. The current 2D reservation is a useful
integration building block but is not automatically supplied by these helpers.

Bind re-audits and cleans private tables, then under the MMU write semaphore
checks default/private context metadata, pending translation flushes, identity
SGX535/rev121, root, zero banks, BIF control/faults/reads, 2D idle, events, reset
and autonomous kick/timer state. It retains both roots BEFORE the first MMIO
write, writes only DIR_LIST_BASE0, invokes the original BIF invalidation function,
and checks root/control readback. Restore requires caller-proven completion,
visibility and retirement or proof no command was issued. It preflights/cleans
default tables and controls before the same checked root/invalidation sequence.
Neither routine writes a bank selector, GPU command, event ACK or reset.

Any uncertain bind or failed restore retains an active/quarantined owner.
Release refuses it even if hw_context metadata is externally changed. There is
no automatic retry, rollback by guess, reset, timeout recovery or userspace
recovery permission. Successful restore only establishes checked root state;
it does not generate a completion or retirement receipt.

Evidence `/home/gama/sgx535-offline/private-mmu-root-transaction-20261028/v4/`:
native, UBSan and i386 each PASS 5557 transaction checks. Tests use instrumented
MMIO and cache callbacks plus the ORIGINAL psb_mmu_flush_pd_locked body extracted
verbatim from retained mmu.c. Cases cover three bind/restore cycles, all bits of
banks/reset/fault/status preconditions, wrong identity/root, outstanding reads,
autonomous sources, cache/allocation failures, dropped writes, changed control,
retention despite metadata tampering, and restore failures. Original antiX MMU
translation unit compiles with both new functions. Confinement regression still
passes 40459 checks. SOURCE_BINDING.json pins the included test ancestry and all
31 protected files unchanged. Earlier v1/v2 compile failures are preserved;
v3 is the earlier 5331-check pass, v4 is authoritative for current code.

No real MMIO, GPU submission, module installation or boot change occurred.
Still required: integrate exact GEM resource closure, enforce the caller's
whole-interval exclusion and immutable mapping contract, couple restore to
owned retirement, and qualify physically with recoverable boot and guards.
Default-directory metadata/cache coverage also needs verification against the
real directory before integration; current tests are synthetic. No private
context isolation or other-kernel compatibility is approved by this result.
