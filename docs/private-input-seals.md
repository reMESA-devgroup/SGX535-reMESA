# Private input sealing and identity reuse

The private backing session can now retain input authentication across distinct
jobs. Its kernel-only seal operation records the exact five input IDs, preserving
their generations, and makes the non-PDS inputs immutable to user writes. PDS
updates use a trusted pure callback; the existing canonical page guard permits
only its declared bounded matrix/light float fields. No new shader instructions,
CODE padding, canonical images or expected pixel results are introduced.

The caller must authenticate the complete initial images/geometry/commands under
the same prepared job holds before sealing. This API does not authenticate content
itself or accept a userspace function pointer. It requires the correct stable
backing owner, surviving file/device, an unissued unattached eleven-role job and
exclusive live/pinned/mapped input holds without leases, quarantine or prior
seals. The complete metadata set is checked before any seal is committed.

Seal before source ownership begins, cancel that admission-only prepared job,
then begin the source lease and prepare fresh execution jobs. The backing lease
freezes topology during that gap. Existing WRITE/LEASE logic denies write leases
to immutable inputs. PDS WRITE validates the complete supplied page before any
copy; a rejected page leaves the backing unchanged. Job holds continue to deny
all CPU leases while a job is live.

The session execution wrapper now checks sealed input identity under the memory
lock before the cold executor. A different ID, generation, seal, pin/map state or
live CPU lease cannot reuse this authentication. Only five metadata entries are
checked; no full-buffer hash, memcmp, readback or diagnostic dump is added per
job. The page comparison belongs to validation of an explicit PDS WRITE, not a
periodic whole-buffer comparison in the render loop.

Initial content authentication and GPU permission construction remain the trusted
entry's responsibility. Reuse is valid only while GPU permissions also preserve
input immutability (READ or CPU_ONLY for the qualified profile). This helper is
not permission authorization for a GPU-writable input or a shader verifier. The
public submission path must enforce its complete resource/command contract.

Backing release validates every recorded seal before clearing any. It requires
source closure, no jobs/CPU aliases/quarantine, and a surviving file/device. A
corrupt seal refuses the complete unseal transaction. File/device loss retains
both the backing and seals; it does not create completion or free uncertain work.
A separate private lease is refused if old seals or a PDS guard remain.

## Validation and remaining work

Evidence: `/home/gama/sgx535-offline/private-input-seals-20261030/v1/`.
`tests-final/` compiles the actual frozen memory-service source with the private
lease/source/seal fragments and unchanged canonical PDS guard. Tests use actual
request, prepared-job, hold, lease, transfer, cancellation, reuse and release
logic. GEM/MMU/PM/vmap are injected CPU APIs; initial image authentication is a
caller assumption over synthetic data. Source/executor callbacks are forbidden.
No synthetic image or state is presented as real GPU output.

Native, UBSan and static emulated i486 runs check transactional seal failure,
immutable CODE/geometry writes, valid PDS float updates, metadata and NaN
rejection without mutation, partial/misaligned write rejection, readonly input
reads, live-job CPU exclusion, different-input denial (including the actual
execution wrapper), new-ticket reuse, transactional unseal and file-loss
retention. `tests/` preserves the earlier successful run before the additional
wrapper-denial case. `session-regression/` reruns the source/backing suite in all
three variants; its unsealed fixtures do not claim input authentication.

`SOURCE_REVIEW.json` and `NORMAL_PROFILES.json` preserve the existing probe/UAPI,
legacy admission checks, cold executor, MMU loan, source protocol, backend,
profile, canonical guard, legacy FAST path, five normal profiles and 31 protected
files. `full-module/` records the complete candidate build. It is not installed
or physically qualified by these CPU tests.

The complete module built in 165.012948 s, SHA-256
`82314bc87443e2dae4e28bbc019270c6383412064043081840876881beb036aa`. The read-only verifier passes the existing
ELF/release/MMIO/lifetime guards. `SEAL_SYMBOL_REVIEW.json` confirms both new
functions and exact staged/built input identity. No module was installed.

The new helpers still have no userspace render-session entry. Next work is a
bounded authenticated submission interface with actual SGX535 rev121 identity,
fixed qualified access permissions, typed updates and fresh operation receipts.
Kernel-managed TA/output ownership must also be enforced before dropping cold
clears or cache maintenance. Then validate multiple physical jobs and correct
COLOR/Z/guards under one source lease, without rearming the LAB probe. General
GL/GLES, desktop 2D, installability, physical 1600x768, and performance remain
separate unfinished requirements.

The subsequent [private work-buffer ownership](private-work-ownership.md)
separately retains roles5..10 and denies their user writes/publication during a
session. It preserves the five-input typed PDS contract and existing cold
execution, and does not itself authorize clear/cache omission.
