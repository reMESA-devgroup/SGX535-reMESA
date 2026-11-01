# Real Mesa owners through the Linux resource endpoint

The subsequent [provider/fence integration](scene-provider-fence.md) now carries
this real owner graph through the normal device fence lifecycle.
That complete C route now passed physical COLOR/Z24 and stock recovery at both
1280x800 and640x640 on the same binary/kernel. Historical
network-pending notes below describe the earlier direct-transport checkpoint.
The unexecuted earlier payload remains preserved.

The general kernel executor already has physical COLOR/Z24 evidence at1280x800
and640x640. Its first collectors uploaded exported constructor images. The next
integration removes that export/upload handoff: the existing Mesa constructors
now use `remesa_linux_memory_ops()` on the real per-file GEM resource endpoint.
`remesa_linux_memory_scene_lab_submit()` consumes their retained capture packet
and sends one of the existing six closed LAB selectors on that SAME fd/context.
The kernel admission, exact-image authentication, address/permission checks,
source ownership and complete retirement contract are unchanged.

The new C transport performs no INFO, hash, readback or retry. Every entered
failure or malformed immutable/identity/publication/retirement response poisons
all endpoint resources, including unused allocation slots. Only explicit INFO
and host-only forget remain safe. Packet/output/endpoint aliases and invalid
local metadata or CPU leases are rejected before ioctl. One entered attempt
per endpoint is allowed; the kernel independently enforces once per boot.
Memory-only diagnostic recorders cannot silently omit this different ABI.
Ordinary resource-only dispatch remains refused; this does not advertise GL.

`scene_native_client.c` borrows an already-open fd from a supervising LAB
collector. It opens the memory session, generates the existing linked program,
geometry, PDS, initial/Z-clear/TA, target/background/event/REGION/DPM owners,
freezes a real capture, compares its full metadata to the qualified profile
with only owner/ID namespace translation, and enters the C transport. It never
uploads the reference GPU images. The same file description remains owned by
the parent after child exit. Capture release occurs only after authenticated
retirement; GEM remains for independent parent INFO/readback/guard validation
and explicit cleanup. A failed child causes no guessed cleanup or retry.

The real producer also owns two CPU-only GEM allocations, the copied body and
DPM plan. They are excluded from the17 GPU bindings but must be retained and
accounted for:21 GPU/guard objects plus these two make23 actual allocations.
Guard placement encloses ALL actual allocations. In particular, the old
export-only raster guard would overlap the CPU-only DPM plan. Typed existing
body/DPM snapshots supply this metadata; generic snapshot refusal is preserved.

The target metadata's draw offset also now matches the direct TA snapshot:
legacy28, initialized56. No GPU image, packet or placement bytes change.

Evidence root:
`/home/gama/sgx535-offline/general-scene-userspace-transport-20261101/v1`.
Native/UBSan/i486 producer regressions cover all three dimensions;27 capture
sets remain byte-identical to the preserved controls. Three syscall-model
variants execute the actual Linux C provider and constructors at both primary
sizes and compare all12 READ images exactly. The separate collector model
checks23-object cleanup and suppresses cleanup after uncertain child/receipt,
aliased extras or overlapping guards. None of these models renders pixels.
Physical validation of this new userspace route is pending.

Normal GL/GLES provider dispatch, production session/resource lifecycle,
complete API behavior, desktop integration and installation remain incomplete.
This LAB integration adds no per-frame cost to the established FAST path.

The first physical attempt currently waits for network reconnection: after one
requested reboot,12 boot observations and4 reconnection observations returned
SSH failure; the separate network probe reports `NO_ROUTE`. No native client,
upload or new GPU submission has been requested. The saved stock default was
preserved by the confirmed one-shot boot preparation. The current boot and
restoration of the temporary GRUB entry remain unverified until SSH returns.
Do not repeat the reboot or launch against an assumed boot. Restore the original
GRUB and verify the actual module/observer/seven N controls/source freshness
before executing the already-prepared single-scene collector.

The remaining ordinary frontend contracts are explicit in current source:
Linux memory `dispatch` still returns `REMESA_NOT_ISSUED`; persistent shader
submission still rejects execution without a native scene/fence handoff;
ordinary Gallium draw rejects a resource-only backend and CPU depth target.
Its legacy32x32 restrictions are not lifted by this LAB bridge. The existing
native2D factory has a32MiB resource-only budget, separate from the96MiB scene
owner. These require real integration, not another ISP/PDS/USSE investigation
or unconditional capability changes.
