# Retained GEM authentication for native 2D copies

`remesa_gem_twod.h/.inc` connects the shared2D packet preparer to the existing
kernel GEM job service. It authenticates a retained high-level request against
the endpoint's own live job and held allocations, then reconstructs the FIFO
packet. It is not an installed ioctl or executor and performs no device action.

The private command has a schema marker and a copy request, not arbitrary GPU
words. The kernel checks the command size/schema/reserved fields, ticket,
PREPARED state, absence of an attached/competing stream, exact allocation IDs
and binding count, GPU VAs, complete BO lengths and initialized source backing.
Same-BO overlap holds one allocation. Different BOs require two matching
bindings, in source/destination order; high bits cannot truncate into an ID.

The 2D preflight now authenticates ownership holds and exact cached read/write
mappings through `psb_mmu_verify_cached_rw_pages`. Under the MMU read semaphore
and table spinlock it checks the complete PDE and PTE words, table ownership,
index/count and 20-bit physical frame bounds. It rejects permission, cache,
unknown-bit and frame changes. This matches the existing cached-RW mapping
constructor; it does not change the older generic 3D verifier. It reads tables
without modifying them, allocating, publishing a TLB or granting execution.
CPU/GPU coherency and authentication of the hardware-selected root remain
part of native issue integration.

The pinned-frame alias check sorts PFNs from the held page arrays. This checks
a distinct property rather than repeating the virtual-to-PFN walk. It rejects
physical aliases within/across bindings, including when virtual addresses are
disjoint. The source/destination packet and logical bounds are rebuilt by the
same `remesa_twod_copy_prepare` implementation used by userspace. The shared
implementation now also compiles directly with kernel types/errno and the
kernel's GNU89 flags; no separate handwritten kernel packet encoder exists.

There is no framebuffer scan, pixel comparison, hash, cache maintenance, MMIO
or job-state transition on successful preflight. Output remains unchanged on
failure. A mapping-authentication fault uses the existing quarantine path and
retains backing; semantic request errors leave the unissued job cancellable.
The existing generic prepare call has its own mapping checks too: this change
does not claim that the entire lifecycle has only one verification pass.

## Tests and compilation

Evidence root:

`/home/gama/sgx535-offline/twod-gem-20261027/`

`RUN.py` includes the original qualified GEM-service C implementation with
explicit host shims and the new preflight include. Native, UBSan and i386 runs
each pass144 parent-process checks, plus joined child fault tests. They cover
1280x800,640x640,641x479, single-BO overlap, immutable copied commands, bad
schema/reserved data/IDs/addresses/lengths/coordinates, the eight-pixel erratum,
allocation failure, incompatible streams, missing source initialization and
stale tickets. Aliased-PFN and PTE-mismatch faults run in child CPU models;
the latter proves quarantine and refusal to cancel/close while resources are
held. Child process exit is not represented as GPU retirement.

Every successful request checks that no flush/barrier occurred. Attempting
the existing issue entry still returns EOPNOTSUPP: no executor was installed.
The first fixture failed because it expected a stale ticket's state query to
succeed; that expectation was corrected to ENOENT without changing driver
checks. Initial build/run artifacts are retained.

The complete original antiX GEM translation unit, with the shared encoder and
new include appended in a private copy, compiles with actual antiX5.10.240
i686 flags/headers and defines both new symbols. Original service sources and
build command remain unchanged. `MESA_FLAGS_COMPILE.json` also records the
current shared encoder compiled with actual Mesa i386 flags.

`packet-regression/RESULT.json` preserves the existing366 checks and38 original
kernel packet comparisons in each native/UBSan/i386 variant after the kernel
port. These are CPU tests and compilation, not physical2D rendering approval.

## Separate 2D completion is required

The existing GEM lifecycle mask7 requires TA, end-render and3D memory-free.
Those events must not be fabricated or relaxed for2D. This change preserves
the3D completion contract and installs no alternate source.

The actual gma500 sources provide a concrete2D route: `psb_reg.h` defines
`_PSB_CE_TWOD_COMPLETE` at status1 bit27; `psb_irq_postinstall` enables it, and
`psb_sgx_interrupt` observes it. The qualified fixed3D backend intentionally
treats that bit as benign and excludes it from its3D ledger. A2D adapter must
capture its own owned operation event in the existing pre-ack IRQ interval,
with a stale-event boundary and authenticated command/cookie association.
Changing the old3D benign mask is not the intended solution.

Next: native2D emission, a distinct completion/visibility/retirement contract,
FIFO/request-base serialization, permission/cache validation and uncertainty
holds, followed by physical readback/guards on rev121. No new acceleration is
advertised before that evidence. One read-only Mini12 availability check timed
out; the user subsequently reported that it was off and would turn it on.
No MMIO, upload, boot change or GPU operation occurred in this step.


## Exact mapping-access follow-up

Evidence: `/home/gama/sgx535-offline/twod-native-20261027/RESULT.json`.
Native, UBSan and i386 each pass 614 checks executing the actual new MMU
verifier with explicit CPU page/locking shims, plus 147 GEM preflight checks.
Coverage includes cross-directory boundaries, the last 32-bit page, all bit
mutations in sampled PDE/PTE words, missing/foreign tables, bad counts, NULL
pages and out-of-range PFNs. Full fixture snapshots remain unchanged and
map/lock calls balance. A third joined GEM child injects access failure and
confirms quarantine and retained holds. These are CPU tests, not GPU execution.

Both complete original antiX GEM and MMU translation units compile in a private
copy using their retained actual i686 build commands. Original source and
command hashes remain unchanged. No executor or ioctl is registered.

Mini12 is reachable again: `MINI12_READONLY_BOOT.json` records boot
`2df880f9-ae95-4c98-963f-543398a19b58`, kernel
`5.10.240-antix.1-486-smp` and PCI 8086:8108. This is metadata only; PCI
revision is not a new SGX core-revision measurement. No module installation,
MMIO or GPU job was performed by that check.

The IRQ source confirms that `psb_sgx_interrupt` acknowledges the complete
observed status words. The future owned 2D event capture therefore must happen
before that acknowledgment, under the existing shared IRQ lock. The FENCE
packet word alone does not establish completion, visibility or retirement.
