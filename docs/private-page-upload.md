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
`NATIVE_RECEIPT_REVIEW.json`.
This component does not enable general Gallium draw dispatch, remove the kernel
LAB job bound/cold initialization or complete GL/GLES, desktop 2D or installation.

## Physical C qualification (2026-10-30)

Userspace source `5e85c74` ran on the Mini 12 in boot
`30d1dc2f-5b03-484f-9621-3aaa23c5a548`, with the unchanged kernel module from
`71e2dda` (SHA-256
`9c7a3a683af43cd2936086f0d1eb3152f6b132dea0a7f9bc6d590ab68d3be85a`).
The static i486 executable and all payload files were hash-pinned before use.
One OPEN, three numbered SUBMITs and one CLOSE completed successfully.

| Job | Native pose | Operation/cookie | SUBMIT wall ms | PDS upload ms | LAB readback ms |
| --- | --- | --- | --- | --- | --- |
| 1 | 1 | 2/2 | 292.560 | No update | 179.574 |
| 2 | 2 | 3/3 | 365.655 | 1.542 | 188.357 |
| 3 | 1 | 4/4 | 322.528 | 1.343 | 259.109 |

Each SUBMIT reported actual core `01130000`, revision `00010201`, phase9,
ledger7, stages15, ACTIVE and error0. Admission-only operation1 was canceled;
OPEN/CLOSE are not GPU submissions. The same eleven BOs and source lease stayed
alive across all jobs. Only two PDS pages (8 KiB) were uploaded per pose change;
the other four inputs were not reuploaded. Preparation/upload took 346.652 ms
and OPEN 17.543 ms. These wall times are not GPU-only timings or sustained FPS;
the cold LAB initialization and diagnostic capture remain in this test.

`VERIFY_PHYSICAL.py` independently checks raw envelopes, distinct operation IDs,
all thirteen mappings, idle provider counts, all five input images and both A5
guards. It compares each output against its pre-pinned native pose: every one
of 4,096,000 COLOR bytes and 1,024,000 defined Z24 values matches exactly. The
two native COLOR poses differ at 179,447 pixels; the third job returns exactly
to the first pose. Existing undefined X8 differs at 200,000 pixels in each
raw depth plane and remains excluded as before. There is no new tolerance or
edge mask. Native historical conformance limits are not promoted to general GL
approval. Final status:
`PHYSICAL_C_SESSION_TYPED_PDS_THREE_JOB_NATIVE_REGRESSION_PASS`.

CLOSE returned state3/stages32, all objects and the memory context were released,
and an independent final observation found no matching collector process,
module live, all six controls N, original GRUB custom file and stock saved
default with empty `next_entry`. The exact-case kernel fault marker scan was
empty, and all 31 protected files remain unchanged. This boot's source/session
allowance is consumed: no source rearm, repeated probe, forced unload, reset or
suspend was attempted.

The first 25.6 MB streaming retrieval timed out; GPU jobs were not repeated.
Process observation found no remaining earlier Python retrieval. An overly broad
archive-preparation process filter initially matched its own parent `sudo` and
refused before creating an archive; the preserved observation identified that
parent, and the filter was narrowed to actual Python processes. One immutable
archive was then downloaded in thirteen bounded chunks. Its full SHA-256
`bb677f40f97c9868b91e4d42b1e0a5211cede6436b41ffee29159552647a930c`
matches the Mini 12 archive (25,610,240 bytes); raw remote evidence is retained.

Evidence root: `/home/gama/sgx535-offline/private-c-pds-20261030/v1/`, particularly
`PHYSICAL_COMPARISON.json`, `physical/capture/`, `FINAL_REMOTE_STATE.json`,
`SOURCE_BINDING.json`, `EQUALITY_CONTRACT.json`, `DOWNLOAD_RESULT.json`,
`CURRENT_STATE.json` and `EVIDENCE_HASHES.json`. LAB evidence costs stay separate
from the reusable upload/submission API and from production FAST.
