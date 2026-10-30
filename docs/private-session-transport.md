# C transport for the bounded private session

The existing Linux memory provider now exposes explicit
`remesa_linux_memory_private_lab_open`, `_submit` and `_close` functions.
They borrow the provider's exact fd and server-issued memory context, with
external serialization. This connects native C callers to the physically tested
[bounded session contract](private-session-lab.md); it does not enable ordinary
Gallium dispatch, expand the accepted rendering profile or advertise OpenGL.
The kernel remains unchanged from the physically tested `71e2dda` build.

OPEN accepts eleven locally owned mapped IDs and a maximum of 1..64 jobs. It
rejects duplicate/unknown IDs, host leases and memory-only recorders, validates
provider backing once per object, and enters the root-only session ioctl once.
The kernel authenticates canonical inputs, exact bindings and permissions;
userspace metadata never grants execution authority. A successful OPEN carries
actual SGX535/rev121 identity, a session ID, state1/stages16 and no GPU operation.

Each SUBMIT creates a fresh zeroed 136-byte envelope and enters exactly one
synchronous ioctl. It does no INFO queries, allocations, readbacks, whole-buffer
comparisons, hashes, logging or diagnostic synchronization. It checks immutable
request fields, session/sequence, actual core/revision, phase9/ledger7/stages15,
state ACTIVE and a strictly increasing nonzero operation equal to the cookie.
The kernel supplies coherency, retirement, root restoration and job release.
Outstanding host leases and exhausted job limits are refused locally.

CLOSE uses the last successful sequence, including zero when no jobs ran, and
requires state3/stages32 with no fabricated GPU operation or receipt. Reopening
this provider after any entered OPEN is refused. An entered-ioctl error, altered
envelope, incomplete receipt, stale operation or error response poisons the
endpoint and every allocation slot. No retry, explicit CLOSE, allocation,
unmap/unpin/destroy or memory-context release is allowed through that object.
The raw response is returned to a separate collector even on uncertainty.
Host-only `forget` still does not claim retirement or release kernel backing.
Native copy/fill and retained submission cannot mix with an active private
session. No reset, source rearm or production diagnostic work is added.

## Verification and limits

Evidence: `/home/gama/sgx535-offline/private-session-c-transport-20261030/v1/`.
`tests-final/RESULT.json` covers native, UBSan and static emulated i486 builds.
Seventy-two fault scenarios exercise OPEN, SUBMIT and CLOSE with transport
errors, every immutable field, incomplete/failed responses and repeated operation
IDs. Success, limits, active CPU leases, empty-session close, recorder rejection,
cleanup and mixing exclusions are also checked. Both native 2D transports and
the existing retained transport pass their unchanged regression tests.

The C client also replays all four real OPEN/SUBMIT/SUBMIT/CLOSE request and
response envelopes from boot `d9f98d2f-c63a-4e9a-910a-a3785c7da95a` byte for byte.
Memory metadata is modeled in this replay; it is not another hardware run.
The actual two-job physical qualification and pixel comparisons remain recorded
separately in `private-session-lab.md`. This C implementation has not yet been
executed on the Mini 12. It must not inherit physical approval from replay.

`LOADER_REVIEW.json` confirms that the pinned Mesa overlay stages the exact new
UAPI header and that its copied C provider passes syntax compilation. No live
Mesa tree, installed library or boot module was modified. Tests can be rerun
without hardware using:

```sh
python3 tools/remesa/test_private_session_transport.py --output NEW_DIRECTORY
```

Add `--capture CAPTURE_DIRECTORY` only to replay preserved envelopes, and the
paired `--i486-cc`/`--qemu-i386` options for the static i486 variant. No live
node is opened by these tests. LAB pixel/guard readbacks belong to a separate
qualification collector; callers still need them to qualify a physical change.
The kernel cold LAB initialization is unchanged and is not a production FPS
optimization. General GL/GLES scene admission, a production device broker,
desktop 2D and installation remain separate unfinished integration work.
