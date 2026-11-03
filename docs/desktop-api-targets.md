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

## Current integration boundary (reviewed 2026-11-03)

ISP, DATA, PDS and USSE already have implementations, original-constructor
oracles and documented physical results in the selected paths. They are reuse
targets, not a new reverse-engineering phase. See the current
[general executor integration audit](general-executor-integration.md) for the
source call graph and completed components. Its historical next-step wording
must be read alongside the newer joined-session results below.

Capture-to-job dispatch and the general service/backend adapter have now been
connected and physically tested. The joined captured frontend/general-session
path ran two separate frontend/native submissions at each of 1280x800 and
640x640, with 18 native bindings, retirement, matching COLOR/Z24, unchanged
READ inputs and guards, complete cleanup and verified stock recovery. See
[the joined client](captured-general-session-client.md) and
`/home/gama/sgx535-offline/captured-general-session-physical-20261102/FINAL_PAIR_RESULT.json`.
The retained eleven-role path remains valid for its original scope; it is no
longer the only general integration available.

Changing uniforms between those submissions is still an integration task.
The sealed DATA transaction and captured PARAM windows have offline/module
qualification. The latest kernel candidate binds policy from the first
acknowledged job's decoded native PARAM/TA plans before releasing its loan.
That candidate has not been physically qualified, and frontend/Linux update
transport is not yet connected. See [sealed input updates](sealed-scene-input-updates.md).

These are integration milestones, not a percentage of complete OpenGL 2.1.
The following API coverage still needs its own implementation audit and
physical tests; success of a fixed scene does not establish it:

| Area | Evidence boundary |
| --- | --- |
| GLSL 1.20 and program variation | Selected NIR/USSE programs are exercised; general language and shader combinations are not qualified |
| Uniform and buffer updates | Frozen captured inputs pass; changing inputs through the ordinary frontend/native path remains unqualified |
| Textures and samplers | The captured scene results do not establish format, filtering, wrapping or mipmap coverage |
| Render states | Selected depth writes and Z-clear are proven; general blend, stencil and raster-state combinations need API coverage |
| API object lifetime and synchronization | GEM/MMU/fence/retirement paths have scoped tests; those do not cover every GL object operation, error or reuse case |
| Contexts, presentation and desktop | Isolated API draws and specialized glxgears are narrower than complete GL2.1/EGL/GLES contexts and desktop integration |

Existing implementation references:

- [Control snapshots](scene-controls.md), [COLOR initialization](linked-color-initialize.md), [cold DPM provenance](cold-dpm-origin.md).
- [Retained job capacity](native-job-capacity.md), [bound actions](native-scene-actions.md), [native controls](native-scene-controls.md).
- [USE windows](native-scene-use.md), [TA references](native-scene-ta-references.md), [state uploads](native-scene-state-upload.md).
- [Vertex bounds](native-scene-vertex-fetch.md), [fragment references](native-scene-fragment-fetch.md), [background](native-scene-background-fetch.md), [event closure](native-scene-event-closure.md), [selected USE bodies](native-scene-use-bodies.md).

Preserve their tests and physical evidence. The next native test must consume
the existing scene through the general ownership/execution path with measured
submission, completion, retirement and correctness. Complete API/desktop and
installation claims still require their own evidence.
