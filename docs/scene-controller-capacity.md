# Complete binding capacity in the closed LAB controller

The internal closed scene controller now handles an authenticated selection
of either 17 or 18 bindings. The existing public ioctl remains restricted to
its original 17-binding catalogue and wire layout. This is a shared controller
implementation for the next separate transport, not registration of a new
hardware operation or arbitrary-packet interface.

The selected packet, binding identities and capacity come from an internal
descriptor. The original wrapper constructs that descriptor solely from the
unchanged ten-profile static catalogue. The common controller checks the
request envelope, all IDs (including the last one), exact backing addresses
and sizes, generations, leases, pinning, quarantine, CPU aliases and cold
writable state. It requires the prepared job count to match the selected
capacity before hashing or execution. Every READ image remains subject to the
existing LAB image identity gate. This diagnostic cost remains outside FAST.

Metadata ID translation now uses the selected count. It continues to change
only captured metadata IDs; no caller supplies a packet, VA or CODE. The
existing source/backing leases, producer exclusion, identity checks,
completion/retirement, cancellation, uncertainty retention and single consumed
attempt remain shared by both capacities. The root and default-off gates
remain mandatory.

The internal trace retains the extra binding separately from the old fixed
trace record. A legacy trace query returns `EOVERFLOW` for an 18-binding
capture and leaves its output unchanged. It cannot silently omit the last
owner. The future trace ABI must expose that additional record explicitly.
No new trace ioctl is registered in this change.

## Verification

Artifacts are under
`/home/gama/sgx535-offline/scene-controller-capacity-20261101/v1/`.

The actual controller passes all 58 existing cases and 44 extended cases in
native, UBSan and i486 builds. Extended descriptors come from the actual
18-binding bound Gallium captures at 1280x800 and 640x640. Controller
dependencies, including crypto and hardware execution, are modeled; the full
retained kernel closure was separately qualified offline by the bound scene
integration. These tests do not prove a physical draw.

The extended tests attack the last backing's VA, size, generation, lease,
pinning, reservation, quarantine and mappings; corrupt the last READ identity;
refuse zero/duplicate last IDs and invalid counts; and reject a prepared job
with the wrong count before hashing or execution. They exercise cleanup and
uncertain retention, verify the full internal trace and legacy trace refusal,
and prevent another attempt through the old entry after consumption.

The existing UAPI header and entire ten-profile generated catalogue are
byte-identical to the previous staged module. The full gma500 module builds
without new warnings and is not installed. Protected physical evidence is
unchanged. Production rendering, caps and the working GLXGEARS path are
untouched.

The [separate wire format and userspace transport](gears-scene-wire.md) now
define and validate the 18-binding request and complete trace. Kernel
registration and versioned publication remain pending. Next connect its exact
catalogue and client, qualify the complete path,
and prepare captures from the preserved positive physical geometry and
coefficient witness with independently pinned pixel/control/integrity oracles.
Only then perform fresh recoverable Mini 12 experiments. Generic test
coefficients are not rendering expectations or hardware approval.
