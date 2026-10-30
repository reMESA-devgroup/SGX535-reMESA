# Bounded userspace qualification of a private GPU session

The private build now registers root-only ioctl 10 for one bounded retained-profile
session per boot: OPEN, numbered SUBMITs and CLOSE. It uses the existing memory
and private-probe admission controls, both default N. The one-shot probe remains
separate; a successful source claim by the session consumes that probe's old
no-rearm gate too. This is not a production render ABI or general GL driver.

The 136-byte envelope accepts a server memory context and eleven owned BO IDs on
OPEN, with max_jobs1..64. SUBMIT accepts only the returned session discriminator,
next sequence and budget1..10000ms. CLOSE accepts the last completed sequence;
it remains available when submission controls have been disabled. No user VA,
PFN, raw packet, pointer, callback or fabricated receipt/event is accepted.

OPEN verifies actual SGX core/revision, exact qualified profile bindings,
initial canonical CODE/PDS/geometry/sidecars and real grammar under prepared-job
holds. It seals the five inputs, cancels that admission-only job and acquires
one source lease. Initial content authentication runs once and uses the existing
canonical comparison, typed masker and geometry validator. It does not hash
inputs per frame. OPEN is not a GPU execution receipt.

SUBMIT prepares a fresh job/snapshot/borrower/loan/observation, checks sealed input
identity, performs the retained cold LAB initialization and narrows input GPU
permissions to READ or CPU_ONLY. The unchanged private executor performs issue,
retirement, CPU publication, default-root restoration and source settlement.
The controller releases borrower/loan/reservation and retires that job before
returning phase9/ledger7/stages15 and its operation/cookie. The source lease and
BOs survive for the next SUBMIT. CLOSE requires settled idle source and releases
source/backing before returning state CLOSED (3), stages32. OPEN/CLOSE responses must never
be counted as GPU submissions.

Submission failures retain stable controller/owner storage, poison the session
and quarantine its job. No retry, reset, fabricated retirement or uncertain free
is provided. File loss retains the backing dependency graph through the existing
orphan policy. This bounded LAB endpoint is deliberately not a production fault
recovery or multi-client device broker.

## Offline evidence

Root: `/home/gama/sgx535-offline/private-session-lab-20261030/v1/`.

- `controller-ubsan/` and `controller-i486/`: actual controller/envelope and initial
  canonical comparison, 24 scenarios. Source, leases, GEM, masking/geometry and
  execution/receipt dependencies are modeled. Two model jobs share one claim;
  duplicate/excess sequence, denied admission, authentication failure, execution
  and cleanup failure, and disabled-controls CLOSE are covered. GPU0.
- `original-probe-regression/`: all 37 original controller scenarios still pass.
  Test-fixture additions are conditional for the new controller.
- `contract/`: actual initial authentication plus actual typed masking and
  geometry validation accept the exact archived native-pose assets. Corrupted
  CODE/PDS metadata are rejected. C/Python ioctl and all 136 request bytes match
  natively, with UBSan and as static emulated i486 code.
- `client-tests.*`: six methods with transport faults, receipt immutability,
  stop-on-integrity failure, unchanged-buffer reuse, dirty PDS page upload and
  safe cleanup. Modeled pixels/receipts are not physical rendering evidence.
- `full-module/`, `MODULE_REVIEW.json`: complete 5.10 build and read-only checks.
  These do not qualify the new endpoint physically.

The initial client test launch used the wrong relative directory; its harmless
import failure is preserved as `client-tests-launch-error.*`. Tests were then
created at the intended path and run successfully, without touching hardware.

## Collector

Offline planning opens no device and is the default:

```sh
python3 tools/remesa/private_session_lab.py \
  --assets ASSET_DIRECTORY --output NEW_DIRECTORY --frames 2
```

Only a separately qualified controlled physical run adds `--execute --device
/dev/dri/card0`. The collector creates/pins/maps the same BOs once, validates OPEN,
every numbered receipt and CLOSE, saves raw requests/responses and per-job
COLOR/Z, input and guard readbacks. An optional `--next-assets` alternates a
second data-only PDS pose with unchanged commands/bindings/other input images.
Only changed PDS pages are sent; no new upload or whole-PDS sync is added for an
unchanged pose. CPU writes are published by the qualified cold executor.

Uncertain replies, mapping or integrity failure stop further jobs and suppress
explicit CLOSE/unmap/free. The collector closes its OS FD, delegating retention
to the kernel, and records failure. Complete receipts/readbacks still leave the
pixel oracle pending and `hardware_driver_qualified=false`. LAB cold clears,
hashes/readbacks and artifact files are not added to production FAST. The tool
never installs a module, toggles controls, reboots or presents software pixels.

Physical multi-job approval, sustained FPS, general GL/GLES, desktop 2D,
installation, 1600x768 and other kernel backends remain unfinished. Qualification
must retain the real native COLOR/defined-Z24 baseline and recoverable stock boot.
