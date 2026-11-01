# General executor integration: reuse the completed rendering components

Reviewed against the current source on 2026-11-01, following the user's explicit
instruction to integrate the existing ISP, DATA, PDS and USSE components rather
than restart their implementation or reverse engineering. Earlier shorthand
such as "ISP still missing" or "DATA admission next" was too broad. The missing
work is a consumer and ownership handoff for the new allocation graph.

## Completed components to reuse

| Component | Existing implementation/evidence | Integration use |
| --- | --- | --- |
| Physical 3D/ISP/depth/Z-clear | [physical depth/lighting/occlusion](phase9/DEPTH-LIGHTING-PHYSICAL-20261023.md), [physical GLXGears](phase9/PHYSICAL-GLX-STATUS-20261027.md) | Preserve the actual encoders, register groups, tested state transitions and oracles |
| PDS/USSE and event DATA/CODE | Existing Mesa encoders, [fixed event profile](linked-event-profile.md), [C PDS upload qualification](private-page-upload.md) | Generate through these owners; no substitute instructions, CODE filler or new decoder project |
| Owned dynamic scene | `remesa_persistent_scene_capture`, controls/USE/TA/vertices snapshots and [scene capture](scene-event-capture.md) | Transfer the actual immutable held graph into the job |
| Actions and control DATA | [native actions](native-scene-actions.md), [native controls](native-scene-controls.md), original `lab_scene_actions.c` and shared serializers | Consume the already-built DPM29/TA31/raster20 actions and raster208/TA56 controls |
| Input/resource checks | TA/state/vertex/fragment/background/event validators and [selected USE bodies](native-scene-use-bodies.md) | Apply these existing checks to the exact held job, rather than recover its encodings again |
| Initialization mechanisms | [native COLOR fill](linked-color-initialize.md), [cold DPM origin](cold-dpm-origin.md) | Invoke the existing operations in order; bind their actual job ownership |
| MMU/source/completion | Private GEM loan/publisher/root restore, [source session](private-source-session.md), [physical C session](private-session-transport.md) | Preserve exclusion, publication, attribution, retirement and quarantine |

These results apply to their documented profiles. They are sufficient reusable
components for the next integration; general ISA decoding is not its prerequisite.
They do not themselves claim complete GLES2/GL2.1, textures, desktop integration
or installation.

## Concrete gaps in the current call graph

1. **Capture to native job.** The capture APIs deliberately reserve no GPU
   operation and do not enter provider dispatch. The general job serializer and
   submission entry must carry held IDs, immutable sections, exact ranges and
   generations into the existing GEM job/loan ownership. Current snapshots already
   supply the source data; they must not be replaced with hand-authored shaders.
2. **General service consumption.** `remesa_gem_private_execute()` refuses
   `count != SGX535_BO_COUNT`. `remesa_fixed_private_scene()` makes the same
   check and ties each descriptor to a fixed role. `sgx535_fixed_service` embeds
   eleven BOs; its bind/prepare/fire stages reconstruct groups from those roles.
   The new one-draw captured graph has seventeen allocations. The software GEM
   bound32 is already implemented; it does not make this service generic.
3. **Backend/source adapter.** The current backend compares action payloads with
   fixed-role reconstructions and its source observation is associated with that
   backend/owner/session. A general adapter must consume the validated action,
   control and USE plans while retaining that ownership and attribution. It must
   not cast17 objects into11, manufacture unused role descriptors, relax gates or
   synthesize a completion. Existing lifecycle/IRQ/MMIO helpers are reuse targets.
4. **One ordered operation.** Retain the complete context before the MMU writer;
   validate default mappings before binding; claim initialization provenance;
   bind the private root; validate the retained closure; publish once; perform the
   existing maintenance/init/USE/DPM/TA/raster sequence; require owned completion;
   transfer CPU visibility; close the source observation; restore the root;
   acknowledge/retire and finally release. Failure keeps uncertain resources and
   source state quarantined. These steps already exist in the fixed executor;
   the missing contract is their connection to the general plans.
5. **Frozen input authority.** The existing private input-seal/LAB authentication
   pattern and typed owners must authenticate the exact final images supplied to
   this adapter. Low-level unknown masks are not a reason to reimplement DATA,
   invent field meanings or add a new floating-point viewport encoder in kernel.
   A changed immutable input must be refused by the selected submission contract.

The existing `incomplete` flags are deliberately conservative and remain31.
Do not interpret each bit as an absent encoder or clear bits without a real
operation consuming their corresponding contracts.

## Next physical integration qualification

Use the actual existing encoders/owners, not a new red/green prototype or CPU
renderer. First connect and test the general adapter offline, including failure
at each transition and the17-resource handoff. Preserve the canonical11 route
as the control. If the current candidate module also needs a runtime regression,
use the existing bounded private C/PDS collector as that control; its success
must not be labelled general17 execution.

The first general hardware experiment should be a cold, bounded offscreen draw
using the captured graph, the existing native initialization and the same build
at640x640 and1280x800. Do not introduce DPM reuse, asynchronous queuing, a new
shader, window presentation or performance tuning in that experiment. Check
SGX535 core/rev121 under the owned hardware entry, source-attributed issue and
completion, retirement/root restoration, final COLOR/defined-Z readback and
buffer guards against the retained oracles, then close and verify N/N/rollback.
LAB verification stays outside normal FAST rendering.

A fresh read-only observation is retained at
`/home/gama/sgx535-offline/general-executor-isp-contract-audit-20261101/v1/MINI12_READONLY.json`:
stock boot `94b878bd-b11b-44e4-b2ad-5c8465ade45e`, expected antiX5.10.240 i486
kernel/module note, no experimental controls/observer/source endpoint, original
GRUB hash/default and empty next entry. This proves current recovery state only;
it is not a new GPU operation or fresh MMIO core/revision observation.

Old one-shot launch/reboot scripts pin their historical boot, module/image and
consumption markers. They must not be rerun against this new stock boot. Prepare
a new pinned candidate and one-shot plan with automatic stock recovery before
mutation. No test boot, GPU operation or rearm was scheduled by this audit.

The immediate next implementation is the general job/service/backend handoff,
reusing the components above. Normal GL/GLES frontend dispatch, full required API
behavior, X11 integration and installation follow that real execution path; they
remain part of the unchanged full-driver objective.
