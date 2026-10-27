# Unbound private SGX page-directory audit

`kernel/remesa/gma500/remesa_mmu_confinement.h` and `.inc` implement a
kernel-side CPU audit of one **unbound, private** SGX page directory against
trusted pinned-page bindings. This is a preparatory component for dynamic
submission and isolated LAB probes, not a new execution or ioctl admission.
The files are not included by the selected production module/profile.

The qualified legacy memory path uses the shared default page directory.
Checking expected mappings there does not establish that no foreign mapping
is reachable. The new function examines every private PDE and every present
PT, including invalid entries, and requires exact equality with the allowlist.

## Contract

The caller owns the private directory, holds GPU/device exclusion and serializes
its page arrays and mappings. The directory must be unbound (`hw_context=-1`),
distinct from the default directory, and use zero invalid PDE/PTE values.
Inputs and workspace are trusted kernel storage; workspace must be disjoint
from inputs and pages, exclusive and never GPU mapped. This is not a user
pointer validator or a replacement for pinning/resource ownership.

Bindings must be sorted and disjoint, cover 4 KiB pages within the 32-bit GPU
address space, and specify READ, WRITE or READ|WRITE and the cache flag.
The audit checks:

- Exact PTE PFNs, access/cache flags and complete expected coverage.
- No extra leaf mappings, nonzero invalid entries or unknown low flag bits.
- PDE PFNs, PT ownership/index and actual valid-leaf counts.
- No physical aliases between data, private translation/control pages and
  default-directory translation/control pages.
- Precomputed workspace capacity; no allocation in the audit.

The driver semaphore is held exclusively for one coherent CPU tree snapshot.
The page mapping is released before sorting PFNs. All success/error paths
leave page tables, metadata, cache/TLB state and hardware unchanged; only the
exclusive workspace may be clobbered. Auditing an empty tree does not authorize
an empty job. No function binds a directory, publishes CPU writes, flushes a
cache, switches a hardware context, holds a job or grants execution permission.

Do **not** use the old `psb_mmu_set_pd_context(pd,-1)` as an unbind operation:
the setter calculates a hardware register address for negative context IDs too.
Safe publication, binding, completion and default restoration remain separate
unfinished work.

## Offline validation

`kernel/remesa/gma500/tests/test_mmu_confinement.c` includes the auditor itself
and the original qualified `mmu.h`, with explicit CPU shims for pages, mapping,
semaphore and sorting. This exercises the C checks, not Linux locking or GPU
translation. The runner checks PTE flag constants against original `psb_drv.h`.

Native, UBSan and i386 runs each pass **40,459 checks**, including cross-PDE
bindings, the last 32-bit VA page, maximum valid PFN, READ/WRITE/RW/cache flags,
one-bit PTE corruptions, missing pages, metadata/ownership/count errors,
data/control-frame aliases, extra PDEs across the entire directory and extra
leaves across every slot of a foreign PT. Every audit compares the entire
fixture before/after, checks lock/map balance and guards the workspace tail.
There are 2,109 locked snapshots per run. No UBSan errors or stderr occurred.

A private copy of the complete original `mmu.c`, with this include appended,
also compiles using the existing antiX 5.10.240 i686 kernel build command and
real headers. Its object defines `psb_mmu_audit_unbound_pd`. This is a
translation-unit build only: no module was installed or hardware exercised.
The original module sources/build command and selected production module are
unchanged.

Evidence and exact commands:

`/home/gama/sgx535-offline/private-mmu-audit-20261027/RUN.py`

`/home/gama/sgx535-offline/private-mmu-audit-20261027/RESULT.json`

The private compiled object is:

`/home/gama/sgx535-offline/private-mmu-audit-20261027/module/mmu.o`

## Remaining integration

Private-PD allocation/mapping ownership, CPU/GPU coherence, context publication,
submission references, fences, retirement and failure/default restoration must
be implemented and qualified before this audit can support any LAB operation
or general userspace draw. The unchanged PDS event controls are not launchable
merely because this component builds. Paired desktop-GL/GLES2 physical tests,
general shader/state/texture/2D support and the installable driver remain
unproved. GLXGears and the recoverable boot/rollback path are unchanged.
