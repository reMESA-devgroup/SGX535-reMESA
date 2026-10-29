# Architecture RFC draft: SGX535 rendering support in drm/gma500

Status: discussion draft, 2026-10-29. Not sent, published, or proposed for merge.
Source reviewed: `6f841fb57b7fc7e52c30e86a200857929c4525dd`.
This is a kernel architecture RFC, not a Mesa merge request or a patch series.

## Problem and scope

The project is developing an open SGX535 rendering path alongside gma500 display
support. The immediate engineering problem is to move from retained, narrowly
qualified command sequences to kernel-managed submission with explicit ownership
of memory, translations, completion, and failure state. We seek feedback on where
that work belongs in DRM and what evidence is needed before proposing a stable
rendering interface.

The current experimental implementation targets SGX535 core `0x01130000`,
revision `0x00010201`, on a retained antiX Linux 5.10.240 x86 base. It is neither
ported to a current upstream kernel nor claimed to be mainline ready. The
experiment uses source staging with pinned input hashes and fail-closed anchor
replacement. That is useful for reproducibility but is not the proposed upstream
delivery format.

## Implemented architecture

The existing private GEM memory endpoint holds device, module, power, and
exclusive-owner references. Its file/context binding controls a bounded table of
GEM backing objects and retained jobs. Jobs hold the objects and prevent CPU
leases or topology changes while execution owns them. These are project-specific
IDs and state machines; using GEM internally does not by itself establish a
standard interoperable GEM submission API.

The private loan resolves the prepared job's actual backing pages, verifies their
default-directory mappings, and builds an initially unbound page directory.
Bindings narrow read/write permissions, and CPU-only LOCAL roles receive no GPU
PTE. The confinement auditor checks the full private tree, exact mappings and
flags, and aliases between data and translation-control frames. Preparation takes
page references and retains a live owner if exceptional cleanup cannot establish
that freeing is safe.

A producer reservation establishes exclusion under the native 2D admission lock
and shared IRQ ownership protocol. The staged console path rejects GPU submission
before FIFO writes while reserved and uses its existing CPU copy helper for that
specific busy result.
Private execution lends IRQ attribution to the fixed backend while preserving the
2D producer reservation. Unexpected events remain errors, including a 2D
completion during the guarded 3D interval.

The same-call executor holds the memory mutex and MMU writer semaphore across
private-root bind, fixed-service execution, CPU ownership transfer, backend close,
default-root restoration, and source closure. It authenticates the default
snapshot before the writer interval to avoid recursively taking MMU locks. Root
publication precedes issue; CPU cache publication covers all GPU-visible backing,
including destinations. Completion invalidates GPU-written backing before CPU
visibility. This is an x86 cold-job protocol using the existing cache primitives,
not a demonstrated streaming/coherency protocol for general rendering.

Retirement requires the fixed service's full lifecycle ledger and release gate,
then successful root restoration and source closure. A source-bound failed
attempt is quarantined rather than guessed complete. Resources remain held and
the module-lifetime probe cannot be rearmed. These receipts are not Linux
`dma_fence`, `dma_resv`, syncobj, or drm_sched integration. No general asynchronous
job queue or cross-device synchronization contract is implemented in this path.

The principal source files are `remesa_private_probe.inc`,
`remesa_gem_private{,_execute}.inc`, `remesa_mmu_{private,switch}.inc`, and
`remesa_twod_{reservation,irq}.inc` in `kernel/remesa/gma500/`, integrated by
`tools/remesa/stage_twod_reservation.py`.

## Laboratory API versus a production API

The current private build registers command index 9,
`DRM_IOCTL_REMESA_PRIVATE_PROBE`, with a 112-byte versioned structure. It requires
root-only DRM admission, CAP_SYS_ADMIN, the memory enable, and a separate probe
enable; controls default to false. Only a context, eleven object IDs, and a
bounded time budget are accepted. The kernel fixes addresses, sizes, images,
commands, and hardware identity to the retained profile. Response and reserved
fields must enter as zero. ioctl success reports a structured outcome; callers
must inspect `error`, `stages`, and `retained`. It does not prove pixel correctness.

This is an isolated qualification interface. Its fixed-scene policy, global
one-shot lifetime, synchronous execution, and permanent retention after uncertain
failure do not define a production ABI. No request is made here to reserve its
ioctl number or freeze its structure upstream. A production design needs jointly
reviewed userspace and kernel semantics for contexts, buffers, validated
submission, synchronization, timeouts, device loss, and recovery. Whether to use a
DRM scheduler, fences, and syncobjs should follow that design and hardware limits.

## Open userspace and the UAPI prerequisite

The repository contains open userspace rendering and transport work under
`mesa/sgx535`, as well as retained-scene and 2D experiments. A complete upstream
OpenGL/GLES implementation, desktop 2D integration, and an installable qualified
driver remain unfinished. The private probe presently has no demonstrated full
userspace driver exercising a production interface.

DRM requires corresponding open userspace patches, reviewed and ready for a
canonical upstream project, before merging new UAPI. A smoke tester cannot meet
that requirement. We will need to identify the real userspace implementation and
its error handling, tests, and review together with the eventual kernel API.
See [DRM open-source userspace requirements](https://www.kernel.org/doc/html/next/gpu/drm-uapi.html#open-source-userspace-requirements).

## Evidence and its limits

The complete experimental `gma500_gfx.ko` builds and links for antiX
5.10.240-antix.1-486-smp as ELF32 EM_386. Its SHA256 is
`7c2237860dfa2c3b0b0f40509d518e4babc42e1dd394a0af2d8eb0e85b13e1f4`.
Final ELF inspection confirms the private functions and false control defaults.
The original and private profile symbols match the same 5,844 retained bytes.
The module was not installed and this build performed zero GPU operations.
These are recorded local build/ELF checks, not independently reproduced upstream
results. The reports are named `RESULT.json`, `MODULE_VERIFICATION.json`, and
`PROBE_PROFILE_VERIFICATION.json`; the local evidence index is kept separately
from this draft. They are not presented as an already published artifact bundle.

The handler model records 36 scenarios across native, UBSan and i386 variants;
GEM, crypto, grammar, borrower, source, and execution dependencies are modeled.
It exercises controller decisions, not the integrated hardware dependencies.
The source-level handler tests and userspace qualification client are separate
development work; their presence cannot establish hardware qualification.

Earlier physical GLXGears and native 2D qualification belongs to different paths.
It does not validate this private MMU transition. Private-root execution,
restoration, cache visibility, independent output/guard readback, and failure
retention still need physical qualification. Compilation and modeled receipts
cannot establish those facts.

## Review findings before broader deployment

The accompanying [source audit](sgx535-kernel-audit-20261029.md) identifies a teardown
integration gap: PCI removal still calls the legacy MMU destructor without
consulting the retained private owner. Keeping a drm_device/module reference does
not prevent that explicit teardown. This needs a device removal/device-loss
strategy before the retention argument covers the entire driver lifetime.
This is a high-priority blocker for private hardware bring-up, established by
source inspection; no use-after-free or removal fault was reproduced on hardware.

Simply postponing the existing unload function until the last device reference
is released would preserve some memory but leave hardware teardown unsafe. That
function performs MMIO, PCI configuration, IRQ, and runtime-PM operations. A late
callback could act on a device already rebound to a new driver instance. Existing
GEM fault and destruction paths also pin/unpin GTT mappings and can touch hardware
after file closure. They need explicit device-loss semantics, not only an ioctl
admission check.

A candidate direction is to separate active detach while the driver still owns
the hardware from later destruction of software state. Detach must block new
admission, synchronize in-flight accesses, shut down callbacks and IRQ handling,
and establish a hardware stop or isolation guarantee before reclaiming anything
the GPU may reference. Final software cleanup must not touch hardware or global
ownership belonging to a later device instance. Retaining quarantined pages
cannot depend on waiting indefinitely for a retirement receipt that the
implementation intentionally does not produce. This is a design requirement,
not an implemented recovery sequence.

After a failed attempt the MMU semaphore is released while the private root may
remain active or uncertain. The staging wrapper defers ordinary flushes, but
this alone is not a proof that every MMU writer and lifecycle path respects the
retained root. The complete call graph needs review. A deliberate permanent
quarantine also needs documented user-visible wedging and administrative recovery
semantics before this can become a production feature.

## Questions for maintainers and next steps

1. Is extending drm/gma500 the appropriate home for SGX535 render support, and
   which parts of the display/render ownership model should change first?
2. What reset, device-loss, removal, and wedging guarantees are required for this
   hardware, given that current qualification intentionally retains uncertain
   execution instead of synthesizing recovery? In particular, where should
   admission/removal synchronization live, and what hardware stop or isolation
   guarantee permits removal to finish without freeing possibly referenced
   translations or waiting forever for a quarantined job? How should active
   hardware detach and deferred software destruction be divided so an old
   instance cannot perform MMIO, runtime-PM changes, or GTT updates after rebind?
3. Which submission and synchronization model best matches a single global MMU
   context and shared native 2D engine? Is an initial fully serialized design
   reasonable while per-client isolation is established?
4. What scope of open userspace and conformance/error-path testing should accompany
   an initial production UAPI discussion?

The next implementation work is to close teardown/lifetime gaps, expand actual
dependency fault testing, and complete a bounded qualification client with
independent readback. Private hardware bring-up remains blocked by the lifecycle
gap; the probe remains disabled. Any later physical qualification must preserve
the known-good boot setup and rollback.

Separately, forward-port onto an agreed current kernel/subsystem tree, replacing
source-staging anchors with reviewable kernel changes and checking contemporary
GEM, locking, MMU, PM, IRQ, and UAPI conventions. Then build and test that target;
the 5.10 result says nothing about current-kernel compatibility. Split a future
series by coherent problems with self-contained rationale and validation, as
described in [Submitting patches](https://docs.kernel.org/process/submitting-patches.html).
Recipients, patch count, authorship, sign-offs, and DCO certification must be
established by the actual contributors at submission time; none are asserted here.
