# Private backing lifetime between jobs

The private build now has a kernel-only backing lease for the existing memory
endpoint. It prevents CLOSE and topology mutation in the gap between render jobs,
where a prepared job alone was insufficient: native close can cancel prepared
jobs and free the endpoint. The lease retains the endpoint's existing GEM,
device, module and power ownership; it does not create another allocation pool.

`remesa_private_session_acquire()` binds a stable kernel owner token to the exact
DRM device, file and context. It refuses another lease, a legacy stream/executor,
retained-interface admission, a live job, CPU lease, outstanding hold, quarantine
or existing COLOR VMA. Acquisition/release serialize through `endpoint_lock`
then the memory lock. The caller also owns its outer DRM entry guard and must
serialize use of borrowed pointers; successful release ends their lifetime.

While held, the existing admission branch freezes CREATE, MAP, UNMAP, UNPIN and
DESTROY. CPU transfer/sync requests still require explicit leases and all their
normal bounds/permission checks; any live job excludes those requests as before.
The legacy fixed-size COLOR mmap is refused because it has no private-session
ownership protocol. The ordinary memory close refuses the lease before dropping
job holds or buffers. The legacy stream cannot claim the same endpoint.

Release requires the same token, a surviving owner file, no device loss, no
stream/executor, jobs, CPU leases, holds, quarantine or COLOR mappings. It neither
waits for nor manufactures GPU completion. The future render-session caller must
settle and release source/execution ownership before releasing backing. These
helpers do not claim a source lease, issue commands, validate shaders, construct
fences or replace the existing retirement checks. They are not wired to a new
ioctl or a production renderer yet.

An active backing lease also retains the device dependency graph and blocks
rebind. This includes the native postclose case that already cleared the file
pointer after close refused: the lease remains and release fails. File/device
loss does not free the buffers. Retention until reboot is deliberate; no physical
remove/unbind/S3 qualification is inferred.

## Validation

Evidence: `/home/gama/sgx535-offline/private-session-20261030/`.
`v2/tests-final/` contains actual helper, extracted close/admission and actual loss
fragment tests, run natively, with UBSan and as a static i486 binary under an
emulator. Mutexes, jobs, CPU leases and backing callbacks are modeled. The tests
exercise invalid ownership, busy acquisition/release, between-job close refusal,
three modeled job intervals, CPU alias denial, file loss, device loss and the
already-orphaned file case. They perform zero GPU operations.

`v2/file-lost/` reruns the existing orphan/close tests. `v1/controller-ubsan/`
preserves all 36 controller cases; the one-shot probe/execute sources are
unchanged. `v1/NORMAL_PROFILES.json` proves the five non-private patched source
profiles match the parent exactly. Protected files remain separately checked.
The full 5.10 module builds are under `v1/full-module/` and `v2/full-module/`;
v1 predates the final active-lease retention/rebind correction and is superseded.
Neither module is installed by these tools. The final module builds in
110.817216 s, with SHA-256
`757bdad110da93bb348fc4406cfbd5d9af0379b7cfc4bc28caa88400b807351e`.
`v2/MODULE_REVIEW.json` passes the read-only module verifier for kernel release,
serialized MMIO and the existing entry/file/device/GEM/fbdev/lid lifetime guards.
`v2/SOURCE_REVIEW.json` binds the runtime changes and records the sole subsequent
header difference: documentation of caller entry-guard/pointer-lifetime duties.

This is the backing-lifetime component of a reusable private renderer. Remaining
integration includes persistent source ownership with fresh observations and
operation receipts, authenticated input updates, safe cache ownership/reuse and
normal userspace submission. The one-shot LAB gate remains unchanged; clearing
its consumed flag or the source's ever-claimed flag is not a reusable renderer.
