# Deferred CPU invalidation for normal native 2D

Status: optional candidate; model-tested, not physically qualified. No FPS or
desktop acceleration result follows from this change.

The normal 2D path flushes every bound source/destination before issuing DMA.
It also waits for completion, performs device visibility maintenance and
retires the authenticated job. Those operations remain unchanged.

The candidate can defer the additional destination **CPU** cache invalidation
after device visibility. A per-backing pending bit is consumed under the memory
mutex before any CPU lease, including writable/partial-write leases, before
returning internal job views, and before returning pages through unpin or
discard. Explicit SYNC also flushes and clears the pending bit with a read
barrier. Already cached internal views are covered. Mapping creation failure
after invalidation does not undo the completed flush.

The candidate also refuses 2D admission while either backing retains an
internal job mapping. Such mappings can outlive cancellation; checking only
userspace leases would leave an internal CPU alias across the GPU interval.
Unpin revokes that mapping. The guard applies in both runtime policies so a
concurrent parameter change cannot invalidate the admission assumption.
Existing exclusive backing holds prevent another prepared job from acquiring
those pages, and the issued job cannot request CPU views in its issued state.

This does not assume that speculative CPU cache fills are impossible. The CPU
boundary invalidation remains mandatory even when no application read occurred
between GPU operations. This change does not skip prefire flushes, PTE/PFN or
address/permission checks, holds, device-side cache maintenance, completion,
visibility, retirement, fault quarantine or uncertain-job retention. It does
not modify the 3D private session, DPM initialization, cold clears or shaders.

## Selection and conservative comparison

The stager requires an isolated normal native-copy/fill profile:

```sh
python3 tools/remesa/stage_twod_reservation.py \
  --native-source /absolute/qualified/native/module \
  --output /absolute/new/candidate \
  --with-native-copy --with-native-fill \
  --with-twod-deferred-cpu-invalidation
```

LAB, vendor-LAB, private-MMU and missing-native-copy combinations are refused.
Other profiles retain their original generated sources and default job body.
The new root-only parameter `remesa_twod_defer_cpu_invalidation` defaults to N.
Pending invalidation is honored independently of the current parameter value:
turning it off cannot bypass an existing CPU boundary obligation. Conservative
publication flushes and clears the destination pending state normally.

The runtime selection permits an A/B/A comparison on the same candidate module
and fresh recoverable boot: conservative, deferred, then conservative again.
Each phase must finish every operation and close its context before changing
the selection. Readback/oracle verification remains outside timed native loops.
Test paired1280x800/640x640, with641x479 as the extra case. Preserve identity,
completion, retirement, guards, source bytes and all normal receipts. Restore
all controls to N and the stock boot default with no pending next entry.
No test may rearm the consumed private-session boot.

Measure native-call time, the following CPU access/readback and total sequence
time separately. A workload reading the result after every operation still
pays the invalidation cost at that CPU boundary; moving it outside the ioctl
is not by itself an end-to-end speedup. No glxgears or X11 FPS gain is implied.

## Offline evidence

Root: `/home/gama/sgx535-offline/twod-deferred-cpu-20261030/v1`.

`alias-final/RESULT.json` binds native, UBSan and static i386 tests. The
new fixture runs160 modeled retired operations, checking deferred and
conservative publication, runtime changes, pending invalidation after disable,
read/write leases, SYNC, cached internal views, unpin, close and failed-vmap
boundaries. An injected DMA/cache model exposes the GPU-written bytes only
when invalidation occurs; it is not a physical cache-coherency oracle.
Existing lifecycle/21 fault models, GEM preflight, copy and fill regressions
also pass. `FINAL_DEFAULT_PROFILES.json` checks six unchanged default source profiles,
the original default 2D job body and31 protected historical files.

Earlier harness failure in `runtime-final/` is preserved: its simplified
module-parameter macro did not reference the registered variable, producing an
unused-variable error. The final test-only shim models that registration
reference. No warning or runtime validation was disabled to pass that test.

Physical comparison and adoption into the default path remain pending. An
installed general GL/GLES/desktop 2D driver still requires the outstanding
generated-scene/native admission, presentation and installation work.
