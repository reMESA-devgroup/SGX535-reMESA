# Desktop, GLES2 and desktop GL2.1 targets

The user-defined next milestone is an accelerated whole desktop, OpenGL ES2.0
and desktop OpenGL2.1. Optimize the correct driver before the subsequent
Minecraft milestone. A specialized glxgears path, successful CPU encoders,
version strings or an isolated 2D operation do not satisfy these requirements.
Keep the complete installable userspace/kernel driver objective active.

## Evidence required

| Target | Required evidence | Current boundary |
| --- | --- | --- |
| Whole desktop acceleration | Active Xorg rendering through the hardware backend, correct windows/composition and presentation, sustained interaction, GPU submissions and retirement, recovery verified | Physical2D fill/copy proven; whole desktop not integrated |
| GLES2.0 | Correct hardware-backed EGL/GLES contexts and implemented API/shader/resource behavior, relevant conformance/regression coverage, SGX submissions/retirement and checked output | General scene/native admission and frontend/API implementation incomplete |
| Desktop GL2.1 | Correct hardware-backed desktop context and implemented API/shader/resource behavior, relevant compatibility/conformance/regression coverage, SGX submissions/retirement and checked output | Working specialized glxgears path is narrower than complete GL2.1 |
| Installable driver | Reproducible build/install/remove, supported kernel/distribution checks, normal loader integration and preserved stock rollback | Private archive/kernel qualification is not installation qualification |
| Performance | Physical per-phase measurements and sustained correct workloads, required synchronization retained, no test-only work in normal rendering | Optimize after correctness; no FPS claim from encoder/model tests |
| Minecraft | Real application rendering through the qualified hardware driver with correctness/stability and performance evidence | Later milestone, not a substitute for complete APIs |

Continue checking1280x800 and640x640 together,641x479 occasionally, and preserve
the1600x768 VaioP compatibility target. Host storage/viewport-size acceptance
alone is not physical validation at those dimensions. Cross-distribution and
kernel support and eventual video acceleration remain in scope; no new version
number is advertised merely to satisfy a launcher.

## Current observed stock desktop

Read-only Mini12 inventory in
`/home/gama/sgx535-offline/desktop-api-targets-20261031/v1/MINI12_READONLY.json`
observed boot `f507f3b8-20e9-4ed8-8a80-73e417ab5cbc`, kernel
`5.10.240-antix.1-486-smp`, IceWM/Xorg, Xorg core21.1.16 and installed Mesa25.0.7.
`glxinfo -B` reported llvmpipe and `Accelerated: no`. Its higher software API
versions are not SGX support. This is the recovered stock desktop, not evidence
that a new reMESA driver was installed or failed a rendering test.

`XORG_ACCEL_READONLY.json` separately preserves the relevant Xorg log lines:
modesetting uses `/dev/dri/card0`, refuses glamor on llvmpipe, reports glamor
initialization failure and initializes the DRISWRAST GL provider. This identifies
the required normal X11 integration boundary. Loading an extension named DRI2
or DRI3 does not itself prove hardware acceleration. No Xorg configuration,
package, boot entry or module was changed by these queries.

The source backend also deliberately retains narrow capabilities:
`mesa/sgx535/remesa_screen.c:set_capabilities()` disables graphics on
resource-only endpoints and retains restricted shader/varying/primitive limits.
Do not raise those advertised limits before their implementation is correct.
The offline Mesa25.2.0 archive tests are separate from installed Mesa25.0.7.

## Immediate implementation dependency

The preserved fixed event DATA64/CODE84 now has an owned immutable upload and
is retained by scene footprint/capture (`9a8a43e`, `e991e31`). This removes the
need to rediscover that fixed program. Owned raster/TA/region control snapshots now bind that frozen graph; see
[scene-controls.md](scene-controls.md). Explicit native COLOR initialization before capture is also connected; see
[linked-color-initialize.md](linked-color-initialize.md). Complete the DPM
initialization contract and authenticated native lifecycle/control consumption next,
then connect the normal frontend/loader and X11 resource/presentation path.
The event's general ISA semantics remain distinct from reuse of its already
preserved fixed CPU construction. Keep LAB diagnostics separate from production
rendering and retain every required address/lifetime/coherency/fence/retirement
and failure check throughout that integration.

The retained job/private-MMU capacity now covers the current17-allocation
scene with a shared32-resource software bound; see
[native-job-capacity.md](native-job-capacity.md). Offline models and a complete
module build passed. This capacity change is not native scene execution or
physical deployment qualification.

Cold DPM initialization now has a kernel zero-origin claim and selected views
that avoid mapping the two32MiB pools; see [cold-dpm-origin.md](cold-dpm-origin.md).
This preserves required publication and does not establish native scene execution
or physical qualification. The trusted general-scene consumer remains next.

Kernel-bound DPM/TA/raster action construction now derives the original groups
from actual retained IDs, and Mesa can retain undefined REGION storage without
the legacy CPU initialization; see [native-scene-actions.md](native-scene-actions.md).
Complete image/control validation and ordered native execution are still
required before this general scene path can be physically qualified.

Raster/TA controls can now also be reconstructed from held kernel IDs and
checked against the matching action plan; see
[native-scene-controls.md](native-scene-controls.md). Mesa and kernel share
the original word serialization. This does not yet establish complete
image/program admission, source-attributed execution or GPU qualification.

Frozen captures now expose the complete owned USE entry windows and bank
settings, with corresponding held-ID/window validation in the kernel; see
[native-scene-use.md](native-scene-use.md). This is numeric ownership metadata,
not validation of instruction contents or permission to execute the scene.
