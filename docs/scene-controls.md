# Control groups from the retained scene

`remesa_persistent_scene_controls_snapshot()` connects a frozen scene capture to
the existing original CPU raster and TA control encoders. It derives the raster
background/event references, COLOR/Z dimensions, pitch and extents, TA stream
address and separate region control groups from the retained owners. Callers
supply a capture token, not addresses, CODE, descriptor sizes or a clear value.

The selected initial branch uses UNDEFINED depth, stencil0 and the original
finalization predicatefalse. The Z-clear owner now retains the same validated
float depth supplied to its existing geometry encoder; raster control uses that
value directly. The geometry and shader encoders and CODE84 are unchanged.
This does not initialize COLOR, Z or DPM contents or infer preservation semantics.

The result contains raster208 bytes, TA56 bytes, seven TA region register/value
pairs and the separate raster408 pair. These remain separate CPU control groups,
not a concatenated admitted native packet, GPU CODE, another BO or direct MMIO
writes. All five incomplete flags remain set. The next native consumer still
needs authenticated resource holds, initialization, ordered execution, required
completion and retirement, failure handling and physical qualification.

A capture retains its four parent identities along with its existing deduplicated
holds. Internal metadata helpers can inspect those held objects after public
frontend deletion without fabricating ownership. Public snapshots still require
public ownership and retain output/lease checks. Controls require a fixed event
PDS owner; auxiliary-only captures return ENOTSUP. Invalid/stale/foreign tokens,
misaligned output and owner aliases reject without modifying output. There is
no allocation, provider I/O, synchronization, hashing, pixel readback or rendering
in control snapshots. External serialization remains required.

Tests cover both capture contracts at1280x800,640x640 and641x479, full frontend
release, repeated stable controls, invalid tokens/output, and zero provider I/O.
An opt-in fixture varies clear depth between1 and0.25; its default remains1 for
previous tests. Assertions verify the retained scalar against existing generated
vertex bytes and the raster clear register, all five Z address contributions,
background/event references, COLOR extent, TA stream and region groups. The
underlying numeric codecs retain their previous original-constructor oracle
qualification; this increment tests their owned binding, not a new decoder.

Evidence root: `/home/gama/sgx535-offline/scene-controls-20261031/v1`.
`source-tests/result.json` records native/UBSan/statici486 results;
`actual-mesa-scene-controls/RESULT.json` records a one-object private rebuild
using actual Mesa/Gallium/NIR flags, simulated resources and unchanged parent
archive/other members. `REGRESSIONS.json` records previous background, Z-clear
and fixed event-profile tests. `FINAL_BINDING.json` pins sources and receipts.
No GPU operations, new physical qualification, desktop acceleration, full GLES2
or desktop GL2.1 completion is claimed by these CPU/provider-model results.
