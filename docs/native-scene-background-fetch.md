# Retained background references and COLOR binding

The selected native scene validator now follows the existing background68
object into primary64 and empty-secondary4 PDS windows, and matches the primary
task to a held pixel-master USE8 entry. This continues the fragment reference
chain without admitting a general scene for execution.

`include/remesa/background_fetch.h` checks the original constructor's complete
68-byte object: compressed primary/secondary references, control, coefficients,
scalar stores, packed dimensions and three depth1 bit patterns. Integer bit
operations reproduce the existing stores; no floating point runs in the kernel.
References require 32-byte alignment, non-overlap and complete spans inside the
explicit selected PDS window. That expected window remains a CPU contract, not
a hardware-register observation.

The BGRA8/tag4 primary is DATA48/CODE16. Its known task, texture descriptor,
dimensions, COLOR address and CODE fields match the unchanged existing producer.
Unknown DATA4..7 and11 retain their masks. Only64 committed bytes are consumed;
secondary consumes4, even with larger reserves. No padding instruction is added.
The selected dimensions and address are constructor inputs. This comparison
alone does not prove texture layout, fetch semantics or pitch behavior on GPU,
particularly for non-aligned widths.

`remesa_gem_scene_background_check_locked()` requires the existing full private
loan/action/control/USE/TA/state/vertex/fragment checks. Under the same endpoint
and MMU locks, with the private root bound, it resolves actual backing and CPU
views for the background and PDS windows. They require READ/cache1 policy. The
primary COLOR address must equal the retained COLOR binding already sized and
validated by the controls checker. COLOR pixels are never read here. The task
must match an immutable PIXEL USE8 entry and cannot overlap its PDS windows.
The result returns held IDs and offsets; it is unchanged on failure. Caller
scratch may change during prerequisite validation.

The builder is explicitly non-inlined and entered after prerequisite checks,
so its transactional result is not retained across that chain. Actual i486
compiler stack frames are recorded with executable-section equality checks.
The new wrapper uses44 bytes and its builder200 bytes in this build.
This is not a complete future executor/caller/IRQ stack budget. Large retained
owner/scratch allocations must still precede the MMU writer interval.

No allocation, new flush/wait, pixel readback/hash, new MMIO, source admission
or GPU operation is introduced. The existing canonical eleven-role executor,
producer implementations, gates, protected evidence and normal profiles remain
unchanged. The new checker validates selected known construction and ownership;
USE shader body, unknown fields, background texture runtime semantics, event
closure and the ordered general executor remain separate requirements.

Evidence root:
`/home/gama/sgx535-offline/native-scene-background-fetch-20261101/v1`.
Pure tests compare the unchanged object and primary encoders for six dimensions
(including all four project targets and1/2048 boundaries), three address windows
and four task fixups. They cover all fixed object bytes, primary known fields,
CODE, unknown masks, capacity, alignment, overlap and transactional failures.
Kernel models extend the17-buffer fixture with actual modeled background/PDS
input and a dedicated USE8 entry; they cover COLOR mismatch, wrong master,
RW PDS target, CODE/secondary/geometry tampering, unknown DATA and stability
after publication. Native, UBSan and static i486 results are CPU evidence.
Full-module builds are uninstalled and physically unqualified. No complete API,
whole-desktop acceleration, installation or performance claim follows.
