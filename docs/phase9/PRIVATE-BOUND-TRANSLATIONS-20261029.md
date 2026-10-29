# Private bound translations match the retained job

The private path now has an explicit pre-issue translation/resource validator.
It validates the authenticated private owner instead of asking whether the
default PD is bound. The legacy default-directory path and normal rendering
admission remain unchanged. The new check is not yet wired into an executable
private-context job; no hardware or framebuffer result is claimed.

Evidence root:
`/home/gama/sgx535-offline/private-bound-translations-20261029/`.
Authoritative current-source results: `v2/RESULT.json`, `v2/gem/RESULT.json`,
`v2/GUARD_RESULT.json`, command logs and qualified-source bindings.

## MMU identity and immutable closure

`remesa_mmu_private_validate_bound_locked` requires the already-held MMU write
semaphore plus retained owner/device/producer/IRQ/power contracts. Expected
bindings must match the private owner's copied VA-sorted closure exactly:
count, VA, page count, access/cache policies and each held page identity.
These are kernel page references, not reads or comparisons of rendered data.
Foreign/mismatched expected bindings fail without poisoning an otherwise valid
owner. The established32-binding limit is explicit.

After association it checks the active/nonuncertain private context, expected
inactive default context and retained restore directory, SGX535/rev121 identity,
exact root, control and existing pre-issue quiet conditions. An actual bound
state failure marks uncertainty, retaining the owner. It acquires no lock,
allocates nothing, rewrites no PTE, performs no cache maintenance, and issues
no command/ACK/reset. The private tree was already audited at preparation/bind;
caller exclusion and immutable held ownership remain required. This does not
replace shader safety or prove a complete command dependency graph.

## GEM authority and views

`remesa_gem_private_validate_locked` requires endpoint mutex then MMU write
semaphore already held. It authenticates the exact loan/ticket/attachment,
prepared-or-issued pre-fire state, retained producer/MMU guard and complete
ordered job views. Each view must match the held object's current generation,
pages/count, VA and size; a nonnull CPU view must be the job-owned cached mapping.
It does not call a default-PD getter or a PTE walker that reacquires sem.

Preparation now copies the validated ID/access/cache rules into the retained
loan. Later caller changes cannot modify this authority. The validator derives
bindings again from actual held backing, preserves policy/page pairing while
sorting by VA, then calls the MMU validator. It never accepts caller PFNs or
additional GPU addresses. Mismatched views are rejected; corrupted actual GEM
backing uses the existing quarantine path. Holds/guard/owner remain retained.
The reserved executor still refuses legacy issue: validation success grants
no new runtime permission.

## Qualification

Native, UBSan and i386 each pass49918 shared root/association checks, including
the existing root regression suite (49737 intermediate report with its outer
invocation check). These are shared counters, not independent counts to sum.
They cover unbound refusal, correct bound association without lock recursion,
address/policy/cache/count/page mismatches, limits, wrong root/events/revision,
unchanged write/cache counters and retention after actual state failure.
GEM models pass405 checks per variant, including caller-rule mutation after
preparation, exact/corrupted views, count mismatch and unchanged CPU mapping/
cache/barrier counters. Their MMU/guard callbacks and kernel/GEM primitives
are modeled; the actual MMU source is exercised separately.

Existing guarded IRQ, reservation and native2D CPU regressions still pass.
Six actual antiX units compile and partially link with private handoff and
normal copy/vendor/fill flags enabled; all new private/GEM/2D symbols resolve.
This is not a full module link/installation or physical isolation approval.
All31 protected files and original checkpoint sources remain unchanged.

v1 initially passed the basic association/GEM tests; v2 adds the explicit
limit and side-effect counter checks. The first combined stager attempt
refused to overwrite v2/module, already used by the isolated MMU build.
That refusal and original entry point are preserved; the combined build uses
a fresh v2/full-module directory. No evidence was deleted or overwritten.

## Remaining integration

The authenticated check and already-locked maintenance are available. The
fixed backend's private publication branch still needs to use those APIs via
a validated retained job adapter. Default-MMU writer exclusion must span the
actual bind/issue/owned-completion/visibility/retirement/restore interval, with
safe retention on uncertain execution. Merely wrapping the old backend in
sem would still hit its default-only getter and recursive maintenance call.
Physical textured drawing, full GL/GLES2, accelerated desktop, VA-API hardware
decoding and an installable driver remain unfinished. No FPS gain is claimed.
