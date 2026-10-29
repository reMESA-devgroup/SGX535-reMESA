# Independent code audit and corrections, 2026-10-29

Four independent agents reviewed MMU/GEM memory, IRQ/concurrency/lifetimes,
kernel–userspace contracts, and Mesa/2D/builds. Initial baseline was `44d2d83`.
The coordinating agent implemented the corrections below. This is a bounded
source audit with CPU reproductions, not certification that all code is secure
or complete. There was no Mini 12 access, module installation, physical render,
new FPS measurement, reset, boot change, or push during this work.

## Confirmed findings and disposition

| Finding | Evidence and consequence | Disposition |
| --- | --- | --- |
| Unexpected IRQ before TA_FIRE can disappear from the scene outcome | Actual capture/source/service counterexample accepted two operations after injected bit29; IRQ acknowledgement can precede baseline polling. CPU locks/statuses modeled. | Corrected in new **private** stages: unexpected prefire events invalidate source evidence. A flag protected by the fixed IRQ lock limits expected INITEND/status2 low4 events to TA_LOAD actions. Private 2D completion also invalidates. Original postfire gates remain. |
| Shared screen references and resource cookies race | Four threads calling the extracted original reference helper lose increments; duplicate cookies are possible with the original unsynchronized increment. | Reference operations and auxiliary reads are atomic; cookie allocation uses saturating CAS without reuse on overflow. **Full screen/backend thread safety remains unfinished**, including resource state and backend registries. |
| Visible-copy helper bypasses deferred TA CPU ownership | An actual-helper CPU witness copies while leaving `ta_gpu_dirty=1`, `ta_cpu_cached=0`. | After mapping succeeds, TA reads call the existing domain-transfer primitive and record CPU caching. This helper is separate and was not included in the retained v3 module. No stale physical pixels were demonstrated. |
| Unmap postcondition accepts a foreign residual PTE | Extracted actual unmap returns success and releases VA when failed removal leaves another PFN. | All newly staged profiles for this pinned trap-mode target require lookup to report an invalid/absent mapping after removal. Any successful lookup or other error quarantines and retains VA/backing. No residual PTE cause or physical use-after-free was demonstrated. |
| LAB collector chooses its own OPEN context | Actual kernel endpoint accepts only zero and returns a server-issued context; the first transport model incorrectly accepted a caller-issued one. | Collector now sends zero and adopts the returned context. A new Python-to-actual-C memory endpoint test covers this contract. Previous model-only success did not establish compatibility. |
| LAB collector accepts malformed memory responses or nonzero ioctl success | Injected positive return or changed context was treated as success. | Memory response fields are checked with an explicit mutation allowlist; invalid replies poison the transport and block further ioctls/cleanup. Probe return must also be zero. This is robustness against malformed responses, not a demonstrated kernel exploit. |
| Stage manifest omits unmodified copied sources | Original stage records only patched/added files; changing an unpinned copied source is absent from that manifest. | Full native snapshots and final staged source inventories are recorded, with a change-during-stage check. Inventory is distinguished from the limited approved hash anchors; it is not a substitute for command/address validation or review. |

The unmap fix relies on the pinned native `psb_driver_load` constructing its MMU
with `trap_pagefaults=1`. It must not be generalized to a non-trap default PD
without checking canonical dummy mappings. The native checkpoint is unchanged.
All normal staged profiles differ from their baseline code only by this unmap
postcondition; the IRQ/source changes are confined to the private profile.

## Verification and its limits

The following evidence is local under
`/home/gama/sgx535-offline/deep-audit-20261029/`:

| Artifact | Scope |
| --- | --- |
| `prefire-irq/RESULT.json` | Original actual source/capture counterexample, UBSan, two operations accepted; GPU0. |
| `prefire-fixed-v1/RESULT.json` | 26 before/after runs over 13 scenarios. Unexpected prefire status1 bits29/27 and status2 bit4 rejected; INITEND/low15 allowed only in the load window. Source/capture/service/borrower real; stage callbacks, flag interval and locks modeled. |
| `screen-counter-regression-v1/RESULT.json` | Exact hold/drop/cookie helpers with a reduced owner, native and UBSan: four threads, 200,000 unique cookies, one final release, overflow refusal. Not a full Gallium concurrency test. |
| `screen-regression-v1/result.json` | Actual screen compiled against pinned Mesa; context contracts, driver and resource sync pass native/UBSan using a recording backend. No SGX execution. |
| `screen-i486-regression-v1/RESULT.json` | Actual Gallium driver/resource-sync tests compile and statically link for i486 with libatomic; QEMU execution passes. CPU-only recording backend. |
| `mesa-screen-race-fixed/I486_ATOMICS.json` | i486 requires libatomic for 64-bit cookie atomics. Qualification linker updated accordingly. |
| `visible-domain-fixed-v1/RESULT.json` | Actual visible-copy helper; transfer/predicate modeled. GPU-dirty clears and CPU-cached sets; repeated read does not transfer again. |
| `visible-readback-regression-v1/RESULT.json` | Existing visible helper and ioctl tests, four modes each, native/UBSan, actual GEM service with host kernel API shims. |
| `unmap-fixed-v1/RESULT.json` | Exact staged unmap, native/UBSan; absent mapping releases, original/foreign residual PTE or lookup error retains. Removal/PTE APIs modeled. |
| `stager-regression-v2/RESULT.json` | Five normal profiles contain only the expected unmap code change; all stage source files inventoried. Private sources exactly match the newly built module. Earlier v1 predates applying the unmap correction to all profiles. |
| `private-module-v1/RESULT.json` | Full i386 module Kbuild/modpost/link passed in 104.056 seconds, one compiler at a time, 768 MiB process address-space ceiling. Uninstalled and physically unqualified. |
| `private-module-v1/MODULE_VERIFICATION.json` | Read-only final ELF verification; all six experimental controls are zero-initialized BSS booleans. No observation of live hardware controls is implied. |

Client evidence is separately retained under
`/home/gama/sgx535-offline/private-probe-client-20261029/`:
`actual-memory-contract-v2/RESULT.json` exercises actual C memory ioctl
admission/creation/map/lease/upload/read/info/cleanup from Python packets, with
only the Linux GEM/MMU/PM APIs modeled. It rejects a user-chosen OPEN context,
preserves a server context wider than 32 bits, and ends with zero modeled
resources. Client unit tests also inject invalid replies and uncertain receipts.

The new full module SHA-256 is
`258f6685300b8736d48abd3e1e6fa719587281015c43fece738f5490e13db6fb`.
Its target remains antiX `5.10.240-antix.1-486-smp`. This hash identifies an
uninstalled build; it is not physical acceleration evidence or LAB approval.

## Unresolved blockers and next work

1. PCI removal still bypasses retained private-root/job lifetimes. The earlier
   [kernel audit](sgx535-kernel-audit-20261029.md) remains applicable. Do not enable
   the private physical path until active detach, callback joining, hardware
   stop/isolation and deferred software destruction form a coherent protocol.
   Moving unload wholesale to final release or clearing PCI bus mastering alone
   was not established as sufficient.
2. General shared-backend/resource synchronization is not solved by atomic
   screen references/cookies. It needs a lock order and real concurrent Gallium
   entry-point tests, including registry/object/fence lifetime transitions.
3. The prefire fix proves final rejection/retention, not that no MMIO write can
   occur after concurrent invalidation: `fixed_write` checks source authority
   before the register write without one common IRQ critical section. Review
   callers before attempting to linearize check/write; avoid recursive locking.
   **Subsequent correction:** the private fixed-write path now holds the existing
   source lock across validation and CPU MMIO issue. See
   [source/MMIO follow-up](../private-source-mmio.md) for the actual two-thread
   counterexample/regression, lock order, build and remaining lifetime limits.
4. Full `fixed_run` TA_LOAD execution was compiled but not exercised by the
   source/service CPU counterexample. Its flag set/clear order was source-reviewed;
   the test models that action interval. Hardware timing remains unqualified.

The PM race raised during review remains a hypothesis, not a confirmed defect or
an excuse for speculative changes. No shader/geometry encoding was reopened.
Complete OpenGL/GLES, desktop 2D integration, VA-API and an installable qualified
driver remain work toward the original objective, not achievements of this audit.

Individual reports and reproductions are preserved alongside this evidence:
`mmu-memory.md`, `concurrency-lifetime.md`, `uapi-transport.md`, and
`mesa-2d-build.md`. They record baseline line numbers and distinguish confirmed
software counterexamples, hypotheses and coverage limits.
