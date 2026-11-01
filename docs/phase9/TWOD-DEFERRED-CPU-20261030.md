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

The comparison below qualifies this bounded component; adoption into the default path remains pending. An
installed general GL/GLES/desktop 2D driver still requires the outstanding
generated-scene/native admission, presentation and installation work.

## Physical A/B/A comparison

The candidate module SHA-256
`1e2e4e3a58c61e14ab5f1b23f8b7b4c1b2db4f5dab2f77c0d7f1711803478ae6`
ran on Mini12 boot `70dd7bf6-c517-4bf1-9d22-fae779900a5f`.
Nine owned-factory contexts completed441 operations: one copy and48 fills
per context, across three sizes and conservative/deferred/conservative phases.
Each context reported SGX535 core01130000/rev00010201, operation49, stages31,
no retained/uncertain/error outcome, one session close and final owned-FD close.
All four guarded source/destination readbacks per context matched the previous
physical oracles byte-for-byte. These inspect logical buffer/guard bytes after
each API sequence, not page padding or every intermediate operation.

| Size | Conservative before | Deferred | Conservative after |
| --- | ---: | ---: | ---: |
| 1280×800 | 25.775 ms | 24.671 ms | 27.631 ms |
| 640×640 | 11.571 ms | 8.763 ms | 10.157 ms |
| 641×479 | 7.468 ms | 6.235 ms | 7.865 ms |

These are mean native fill callback wall times, including checks, cache,
synchronization and retirement. Relative to the mean of both conservative
controls, reductions are7.61%,19.33%,18.67%. Total client times, including
uploads, four readback/check/capture sequences and cleanup, changed by
+0.27%,+1.43% and−0.93% respectively when expressed as reductions. The short
sequences and variable CPU/readback costs do not establish a sustained total
performance gain. The option remains default N. No desktop or glxgears FPS
is measured by this test.

Evidence: `hardware/MATRIX_COMPLETE.json`, `hardware/COMPARISON.json`, and
`hardware/cases/*/VERIFIED.json` under the root above. The same original
factory client is hash-pinned; diagnostics/readbacks remain outside native
loops. Initial SSH verification timed out during boot; subsequent observation
confirmed the new identity and restored stock GRUB configuration before any
GPU work. The first conservative1280 case predates an additional completion
policy sample; its preflight default-N, absence of a policy write to Y and
final controls remain recorded. The other eight also sample the selected
policy at client completion.

All nine supervisors were terminal and all controls N in their final receipts.
Stock GRUB/default and empty next entry were restored before testing. A later
SSH attempt to request the stock reboot could not connect, followed by
`No route to host`; no acknowledgment of that reboot exists. Consequently
current remote state and actual stock return are **not verified**. Inspect
the existing namespace and current boot before taking any further action;
never replay GPU cases or blindly repeat a reboot based on the transport error.
