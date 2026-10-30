# Private session ownership of GPU-written work buffers

The bounded private-session controller now seals roles5..10 under the same
PREPARED admission job used to authenticate and seal the five inputs. These are
COLOR, region/TA/DPM scratch and Z. The source lease is acquired only after both
ownership steps and admission-job cancellation. No new ioctl, parameter or UAPI
field is added, and the existing cold execution/clear/flush policy is unchanged.

`remesa_private_session_seal_work` requires the exact backing owner, live file
and device, authenticated input seals, no source/executor/legacy TA-cache owner,
and a held unissued/unattached eleven-role job. It checks all six live IDs and
generations, exclusive holds, pins, mappings, pages, input-seal absence and CPU
alias absence before committing any work-role metadata. IDs must be distinct
from all earlier roles. A failure commits none of the six ownership records.
This helper is trusted kernel metadata admission, not initial image
verification, initialization or proof of GPU completion.

The private memory service refuses user WRITE, write-LEASE and SYNC on those
six IDs throughout the backing session. Read leases and INFO retain the existing
in-flight-job and CPU-alias checks. Typed PDS updates remain supported; unrelated
owned BOs retain their ordinary CPU operations when the session is idle. The
legacy COLOR mmap path was already excluded during a private lease. Kernel
initialization and the trusted GPU job can still write the work buffers.

New prepared jobs validate the six work IDs/generations and backing ownership
alongside the five sealed inputs before the original cold executor is entered.
Source settlement, coherent CPU publication, default-root restoration, borrower/
loan/reservation release and job retirement remain unchanged. This metadata
check does not hash or compare complete buffers.

Backing release first verifies that the source is closed, all jobs/leases/
aliases are gone, and every work/input record is valid. Duplicate or invalid work
records refuse the entire release before any input seal is cleared. Only then
are all records removed together. File/device loss retains the whole graph;
there is no rearm, forced reset, guessed retirement or uncertain free.

## Evidence and limits

Root: `/home/gama/sgx535-offline/private-work-ownership-20261030/v1/`.

- `tests-final/`: actual memory request/job/lease/transfer and private helpers,
  native, UBSan and static emulated i486; 168 checks each. GEM/MMU/PM and initial
  authentication are modeled. Tests cover transactional admission, READ versus
  WRITE/SYNC, PDS updates, unrelated BO access, swapped work-ID refusal before
  any executor callback, failed release and retained file-loss ownership. GPU0.
- `controller-ubsan/` and `controller-i486/`: 25 actual controller scenarios with
  modeled dependencies, including work-seal failure before source acquisition.
- `original-probe-regression/`: all 37 original controller scenarios pass.
- `source-regression/`: backing/source orchestration passes all three CPU
  variants, preserving the monotonic source lease and uncertainty behavior.
- `NORMAL_PROFILES.json`: all five nonprivate source profiles are byte-identical
  to the parent. `SOURCE_REVIEW.json` shows that the controller's only execution
  path change is the pre-source work-seal call and its error branch.
- `full-module/`, `MODULE_REVIEW.json`: complete 5.10.240-antix.1-486-smp build
  and read-only existing MMIO/entry/file/device/GEM/fbdev/lid guard checks pass.
  Module SHA-256:
  `8ec0f16b3cac519f28618a2c9688bec1aeb488c4b5f5b67ff57c0bab37aa7119`.
  Build time was 123.144 s. The existing input kernel/symbol files are unchanged.

The first test invocation supplied a nonexistent shim path and failed before
compilation; `tests/native-build.log` is preserved. It was rerun using the
previously qualified shim. An initial normal-profile comparison also used the
invalid fill-without-copy configuration; the corrected comparison uses the
existing legal profiles. No runtime gate or test expectation was weakened.

The module is not installed or physically qualified. The last Mini 12 physical
result remains the three-job C/typed-PDS qualification documented in
[private-page-upload.md](private-page-upload.md). This change alone does not
justify omitting clears or cache maintenance, establish reusable DPM epochs,
complete the generated-scene event CODE, or enable general GL/GLES/desktop 2D.
It supplies necessary work-buffer ownership for the next integration steps.
