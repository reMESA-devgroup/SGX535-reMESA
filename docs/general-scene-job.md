# Assemble the general scene into an owned GEM job

`remesa_gem_scene_job_prepare()` connects the existing GEM job, DPM provenance,
action/control/USE preparers, selected CPU views and private-MMU loan. It uses
the actual retained allocation list up to the existing32-resource bound. It
neither packs seventeen objects into eleven roles nor changes an encoder.

The trusted caller retains the existing outer DRM entry and externally
serialized private backing-session ownership for the endpoint across all calls;
this constructor does not acquire a file/context lifetime from a raw pointer.
It allocates and zeroes one stable `remesa_gem_scene_job` before taking
endpoint/MMU locks. It contains the real ticket, views, plans, loan and scratch
for the existing scene checkers. This context must not be stack-local and must
survive any retained failure. Section offsets are kernel-internal metadata,
not a new ioctl ABI or user-supplied register list.

Preparation copies immutable commands through `remesa_gem_job_prepare`, reads
the authoritative copied action section to select the existing two DPM claims,
and calls the existing action/control/USE preparation routines. It snapshots
all held backing but obtains CPU views only for READ inputs. DPM, REGION,
COLOR and Z remain held without exported writable-target CPU aliases. The
existing private preparer then constructs an unbound tree and claims the same
producer guard. No additional buffer clear, scan or hash is added.

An ordinary preparation error cancels the job and its holds through the
existing cleanup functions. A retained partial loan or refused cancellation
leaves state HELD, preserving the stable context and original error. Explicit
cancel releases only a safely unbound/unissued loan and job; it cannot invent
GPU retirement, root restoration or source closure. Cancelling a job preserves
the still-open memory endpoint's exclusion/power ownership until endpoint close.

This is real kernel preparation plumbing for the general service, not a new
ISP/DATA/PDS/USSE implementation. It does not bind the private root, acquire a
source lease, publish, issue, wait, read pixels or acknowledge execution. The
existing bound-root closure check and input authentication remain mandatory
before dispatch. Vertex-section consumption remains with that existing checker.
The canonical eleven-role executor is unchanged and still refuses this general
job. The plan-consuming service is now connected as described below. Its native
backend/source adapter and capture submission entry remain integration work;
see [general-executor-integration.md](general-executor-integration.md).

Evidence root: `/home/gama/sgx535-offline/general-scene-job-20261101/v1`.
The scene model retains all existing17-resource/four-dimension/body/reference/
publication cases. A new integration case exercises successful prepare/cancel
and reuse, exact29/31/20 action counts and ten USE entries, READ views with
writable targets excluded, malformed control/USE offset rollback, refusal to
prepare over a live context and no additional page clearing. Native, UBSan and
static i486 runs are CPU evidence. The first test incorrectly expected endpoint
exclusion to be released by job cancellation; its failing log is preserved.
Cancellation also refuses any context marked as execution attempted/started/retired,
before releasing a loan; post-execution cleanup needs its retirement path.
The corrected assertion requires the existing exclusion to remain held. No
production guard was relaxed.

The full candidate module is built separately and remains uninstalled. No new
GPU operation, rendering/API qualification or FPS claim follows from preparing
an unbound job. The fresh stock observation and next bounded physical protocol
are recorded in the integration audit; no old one-shot test was rearmed here.

## Retained plans connected to the service

`remesa_gem_scene_job_bind_service_locked()` runs the existing bound-root scene
closure checker against the retained job, READ views and scratch. Under the
existing endpoint-then-MMU locking and external lifetime contract, it copies
the validated 29/31/20 actions, TA/raster controls and USE bank plan into the
stable context. It binds `remesa_scene_service` to that context's session.
Neither a synthetic eleven-role descriptor array nor a legacy XHW wire is
constructed. Cancellation additionally refuses a started service.

`stage_scene_service.py` derives the general service from the unchanged,
SHA-pinned qualified service. It preserves the phase, ordered stages,
status interpretation, observer, failure, timeout and retirement implementation.
Only binding and the plan construction sites change. TA_INFO now supplies the
validated load action array and SCENE_INFO the retained plan; these internal
CPU metadata callbacks are not hardware replies. Fire callbacks consume the
validated action arrays without a fabricated wire. USE callbacks receive the
actual bank mask and words. This new callback contract requires its own native
adapter; the old fixed backend must not be passed these payloads.

Evidence: `/home/gama/sgx535-offline/general-scene-service-20261101/v1`.
The real generated service and original phase helpers execute 208 protocol
cases per native, UBSan and static i486 run, covering four target dimensions,
reuse, every one of the 19 callback failures, source error, unowned status,
MMU fault, timeout, reordered completion and duplicate TA. Callback hardware
is modeled; these tests perform zero GPU operations. The harness explicitly
counts the 208 executions: an earlier run built the scenario routine but did
not call it, and is retained as incomplete coverage rather than acceptance.
The 25-case LAB controller regression and five existing profile execution
transformations also pass. The full candidate builds but remains uninstalled.

Service phase RETIRED alone never authorizes releasing a real GEM loan.
Source closure, actual hardware completion, CPU/GPU visibility, root restore,
settlement and acknowledgement remain mandatory in the native executor. The
next component is the plan-consuming MMIO/source adapter followed by the
ordered executor and bounded physical qualification. ISP, DATA, PDS and USSE
encoders already covered by their existing oracles are not reopened here.
