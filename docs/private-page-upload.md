# Direct page upload and native C session qualification

`remesa_linux_memory_upload_pages` transfers complete caller-owned pages into
an existing allocation using one write lease, bounded WRITE requests and one
unlease. It avoids the old CPU-map path's full allocation READ before writing.
It does not allocate, read back, hash, query INFO, compare whole buffers or issue
SYNC. Publication remains the integrating executor's responsibility. The
qualified cold private executor already publishes these CPU writes before GPU
execution; the collector does not add a second PDS flush between frames.

Local checks reject zero/misaligned/out-of-bounds/overflowing ranges, unknown
IDs, poisoned objects and outstanding host maps. During an active private LAB
session only its sealed PDS ID can be uploaded; the kernel's unchanged typed
page guard still checks every write. Initial generic uploads happen before the
session. A known lease denial before any write returns its error. An uncertain
lease, any failed WRITE after lease acquisition, or failed unlease poisons the
endpoint and retains every allocation. The helper never guesses an unlease,
retries a partially written batch or permits subsequent GPU submission.

## Offline evidence

`/home/gama/sgx535-offline/private-page-upload-20261030/v1/tests-final/` tests the
actual C helper with modeled kernel calls, natively, with UBSan and as static
emulated i486 code. Exact page offsets, untouched surrounding pages, no READ/
SYNC/INFO requests, 24 lease/WRITE/unlease fault cases, CPU leases, ranges and
active-session PDS-role restrictions are covered. Existing session and native
copy/fill/retained transport regressions are included. GPU0.

`/home/gama/sgx535-offline/private-c-pds-20261030/v1/tests-final/` builds
`tools/remesa/private_session_c_probe.c` and exercises seven collector scenarios
through `/dev/null` plus interposed ioctls. Three distinct numbered jobs, pose
updates, raw receipts/readbacks, successful closure and uncertainty retention
are checked. Modeled COLOR/Z are not rendering evidence. Separate tests execute
the actual kernel typed PDS guard against both archived native poses: all pages
pass, only offsets 24576 and 36864 differ, metadata mutation and NaN are rejected.
All three CPU variants pass. Earlier compile failures in the test-only harness
(missing stdbool and misleading indentation) are preserved, then corrected
without weakening compiler warnings or runtime validation.

## Explicit LAB collector

The C collector is invoked only with:

```text
private_session_c_probe --execute DEVICE ASSETS NEW_OUTPUT NEXT_PDS_DIRECTORY
```

It does not install/load a module, change controls, reboot or render on the CPU.
It creates the eleven qualified BOs and two guards once, uploads inputs once,
opens one bounded session, and executes pose1/pose2/pose1. The two dirty PDS pages
are planned before opening the device and uploaded only on pose changes. Other
inputs are never reuploaded. Kernel coherency, permissions, fences and retirement
remain authoritative. The collector saves raw requests/responses, inputs,
COLOR/Z and guards, with LAB-only per-job INFO and integrity comparisons. Those
costs are outside the reusable C submission/upload primitives and are not
introduced into production FAST. Its timing fields are host wall time including
LAB cold execution/readbacks, not GPU-only measurements or sustained FPS.

On uncertainty it stops future jobs and suppresses explicit close/free. Only an
acknowledged session CLOSE permits reverse BO cleanup and memory-context close.
Success still says `C_SESSION_COLLECTED_ORACLE_PENDING` and
`hardware_driver_qualified=false`. Independent native COLOR/defined-Z24
comparison is required for physical qualification. Initial fixture/profile
parsing and local mapping checks cannot authorize unsupported GPU code.

The pre-pinned equality contract and native receipts are in
`/home/gama/sgx535-offline/private-c-pds-20261030/v1/EQUALITY_CONTRACT.json` and
`NATIVE_RECEIPT_REVIEW.json`. Physical execution of this C collector is pending.
This component does not enable general Gallium draw dispatch, remove the kernel
LAB job bound/cold initialization or complete GL/GLES, desktop 2D or installation.
