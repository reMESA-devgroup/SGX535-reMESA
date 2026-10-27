# Native 2D GEM job lifecycle

`kernel/remesa/gma500/remesa_gem_twod_job.h/.inc` implements the retained GEM
lifecycle for a native 2D copy without changing the existing 3D completion
mask or installing an ioctl. It is kernel integration code; the native MMIO
adapter is still missing and no physical 2D acceleration is advertised.

`remesa_gem_twod_issue` runs the existing exact 2D preflight under the endpoint
mutex, obtains the held page arrays and reconstructed packet, publishes CPU
writes with cache maintenance and a DMA write barrier, and reserves the
endpoint executor slot. It marks the job ISSUED before the native start callback
because that callback may partially issue even when returning an error.
Neither page arrays nor CPU views are allocated here. Source and destination
are both flushed before possible GPU writes, including the single-BO overlap
case, where the allocation is flushed once.

`remesa_gem_twod_wait` accepts only an exact ticket/cookie match, the owned
TWOD_COMPLETE event (bit27), empty FIFO and nonbusy blitter, with no status2
events. The adapter must capture status before the existing IRQ acknowledgment,
exclude only the master interrupt bit, associate events with its operation,
check BIF fault state and establish a stale-event boundary. An idle register
snapshot alone is insufficient. The FENCE command word and old 3D lifecycle7
are not accepted as 2D completion.

`remesa_gem_twod_publish` requires COMPLETE, reauthenticates exact mappings,
finishes native device coherency, and invalidates destination CPU cache lines
before a DMA read barrier and VISIBLE state. It does not read pixels. Source
cache lines need no post-GPU invalidation for a distinct source allocation.

`remesa_gem_twod_retire` requires VISIBLE, reauthenticates mappings and invokes
native retirement while all job holds remain live. Only successful native
retirement permits detaching the endpoint, dropping holds and freeing the
copied command. The trusted caller retains the job storage and adapter/context
until successful retirement; start, wait, visibility, mapping or retirement
uncertainty leaves the job attached and quarantined. There is no reset or
forced release fallback. Held resources block CPU leases, cancel and close.

The reserved executor has non-NULL rejecting callbacks. Accidental use of old
3D wait/publish/acknowledge functions therefore fails closed instead of
dereferencing NULL, producing a fake 3D receipt or releasing a 2D job. The old
3D functions and mask remain byte-for-byte unchanged in their original source.

No hashes, checkpoints, framebuffer comparisons, dumps, readbacks or diagnostic
waits occur in these 2D functions. LAB tools may read visible output separately.
Buffers can be reused after retirement without reallocation or reclearing.
Essential cache and synchronization work remains; its physical cost has not
been measured for this new path.

## Evidence and remaining integration

Evidence directory: `/home/gama/sgx535-offline/twod-lifecycle-20261027/`.
`RUN.py` executes actual GEM service plus 2D source in native, UBSan and i386
CPU models. Each reports 621 parent checks plus 21 joined fault/cross-engine
child cases. Successful 1280x800, 640x640, 641x479 and same-BO overlap fixtures
each reuse their allocations for three operations. Callback assertions verify
the ISSUED boundary, held pages, cache publication order and retirement order.
Fault cases cover start error/zero cookie, timeout, every ticket component,
cookie, absent/extra/wrong-engine events, FIFO/busy/status2 rejection, failed
visibility/retirement, mapping faults and legacy 3D calls. Fault children retain
holds and reject cancel/close; process exit is not hardware retirement.

The initial test incorrectly compared one serialized packet byte with a DWORD;
only that test assertion was corrected. Its failure is retained in
`FIRST_FIXTURE_FAILURE.txt`. An earlier successful build's unused-parameter
warnings were removed explicitly in the rejecting callbacks, without disabling
warnings. Subsequent builds and runs must be inspected in RESULT/CLOSEOUT.

The actual complete antiX GEM translation unit with all 2D includes appended,
and the MMU translation unit with exact-access verification appended, compile
using original i686 commands in a private copy. Original input hashes are
checked. Existing GEM and 3D stream tests also execute against that appended
translation unit; their synthetic executor remains explicitly CPU-only.
The exact MMU suite retains 614 checks per variant.

Next required component: native hardware/root authentication, FIFO/request-base
serialization against old producers, pre-ACK owned IRQ capture, device-side
visibility and safe restoration on retirement. Then rev121 physical copy
tests at 1280x800 and 640x640, occasional 641x479, with submission, completion,
retirement, readback and untouched guards before Gallium dispatch. No module
installation, GPU submission, new FPS or complete GL/GLES support is proven by
these CPU tests.

The old producer exclusion needs actual integration, not an assumption:
`sgx535_gma500_exclusive_claim` only arbitrates the private scene owner. The
original `psbfb_2d_submit` independently takes `dev_priv->lock_2d`, records a
source-producer notification and writes its packet; it does not consult that
claim. Its timeout calls `psb_spank`, which resets both 2D and 3D and rewrites
the request base. A new native reservation must be checked by this old path
under the same producer lock before borrowing BIF_TWOD_REQ_BASE. Holding a
spinlock across split-phase calls is not a valid substitute. Do not issue a
native packet until this exclusion and the pre-ACK event owner are wired and
offline-tested. The new lifecycle alone intentionally does not authorize it.
