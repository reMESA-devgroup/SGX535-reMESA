# One source lease across distinct private jobs

The private build now connects the backing lease to the existing monotonic
source protocol. Kernel-only `remesa_private_session_source_begin()` claims one
source lifetime for the same stable backing owner token. The session keeps this
lease across successful jobs. It never clears `ever_claimed` or reopens the
one-shot LAB entry.

`remesa_private_session_execute()` serializes endpoint/file loss, checks the
exact backing owner, a live source lease and a surviving file/device, then calls
the unchanged `remesa_gem_private_execute()`. The caller must retain its outer
DRM entry, authenticate the complete final input/command closure and prepare the
borrowed scene and private loan. Each job needs a fresh ticket and observation.
The cold executor preserves its existing resource/permission validation,
publication, bound source attribution, completion/retirement, default-root
restoration, observation close and settled boundary. It unbinds that observation
without releasing the session's source lease. The caller then releases the loan
and retires the job before preparing another.

This is a kernel-internal integration boundary, not a new user ioctl or a
shader-admission implementation. It retains cold uploads/cache maintenance; it
is not yet a cache-epoch or production FAST implementation. No per-frame hashes,
readback, oracle comparison or evidence dumps are introduced here.

An executor error poisons the source/session and quarantines the job. No second
execution or source/backing release is allowed after that error, including a
failure before the cold executor's possible-issue transition. The original error
is returned; a failed quarantine cannot make the source/backing lease releasable.
The caller must retain arguments, backend/loan/observation and backing storage.

`remesa_private_session_source_end()` refuses live jobs, executors, leases,
holds, quarantine and aliases. It requires the existing hardware settled check
before releasing the source. Either failure leaves the session poisoned and
retained. Backing release now also refuses held or poisoned source state. File
or device loss is denied before source MMIO or a cold executor call; retained
orphan metadata never grants hardware access.

## Evidence and remaining qualification

Root: `/home/gama/sgx535-offline/private-source-session-20261030/v1/`.

- `tests/` executes the actual session/backing/admission/close/loss fragments,
  natively, with UBSan and as a static i486 executable under emulation. Source
  callbacks, the cold executor, jobs, leases and kernel primitives are modeled.
  The successful case has three modeled jobs and one source claim, one final
  settled/release and no rearm after close. Failures cover pre-issue/execution,
  settled and source-release errors; device loss prevents another executor call.
  The 652 checks include the backing suite, not 652 physical rendering trials.
- `SOURCE_REVIEW.json` and `NORMAL_PROFILES.json`: cold executor, original source,
  one-shot probe, scene/profile, backend and loan sources unchanged; five normal
  profiles byte-identical; all 31 protected files preserved.
- `full-module/`: complete 5.10 candidate build; separate read-only module review.
  No module installation, reboot or GPU operation is performed by these tools.

The full module build completed in 133.622190 s, SHA-256
`401d139324bd81f1476e6bb5e2a9c01477f6ddb14ae43cdd8ffdb8279b493653`.
`MODULE_REVIEW.json` passes the existing ELF/release/entry/MMIO/lifetime guards;
`SESSION_SYMBOL_REVIEW.json` confirms all five backing/source/session functions
are defined and that tested staged inputs match the built inputs.

No physical multi-job result or sustained FPS is claimed. The previous exact
[native/private single-operation regression](private-native-regression.md)
remains preserved and does not qualify this new session automatically. The next
runtime integration must expose a bounded, authenticated render submission path
and validate multiple real operations with fresh receipts, correct readback,
unchanged guards, idle closure and recovery. Variable-sized drawables, general
GL/GLES and desktop 2D still need their own integration and physical validation.

The subsequent [admission correction](private-session-admission.md) rejects
legacy retained/probe calls while this backing lease is held, before those
interfaces can initialize or mutate the endpoint. This is separate from the
still-pending authenticated userspace session interface.
