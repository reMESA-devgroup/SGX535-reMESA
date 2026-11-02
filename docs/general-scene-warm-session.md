# General scene jobs with retirement-bound DPM reuse

The trusted kernel session can now prepare multiple general scene jobs against
the same protected DPM pools. This reuses the existing DPM load constructor,
general closure/executor and source/backing lease. It adds no ioctl, register
sequence, shader, diagnostic hash or readback to production rendering.

`remesa_private_session_scene_prepare` validates the endpoint, owner, live
unpoisoned source lease, exact sealed pool IDs and absence of another live job.
It calls the existing packet/section preparers. The ordinary
`remesa_gem_scene_packet_prepare` and `remesa_gem_scene_job_prepare` retain
their cold behavior; they do not implicitly acquire warm authority.

## Separate initialization and retirement facts

A new job first tries the unchanged `remesa_gem_job_claim_zero_dpm`. If the
trusted session job finds consumed zero provenance, a separate warm claim
requires the same pool IDs/generations, no conflicting jobs or aliases, a live
source owner and a previously acknowledged operation from that session.
Both pools remain initialized and pinned, with the same original size/address
constraints and complete job holds. It records the prior operation on this
job without setting either pool's `zero_pristine` flag.

The action preparer and execution closure check that prior operation again.
The29 DPM load actions and their four-completion/INITEND protocol remain
unchanged. The warm claim covers the two pools only: REGION must still have
real zero contents and no CPU alias. For the session path, already-used REGION
pages are overwritten under exclusive job holds before preparing its actions.
Only this actual full-page overwrite restores REGION's zero provenance.
It is followed by the existing publication flush/barrier. The allocation is
cleared as an initialization policy; its reserved size is not declared to be
the hardware's consumed extent. COLOR and Z retain their existing GPU clear.

Publication invalidates readiness for subsequent jobs before any possible
GPU write to a sealed pool. The executing warm job keeps its immutable prior
operation and is checked in the ISSUED state while that next-job readiness is
false. Only the source-session wrapper's successful return through the full
general executor, authenticated retirement, root restoration and source
settlement records a new acknowledged operation and makes readiness true.
Cache residency and GPU-dirty flags alone never provide this credential.

The previous job, loan and reservation must be released and retired before
the next session preparation. Canceling an unissued job can leave the prior
acknowledgement available, but cannot create a new one. Duplicate retirement,
stale epochs, mixed pool identities, changed source owner, missing source,
file/device loss and source poisoning are refused. Failed preparation that
retains a HELD job poisons the source. Every entered executor failure revokes
readiness and retains the existing source/job/root quarantine policy.

Read-only CPU leases use the already integrated
[cache ownership transitions](general-dpm-cache-ownership.md). They do not
change the last retired operation; the next valid GPU consumer flushes the
CPU-touched pool. Writable leases remain denied. Final source close/backing
release retains its full idle validation and CPU-coherence transfer and clears
the session's epoch metadata.

## Offline verification

Evidence root: `/home/gama/sgx535-offline/general-scene-warm-20261102/v1`.

- `warm-final/RESULT.json`: current actual memory/source-session/packet
  preparation, claims, closure, service, executor and retirement operate on
  the previously exported real OpenGL/native inputs at 1280×800 and 640×640.
  Native, UBSan and static i486/QEMU pass five jobs per case: one cold, three
  untouched warm jobs and one warm job after an actual modeled CPU read lease.
  An extra unissued preparation is canceled and prepared again. All pool
  page clears after allocation remain zero. REGION uses30 pages at 1280×800
  and its layout-derived allocation at640×640, overwritten only when needed.
- Cold execution has21 flush calls, untouched warm execution19, and warm
  execution after reading one pool20. These are modeled operation counts,
  not milliseconds or FPS. The existing validation and event stages remain.
- Epoch/source/identity/state mutations are refused without extra backend,
  cache or clear work. Another job cannot prepare while the prior acknowledged
  job still retains resources. Duplicate retirement cannot advance the epoch.
- `warm-failures/RESULT.json`:54 cases cover nine failure stages on a warm
  job after an initial successful cold job, both sizes and all three variants.
  Readiness is revoked, source is poisoned and jobs/roots remain retained.
  Retry, next preparation, readback lease, source release and cleanup refuse.
- Existing seal, cache and source/backing/adapter regression results are
  preserved separately. No old LAB controller or catalogue is rearmed.
- `incremental-final/RESULT.json`: the complete antiX5.10.240 i486 module
  builds with no new warning messages and exports the new preparation/epoch
  methods. Final candidate `module/gma500_gfx.ko` SHA256
  `a01343c03ccec8eb8341ca594dc1c5a0939ee4fa86e56c53195d798ea6023282`.
  After the first full build ended, the final comment/header contracts were
  synchronized and their dependents rebuilt. This module is not installed.

The initial failure-suite generator read the positive suite's command journal
while it was still being written, so its in-memory snapshot lacked the final
i486 build entry. Native/UBSan completed; the generator then stopped with
`StopIteration`. `RESUME_FAILURE_I486.py` reads the now-terminal journal and
runs only the missing i486 cases, retaining the already completed cases.
No hardware command was involved or repeated.

All hardware/source/cache/MMU/GEM primitives in these tests are injected models.
The old physical GLXGears and finite native OpenGL proofs remain unchanged;
these tests supply no new physical GPU completion, pixel correctness or FPS.

## Userspace and physical integration

The separately [bounded general-session transport](general-session-transport.md)
now connects these methods to the real native OpenGL client. The old eleven-ID
session ioctl and one-shot controllers keep their contracts. The new collector
captures cold and first warm jobs separately, with complete identity,
completion, retirement, readback, input and guard checks.

The new sequence has passed physically at 1280×800 and 640×640 in separate
controlled boots, each followed by verified stock recovery. The [client evidence](opengl-general-session-client.md)
distinguishes offline models from new GPU execution. The historical FAST waited
until after three cold jobs before skipping pool clears; its approval was not
transferred to this earlier warm transition.

Do not clear consumed gate/source lifetime flags, restore pool zero provenance,
claim an arbitrary DPM capacity, or count the model as hardware rendering.
General GL2.1/GLES2, complete desktop2D and installation remain unfinished.
