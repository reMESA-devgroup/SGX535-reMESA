# Reusing a frozen scene through the provider

`remesa_persistent_scene_capture_submit_keep` submits one frozen scene through
exactly the existing native callback, registry and fence lifecycle. On success
it retains the capture and its transitive resource owners. The caller can submit
the same capture again after the acknowledged operation. The original
`remesa_persistent_scene_capture_submit` still consumes the capture on success.

This closes a userspace lifetime gap for the general source/backing session:
a successful operation need not destroy generated resources that a longer-lived
native session still owns. It does not open or close that native session, select
a kernel profile, enable a gate or widen shader/API admission.

Every call obtains a distinct provider operation and checks the native receipt's
context, operation, retirement, publication and acknowledgement. It submits once;
there is no replay loop inside the callback. It performs no extra allocation,
upload, readback or buffer hash. The existing packet and registry validation
remain in place. Callers must externally serialize the complete lifecycle.

Published fences still occupy the bounded job slots until destroyed. Keeping
eight fences applies backpressure to a ninth submission; destroying an already
published fence allows slot reuse while the capture remains alive. A conclusive
NOT_ISSUED result leaves the capture intact and the output NULL. Any ambiguous
attempt or malformed receipt quarantines the whole endpoint, retains the
capture and failed fence, and refuses further submissions and resource release.
This also applies to a later submission after an earlier successful operation.

The caller must obtain acknowledged close of any native source/backing session
before explicitly releasing the capture. Capture release cannot infer kernel
session close. Destroying application owner references while the capture is
alive does not free its backing.

This API is a building block for the general driver, not continuous OpenGL
qualification. The current frontend still prepares its scene through the
existing path. Normal dispatch integration and changing inputs are separate work. The
[subsequent COLOR READ lease](retired-scene-color-read.md) provides explicit
retired output access while keeping the capture alive. The ordinary map path
continues to refuse CPU access to held scene resources.

## Verification

Evidence root: `/home/gama/sgx535-offline/reusable-scene-capture-20261102/v1`.

The new `test_scene_provider_keep.c` reuses the actual persistent provider,
registry, encoders and owner graph. Only native execution receipts and the
memory backend are modeled. Eleven cases cover 32 repeated submissions of
one capture after dropping application owner references, unchanged packet and
allocation counts, fence capacity/reuse, a conclusive later refusal, and eight
later ambiguous/malformed receipt modes. Every successful path ends with exact
owner cleanup. Failed paths retain their modeled resources until process exit.
The unchanged one-shot behavior is checked separately with its original
14 scenarios.

These tests produce no pixels and supply no hardware submission, FPS or
OpenGL conformance evidence. The new kernel READ-seal candidate still needs
physical qualification after the reported boot error is understood. The Mini 12
was returned to stock and its boot configuration recovered before this work;
no new hardware test or reboot is issued here.
