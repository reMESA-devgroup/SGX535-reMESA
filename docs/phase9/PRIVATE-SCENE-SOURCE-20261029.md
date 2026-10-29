# Eleven-role borrowed scene, service and source qualification

The earlier private GEM execution test used three resources and modeled the
service/source callbacks. This qualification exercises the retained eleven-role
borrower, stream contract, fixed service, source guard and nohash terminal checks
together. These are the original actual sources, compiled with the existing
1280x800 depth profile. The backend stage callbacks and status inputs are still
modeled; no GPU, MMIO, physical mapping, rendering or readback is claimed.

The fixture takes addresses/sizes from the retained qualified stream profile.
It uses the real requirement lookup, initializes the contract's CPU user images,
extracts CONTROL commands and invokes the actual stream borrowed-owner initializer.
Large TA pools contain page metadata only; no full 32MiB data allocation is needed.
All frame/image initialization is test input and is never installed in the driver.

The actual service generates and processes its bootstrap, TA, raster and release
sequence. The modeled backend preserves stage order and invokes the real source-
write gate before simulated execution. Synthetic TA/end-render/memory-free status
inputs pass through the actual source pending/sample callbacks and service
observers. The fixture never manually writes the session's retired phase, event
ledger or capsule terminal state. This distinguishes it from the earlier isolated
source harness, which explicitly assigned those fields to test source bookkeeping.

The normal case completes two distinct operations under one source lease. Each
requires the service's RETIRED phase, exact sequence, event mask7, may-release gate,
source finish_nohash and source unbind. Borrower teardown refuses while its service
observer remains attached, then succeeds after detachment. Source settling and the
original no-rearm rule are checked. The nohash capsule has no captured color and
fails the LAB capsule-success predicate, as required.

Fault cases inject an unknown status, omit raster completion, change sequence,
change task, or add pending events after retirement. They exercise rejection and
retained model state without inventing recovery or clearing the original gates.
No legacy admission calls are used once the stream source lease is active.

## Results and limits

`/home/gama/sgx535-offline/private-scene-source-20261029/v2/RESULT.json` records
native, UBSan and i386 builds, run sequentially. Each variant passes:

| Case | Checks | Result |
| --- | ---: | --- |
| Two normal operations | 135 | Service retirement and nohash source close |
| Unknown status | 77 | Rejected |
| Missing completion | 84 | Rejected |
| Wrong sequence | 78 | Rejected |
| Wrong task | 78 | Rejected |
| Pending after retirement | 85 | Retained despite service retirement |
| Event after source unbind | 89 | Source settling refused |

`RUN.py`, generated model types, layout inputs and original source copies are
retained alongside the result. This is component composition evidence. It does
not invoke the new private GEM execution function, actual MMU switching or the
actual hardware backend; those have separate source/model tests. An integrated
physical candidate still needs to prove the whole path on SGX535 rev121, with
submission, completion, retirement, CPU visibility, readback and buffer guards.
No full OpenGL/GLES2, desktop2D, VA-API or installable-driver completion follows
from these CPU results.

The final case exposed a missing integration gate in the private GEM executor:
its first version unbound the source but omitted the original end-boundary
`remesa_stream_source_settled()` check. That check now runs before the final
ACKNOWLEDGED/execution-retired receipt. An error there retains the job, loan and
backing even though root restoration and source unbind already succeeded. The
actual source regression proves that a post-unbind event fails settling; the
separate GEM executor model checks that this failure blocks release.
