# Capture metadata connected to native job preparation

`remesa_persistent_scene_packet_snapshot()` derives a compact submission
snapshot from the existing frozen capture and its actual typed owners. It
reuses the controls, USE and vertex snapshots, target metadata, original clear
depth bits and REGION/DPM owners. It keeps all original capture holds and adds
no provider allocation, map, readback, synchronization, hash or instruction
encoding. The caller retains the capture through the complete native lifecycle.

The shared internal `remesa/scene_packet.h` contains owner/serial, file-local
IDs, access bits, dimensions, committed ranges and bank selectors. It contains
no GPU addresses, pointers, register words or shader CODE. It has no ioctl
number and is not a stable public ABI. Metadata validation checks bounded
counts, membership, duplicates, reserved fields and unused tails. Existing
native validation remains responsible for full section/image/reference and
hardware admission. An unissued capture snapshot is not an execution receipt.

The packing helper emits only the populated action, control, USE and vertex
sections. The tested single-draw graph has 17 allocations, ten USE windows and
two vertex windows; its immutable job metadata occupies 448 bytes rather than
the fixed array capacity. Those bytes are CPU job metadata, not PDS/USSE CODE
or a measured GPU fetch extent. Counts/reservations and actual program extents
retain their existing meanings.

`remesa_gem_scene_packet_prepare()` resolves each packet ID/generation against
the actual memory endpoint. VA, backing size and pages come from that endpoint,
then the existing general job preparation takes actual GEM holds and copies
commands. All captured GPU references retain the existing cached=1 contract,
including REGION/DPM. Writable scratch still has no exported CPU view. Context
identity must match; malformed metadata and missing/foreign backing refuse.
Temporary kernel storage is allocated before preparation and freed after the
existing prepare has copied its commands. Any retained partial job remains in
the caller's stable context according to the existing failure rules.

The kernel section sizes and binding/USE/window capacities have compile-time
checks against the retained preparers. The original all-view checker, canonical
executor, native backend, controls, encoders and admission gates stay intact.
The private stager adds only the bridge include and shared header/component.

## Verification

Evidence root: `/home/gama/sgx535-offline/general-scene-packet-20261101/v1`.

The real userspace capture suite compiles current sources against the existing
Mesa headers in native, UBSan and static i486 configurations. Its actual owner
constructors produce the packet at 1280x800, 640x640 and 641x479, across the
existing auxiliary-only/fixed-event/raw-REGION capture variants. It tests
17/10/2 counts, exact section offsets/448 bytes, insufficient capacity,
duplicate/missing IDs, unused tails, stale/foreign tokens and unchanged output
on refusal. Existing zero-provider-I/O and capture lifetime assertions remain.
This is source compilation/testing, not a rebuilt Mesa driver archive or GPU
qualification.

The kernel scene suite now prepares its real modeled GEM jobs through the
shared packet bridge before the existing 208 service cases, 144 native-consumer
cases and four complete transaction successes per variant. Each variant also
runs the ten isolated executor failure processes. Wrong endpoint owner and
duplicate bindings refuse before preparation. Original compact-section parsing,
bound-root/image/USE/geometry checks, publication, source/root/retirement models
remain active. Hardware/source callbacks are modeled; all tests perform zero
GPU operations.

The initial bridge incorrectly derived an uncached REGION mapping. The first
kernel model refused its cache policy; code review also confirmed the existing
REGION/DPM gate requires cached=1. The bridge now derives cached=1 for all these
captured references. The model's old deliberately uncached unrelated input is
kept for its existing cases; the packet-specific fixture checks the new exact
all-cached policy. The superseded compilation was explicitly canceled with its
own process group, and its partial artifacts and failure receipt are preserved.
No gate was relaxed or failed candidate installed.

## Remaining live entry

The capture snapshot and native preparation bridge are connected in source.
A live transport/admission entry still must retain the existing DRM/backing
session, source lease and producer reservation, authenticate the actual input
construction, SGX535 rev121 identity and selected PDS window, and call the
ordered executor with stable context/backend/observation storage. It must
transfer capture lifetime into real completion/visibility/retirement and retain
all uncertain failures. A metadata token or NEEDS flags must never override
those gates. This bridge does not enable arbitrary existing memory ioctl
submission or turn the canonical eleven-role path into a seventeen-role job.

Fresh bounded recoverable physical qualification follows that entry. Complete
normal GL 2.1/GLES 2 dispatch/resource behavior, desktop integration and driver
installation remain incomplete.


The subsequent [real producer/consumer replay](mesa-kernel-scene-replay.md)
now feeds actual captured READ image bytes, unchanged GPU addresses and packet
metadata through the kernel closure and ordered transaction. Both source and
hardware callbacks remain modeled, so this does not enable live transport or
supply physical qualification.
