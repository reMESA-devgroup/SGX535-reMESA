# Retain the fixed event program with its scene

`remesa_persistent_scene_footprint()` and the existing unissued scene-capture
API now accept either the original auxiliary owner or the new fixed pixel-event
program owner in their event argument. The auxiliary-only contract remains
unchanged. Supplying a program owner adds its allocation as a seventeenth READ
reference with role `REMESA_SCENE_REF_EVENT_PDS`; the auxiliary remains a separate
READ reference. Reservation400 is reported as logical ownership capacity, not
committed CODE size or measured GPU consumption.

The program is revalidated through its typed snapshot before its auxiliary is
resolved. Existing same-target, live mapping, generation, disjoint address,
bank, memory access and endpoint checks remain in place. The scene must refer
to the exact same target owner through background, event and region; a numeric
address match cannot substitute ownership. Captures freeze both the event
program and auxiliary, retain the graph after public frontend destruction, and
release it through the existing cancellation/quarantine path. They are unissued
userspace holds, not native GPU jobs, fences or completion acknowledgements.

All five incomplete flags remain set. In particular, retaining the fixed PDS
program does not yet qualify its event execution as part of a complete newly
constructed scene. Initial COLOR/DPM contents, full raster/control preparation
and native lifecycle/admission still need their own implementation and physical
validation. No existing gate, capability, shader, geometry or CODE84 changed.

Tests exercise both event contracts at1280x800,640x640 and641x479. They check
the additional READ reference, all eight simultaneous capture slots, mapped
buffer rejection, frozen program/auxiliary maps, stale and foreign tokens,
public frontend destruction, stable repeated snapshots, ordinary cancellation
and uncertain disposal. No provider I/O is added to capture/snapshot. Existing
auxiliary-only scene-footprint tests remain separate regressions.

Evidence root: `/home/gama/sgx535-offline/scene-event-capture-20261031/v1`.
`source-tests/result.json` records native/UBSan/statici486 results;
`actual-mesa-scene-event/RESULT.json` records a private persistent-object rebuild
with actual Mesa/Gallium/NIR and simulated resources/fences. The previously
qualified fixed-profile archive and other members remain unchanged.
`REGRESSIONS.json` records the previous event-profile and scene-footprint tests;
`FINAL_BINDING.json` pins final source and receipts. These tests perform no GPU
operation and are not physical rendering or installed-driver qualification.

`REMOTE_STOCK_OBSERVATION.json` separately records a successful read-only Mini12
observation: boot `f507f3b8-20e9-4ed8-8a80-73e417ab5cbc`, kernel
`5.10.240-antix.1-486-smp`, expected stock gma500 module note, no test observer or
controls, original GRUB configuration/default and empty next entry. This closes
the earlier unverified stock-return condition. It does not report a new GPU
test or establish that the previous reboot command was acknowledged.

The retained graph now also supplies the original raster/TA and separate
region control groups through [scene-controls.md](scene-controls.md). This
CPU binding leaves initialization and native admission pending.
