# Join standard GEM destruction and retain lost objects

The private profile now tracks every standard GTT-backed GEM allocation that
can reach `psb_gem_free_object`. The two constructors are `psb_gem_create` and
console `psbfb_alloc`. Private memory endpoint and legacy frozen-owner GEM
containers are different types, use explicit release protocols, and are not
passed to this tracker.

Each tracked allocation owns a DRM device reference and a module reference.
The tracking node and flags are appended to `gtt_range`, preserving existing
field offsets. A mutex protects the allocation list. No extra object, hash,
pixel readback or per-frame checkpoint is allocated. This bookkeeping runs on
allocation/destruction and device removal, outside the render frame loop.

## Destruction contract

A destructor admitted by `drm_dev_enter` performs live cleanup inside that
read interval; unplug must join it before destructive unload. Tracking refs
are dropped only after cleanup and `drm_dev_exit`, using a saved device pointer
because the original cleanup frees the range.

Before releasing GEM backing, the wrapper drops an mmap pin through the
existing `psb_gtt_unpin`. That native API returns void and leaves `in_gart`
unchanged when blitter-idle fails. The wrapper requires the count to decrement
exactly once. Failure, an invalid mmap pin, or remaining nonstolen pins retains
the complete object and tracking refs. The original destructor then sees the
mmap pin already cleared and does not perform a second unpin/wait. This avoids
the original ordering that released GEM state before finding an unpin failure.
The native unpin implementation itself is unchanged.

After unplug, no original destructor, GTT unpin, backing release or MMIO runs.
The zero-GEM-kref allocation stays on the driver's list and is marked orphaned.
Its GEM kref is never resurrected. Mmap-offset metadata, GEM/shmem state,
pages, GTT reservation, allocation metadata, device and module ownership remain
retained. There is no retry or forced free.

Device retention and early rebind admission now consult both the existing
private endpoint and this GEM list. Even an object with a positive GEM kref at
unplug retains its device dependency graph; it need not wait for its destructor
to run. An orphaned object from failed live cleanup also retains the graph.
The tracking lock is never held across GTT/MMU work, original destruction, or
the unplug SRCU join. Ownership queries only use DRM entry/exit checks; they do
not access hardware.

Construction failures use balanced GEM puts: handle creation failure and console
creation's error path no longer bypass the tracked destructor with raw range
free. Failure before successful GEM initialization remains on the untracked
range cleanup path.

## Qualification and recovery limits

This is conservative quarantine, not a complete production recovery policy.
Console GEM allocations now pin the module. Ordinary `rmmod` may return busy
while such objects exist; unbinding after unplug quarantines them and blocks a
new binding until reboot. There is no automatic collection or declared GPU
retirement. Do not force module removal. Reboot remains the recovery boundary
until actual GPU stop/isolation and safe collection have been implemented and
qualified. No boot configuration or installed module was changed by this work.

The change resolves the two modeled standard-GEM destructor schedules. It does
not establish physical unplug safety for every KMS, console, ACPI, PM or legacy
owner callback. Nor does an observed native pin-count decrement independently
prove MMU invalidation on hardware. Those paths, complete GL/GLES/desktop 2D and
an installable qualified driver still require work and physical validation.

## Offline evidence

Evidence root: `/home/gama/sgx535-offline/private-gem-lifetime-20261029/`.

The tests compile actual tracker/wrapper fragments, original live destructor,
GTT-free body, common GEM constructor and console allocator. DRM/list/GTT APIs
and reduced metadata are modeled; entry joining uses real pthread scheduling.
They are not kernel SRCU or physical GPU tests. The unload/probe test separately
compiles those actual function bodies and covers retention/rebind from GEM alone
as well as the private endpoint.

The initial `v1` tests cover the entry/join and lost-object implementation before
the failed-unpin handling was added. `v2/gem-test` retains a failed compilation:
its fixture incorrectly modeled native signed `in_gart` as unsigned, triggering
`-Werror=sign-compare`. The original fixture and compiler log are preserved.
The fixture was corrected to native signedness; no driver type was changed.

Final artifacts:

- `v4-gem-test/RESULT.json`: actual GEM implementation and constructors,
  native/UBSan; preloss unpin join, postloss quarantine, allocation/init/handle
  failures and console allocation. Separate modes cover failed idle/unpin,
  remaining nonstolen pins and invalid mmap pin counts. Freed fixture metadata
  is poisoned; this is not an actual allocator or physical use-after-free test.
- `v2/unload-test/RESULT.json`: actual unload/probe, native/UBSan; endpoint-only
  and GEM-only retention/rebind controls, with modeled resource APIs.
- `v2/orphan-test/RESULT.json` and `v2/entry-test/RESULT.json`: previous
  file/device revocation and CPU-entry/join models pass, compat both ways.
- `v4-i486/RESULT.json`: five static i486/QEMU fixtures pass; GEM also runs all
  three failure modes. Compiler ceiling 768 MiB virtual address space; QEMU
  ceiling 6 GiB for guest mapping reservation, CPU/time bounded.
- `v2/full-module/RESULT.json`: full i386 Kbuild/modpost/link, 153.848 seconds,
  one build job, 768 MiB process address-space ceiling. Kernel cache keys and
  observer symbols unchanged; module uninstalled and physically unqualified.
- `v2/full-module/MODULE_VERIFICATION.json`: read-only ELF requires the new
  tracking/destructor/retention functions and earlier guards. Six controls are
  zero-initialized BSS booleans, not live N/N evidence.
- `v2/VERIFIER_NEGATIVE.json`: previous module rejected when GEM lifetime is
  required; binary unchanged. `v2/NORMAL_PROFILES.json`: five normal generated
  profiles unchanged against `9313a11`. `v2/SOURCE_IDENTITY.json`: 204 staged
  input files match built sources, also checked after Kbuild. `v2/PROTECTED.json`:
  all 31 protected files unchanged.

Uninstalled module SHA-256:
`47ab5bb7c5e485737788b92180f6cdfcadcceab9d319c68fc69483b720042458`.

Remaining direct KMS/GTT operations include `gma_display.c` pin/unpin callers
and console rolling. This component guards the standard GEM destructor and its
CPU interval; it does not guard those entire external callbacks. A final unplug
review must cover them before any physical qualification.
