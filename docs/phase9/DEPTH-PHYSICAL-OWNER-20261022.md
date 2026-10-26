# Typed Z backing preparation and physical boundary

Status: IMPLEMENTED_OFFLINE / DEPTH_PHYSICAL_BLOCKED. This checkpoint resumes the sealed depth-clear-shader contract without replacing its evidence. No new SGX submission, completion, retirement or GPU readback occurred. Read-only SSH inspections succeeded on Mini12.

## Implemented

remesa_persistent_depth_allocate and remesa_persistent_depth_snapshot reuse existing create/pin/map, VA reservation, registry generation, budget, cleanup and quarantine mechanisms. Width/height are padded to32 elements and row alignment is inherited from the resource allocator. Only cpp2/4 and the existing bounded CPU descriptor profile are accepted. The Z backing is distinct from other allocations and reserved ranges.

The snapshot derives handle, VA, owner, generation and initial surface descriptor from the live endpoint-owned BO. Callers cannot supply another BO's device address. Initial content remains UNDEFINED regardless of zero-filled provider memory; allocation is not clear. Active CPU leases, foreign/untyped/deleted objects, invalid dimensions/format, budget/VA failures and output aliases refuse without publishing a partial snapshot. Existing destroy_bo owns teardown. No new release/retirement semantics are introduced.

A depth-tagged4096byte BO cannot enter the color diagnostic submit path. Larger Z BOs also refuse the existing exact-color ABI. Gallium Z format/DSA/GLX visuals and all execution gates remain unchanged. These APIs prepare storage and CPU descriptors only; they do not authenticate requester coverage, TLB visibility or physical Z format.

## Tests actually executed

The persistent-owner suite passed native, UBSan and i386/QEMU; it includes the new depth allocation/snapshot cases and existing resource/fence/rollback regressions. Actual Meson archive test passed. Mesa25.2.0 static and DRI rebuild passed; the existing linker RWX-segment warning is retained in its build log.

New selected cases cover33x35 cpp4 ->pitch64/padded64/16384bytes,1x1 cpp2 ->logical2048/page4096bytes, reserved VA hole avoidance, all five split address contributions, descriptor contents, no read-before-initialization, lease refusal, invalid/foreign/deleted BOs, create/pin/map failure rollback, budget/aperture refusal, balanced cleanup, and zero dispatch. These are mock provider/CPU tests, not physical allocations or Z captures.

The original20 ISP differential cases and parser metadata are unchanged and retained in the parent manifest; broad encoder/NIR suites were not rerun without relevant source changes. The prospective32x32 oracle was executed and six expected planes generated with exact rational coverage checks. Those files are labelled expected, not readback.

## Physical inspection and exact blockers

The Mini12 boot UUID is bb427a9c-1db0-42e9-aadc-9df120057ffb. uname reports i686 kernel5.10.240-antix.1-486-smp; PCI is8086:8108/1028:02b1. Source guard remains STARTUP_LIFECYCLE with asserted_reset127/released0. Both runtime controls are N. No register MMIO was used and CORE_ID/REVISION were not freshly read.

The gma500 build-id note SHA2564cdea656... matches the preserved old green module note. That module's whole-image validator does not admit the later independent-VS/quarter-coordinate carrier. This is an actual profile mismatch, not an absent SSH connection. The later qualified quarter module has notea66ca2de... and SHA58c8b03f... . Updating a controller hash alone would mask incompatible admission. No such update or submission was performed.

Depth additionally has no role in the current retained10-binding job ABI; the existing profile admits one exact triangle/REPLACE framebuffer. A separate typed Z allocation cannot be appended to that fixed command/image contract or substituted for another role. A clear plus three ordered draws, compatible state upload, nonzero VS Z/viewport transport, visibility/containment and preservation remain unqualified. They cannot be inferred from the CPU descriptor or from another retained owner's lifetime policy.

## GLX without depth

A depthless32x32 GLX triangle is already implemented and is independently useful. Its next physical boundary is the previously qualified quarter boot plus a closed current-boot executable/library/runtime controller and X11 authentication. It does not require Z. The original glxgears requests a depth visual and also needs fixed-function transforms/lighting, broader geometry, preservation and repeated draws/swaps. Disabling its depth request would not establish original glxgears qualification.

## Next discriminating work

For depth, compose and independently validate the full clear/state-upload/three-draw scene with an explicit Z owner before designing a new physical profile. Do not weaken the old exact profile. DEPTH_EXPERIMENT_INPUTS.json freezes window geometry, draw order and expected planes; comparisons alone are not hardware proof. The first bounded profile must use one submission with no retry, complete memory ownership/guard capture and authenticated completion/retirement. LESS/write predicts276magenta and748opaque-black pixels; matched ALWAYS/no-write controls predict the final far color. Reversed order and raw Z captures distinguish selected alternatives; cross-submission preservation requires a separate later no-second-clear test.

For GLX, controlled boot of the already-qualified quarter image with stock rollback preserved is the smallest independent physical step; then revalidate state and all controller inputs. No boot, kernel/module replacement, persistent setting or runtime gate was changed in this checkpoint.
