# Bound native scene action groups

The general scene path must consume the actual retained allocations rather
than insert its seventeen resources into the old eleven-object executor.
`remesa_gem_scene_actions_prepare()` now constructs the existing DPM, TA and
raster action groups from kernel-held object identities. It does not execute
them or admit a complete scene.

The kernel-only command section contains dimensions and three held IDs, never
caller-supplied GPU addresses or register lists. Its schema, size, alignment,
extent and reserved field are checked against the immutable kernel command
copy. REGION must have exactly the storage extent derived by the existing
layout encoder. Both 32 MiB DPM pools must already have a kernel zero-origin
claim. All three scratch objects must be fresh zero storage without CPU
aliases. Preparing the groups reserves REGION against selected CPU snapshots;
job cancellation/reuse invalidates the reservation and ticket.

The existing CPU producer is staged with include-path changes only. Its action
ABI must match the selected native contract. The groups remain 29 DPM actions,
31 TA actions and 20 raster actions, including the strict DPM completion mask
15. No CODE, geometry, shader or register sequence is invented to fit the new
allocation set. Preparing groups performs no allocation, mapping, clearing,
cache flush, MMU switch or MMIO.

`remesa_gem_scene_actions_check_locked()` requires the endpoint lock followed
by the MMU writer lock and an already bound private root. It uses the complete
private-loan validator, then authenticates the ticket, immutable command,
scratch roles, GPU read/write permissions, cache policy and absence of CPU
aliases. It regenerates and compares the small action arrays. This is not a
framebuffer comparison or a diagnostic hash in the rendering loop. Required
publication/coherency remains in the existing private publisher; publication
consumes zero provenance without invalidating the job-local reservation.

Mesa's new `remesa_persistent_linked_region_allocate()` allocates undefined
REGION storage without the legacy CPU zero/verify/sync sequence. Metadata,
footprint and frozen capture may retain that storage, but CPU reads still
return `EAGAIN`. Allocation alone does not prove kernel zero origin or GPU
initialization. The old `linked_region_prepare()` behavior is unchanged.

## Validation and remaining integration

Evidence directory:
`/home/gama/sgx535-offline/native-scene-actions-20261101/v1`.
Kernel models exercise seventeen allocations, immutable command ownership,
zero origin, optional CPU views, permissions/cache/MMU refusal and action
tampering at 1280x800, 640x640, 641x479 and 1600x768. These models execute no
GPU operation. Mesa tests retain both legacy and undefined REGION variants,
including capture lifetime, active-lease refusal and bound control snapshots.

The first Mesa test failed because its old lease case tried to read undefined
REGION storage. That failure is preserved in `mesa-source-tests/`; the test
now acquires its lease through the initialized background for the raw variant.
The production refusal was not weakened.

Next, integrate complete image/control/program validation and ordered native
execution under source/device ownership, private MMU binding, required
publication, completion and retirement. These helpers alone do not establish
initialization of other resources, source ownership, interrupt sequencing,
post-render scratch reuse or native scene admission. The old fixed-profile
executor and LAB gates remain separate. The new module must be physically
qualified before hardware support or performance is claimed. Whole-desktop
acceleration, GLES 2.0, desktop GL 2.1 and installation remain incomplete.
