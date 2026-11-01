# Real Mesa owners through the Linux resource endpoint

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
