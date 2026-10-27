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

The existing binding verifier authenticates every PTE/PFN and ownership hold.
This verifier checks VALID/PFN identity, **not all access/cache flag bits**;
permission/cache validation remains part of the unfinished native issue
contract. The backing service itself creates cached read/write mappings and
does not expose caller-selected PTE flags. This preflight grants no execution
permission based solely on those constructor facts.

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
