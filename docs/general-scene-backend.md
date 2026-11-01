# Native backend for the retained general scene

The general service now has a native consumer, `remesa_scene_backend_begin()`
and `remesa_scene_backend_ops`. It uses the real retained job/session and the
existing fixed IO/actions, shared IRQ owner, source boundary and capsule ledger.
It does not manufacture a legacy owner, eleven descriptors or an XHW wire.
The private build opts into this integration with `REMESA_GENERAL_SCENE`.
There is still no general capture dispatch or new live ioctl.

The caller must retain the outer DRM entry, backing-session/device/power/source
lease and the stable job, backend and observation. It holds the endpoint mutex
then MMU writer continuously, has bound the private root, has authenticated the
immutable input construction and has called the existing general closure and
service binder. Source bind must already have consumed the attempt. Begin
requires `execution_attempted` and refuses publication, prior execution, wrong
session/plan, inactive power, mismatched guard, or failed live backing/PTE checks.
Every failure after source bind must quarantine the complete executor context;
a failed begin is not permission to free, retry or infer a root restore.

Begin reuses the two read-only source boundary/IRQ handoff intervals and existing
producer reservation checks. It neither wakes nor drains nor resets the device.
The shared IRQ capture and sampler select the actual general device/session.
The actual source capsule admits the job as owner and the general service as
service identity. New typed observers validate that same pair, with no legacy
fallback. The nohash close checks that owner, terminal phase, ledger, pending and
processing state. The legacy COLOR-prefix evidence entry rejects a general job.

The native stage consumer publishes through the existing GEM loan publisher,
validates the retained translations and uses the existing private maintenance.
It consumes the checked 29/31/20 action plans and original schedule constructors.
DPM load keeps the shared-lock `prefire_load_active` interval and requires the
existing strict load poll before reporting observations. USE programming uses
the retained bank mask/words and relinquishes its reservation only after scene
completion. Possible issue and deadline are recorded before TA actions; an
ambiguous ISP reset assertion remains held until a confirmed clear. MMIO writes
use the existing source-lock authority/write interval, including the general
job whose legacy owner pointer is NULL. No pixel scan, hash or diagnostic
readback is added.

The general job pointer is appended after the retained private backend fields.
`remesa_scene_backend_abi.c` checks every preceding field offset against the
pre-extension private structure, including all original observer fields, and
requires the extension to follow that whole structure. The first uninstalled
candidate placed the pointer after `owner`; review caught the observer ABI risk
and that artifact is superseded by the final build with these assertions.

## Offline evidence

Evidence root: `/home/gama/sgx535-offline/general-scene-backend-20261101/v1`.

- The existing native/UBSan/static i486 scene tests execute 208 service cases
  and 144 native-consumer cases each. The latter use plans from the actual
  prepared 17-resource fixture at all four dimensions and run the actual native
  consumer against original action/control oracles. IO, GEM publication and
  source hooks are modeled: this does not attest real hardware execution.
  Cases include ordered completion, stage IO failure, reset ambiguity, load
  interval, possible issue/deadline, failed backing validation, source boundary
  and IRQ handoff. Explicit counters are required by the test main.
- The source regression runs actual generated routing plus unchanged capsule
  and source-guard implementations, with reduced job/device/lock models. It
  exercises general nohash closure and refusal of wrong service/session/owner,
  sequence/task, ledger/phase, pending/late events, producers, overflow and
  legacy COLOR-prefix capture. It also runs the existing legacy routes in the
  new compiled profile. No MMIO is executed in this source test.
- The pthread MMIO race test reproduces the old check/write race and checks
  the protected writer for legacy and general device identity, including a
  NULL legacy owner, invalidation, power/mapping/deadline/task/bounds refusal.
- The LAB controller remains a separate 25-case UBSan regression. The full
  i486 module is built but uninstalled. Stack figures are compiler frame
  sizes with executable-section comparison, not a complete caller/IRQ proof.

The first harness failures are preserved: missing source-facts include and
an unused parameter in the modeled MMIO macro; a later source test incorrectly
used the legacy COLOR close before the late-pending check. The corrected test
uses nohash closure. No production safety check was relaxed for these failures.

## Next execution component

The ordered general executor must connect this backend to the real GEM job:
authenticate selected snapshots/default mappings before the writer, bind
source/root, bind the validated service, enter the native backend, record the
actual issued job/cookie before service publication, run real status callbacks,
require owned completion and retirement, transfer CPU visibility, close the
source, end the backend, restore the root, unbind and settle the source, then
acknowledge the actual loan/job. Any uncertainty retains everything. Capture
serialization/submission must call that executor with the existing images.

A new pinned recoverable physical qualification is required before general
rendering or OpenGL/GLES claims. Existing ISP/DATA/PDS/USSE encoders and their
physical/oracle evidence remain in use. Normal API dispatch, full OpenGL 2.1
and GLES 2 behavior, whole-desktop integration and installation are incomplete.
