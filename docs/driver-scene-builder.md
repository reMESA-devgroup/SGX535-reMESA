# Driver-owned assembly from frontend COLOR/Z resources

`remesa_persistent_linked_scene_prepare()` moves the existing cold linked-scene
composition into the driver library. Its request supplies frontend-owned COLOR
and typed Z attachments, semantic shader linkage, raw vertex/index input,
existing ISP/viewport state, banks and clear depth. It composes the established
program/geometry/PDS/state/close/body/initial-prefix/TA/target/background/event/
REGION/DPM owners and freezes their existing capture. No encoder is replaced.
It performs no dispatch or capability promotion, CPU vertex transform, pixel
rendering, CPU attachment clear or64MiB DPM-pool clear.

The frontend keeps its attachment references. Generated temporary public owners
are dropped after capture, leaving the existing retained graph responsible for
their lifetime. A successful capture can be cancelled, or submitted through the
previously qualified provider/device-fence path. It remains valid after frontend
owner deletion. No generated public BO tokens need to escape into Gallium.
Ordinary preparation failure unwinds generated owners and leaves caller COLOR/Z
untouched; uncertain ownership retains endpoint quarantine and refuses close.

This is cold scene assembly for the existing linked shader/PDS restrictions,
not preserved-color rendering or a steady-state scheduler. Native submission
still authenticates actual inputs, initialization/load policy and ownership.
The function does not enable ordinary GL/GLES dispatch, add API capabilities or
remove any native gate. It is not used by the already compiled physical client
from commit3bc7b59. No new hardware qualification is claimed for this wrapper.

Evidence root: `/home/gama/sgx535-offline/driver-scene-builder-20261101/v1`.
`RESULT.json` records39 cases across native/UBSan/i486, including valid1280x800,
640x640 and641x479, dimensions/viewport mismatch, invalid indices/link/banks,
late invalid clear depth, pool budget exhaustion, ordinary and uncertain create
failures, output aliases and deleted frontend owners followed by a published
provider fence. Complete fake COLOR/Z/DPM allocations remain unchanged A5;
ordinary rollback leaves only the caller's two attachment objects.

`ASSEMBLY_EQUIVALENCE.json` compares nine capture sets with the preserved
qualified constructor inputs: every READ image is byte-identical, all17 GPU
addresses/extents/permissions agree, and the whole packet agrees after only
context and file-local handle translation. Geometry remains raw P/N/L data.
The31 protected files are unchanged.

The first test compilation referenced a nonexistent linkage-level schema field;
that test failure is preserved. The next test exposed output-token validation
before overlap rejection. The implementation now rejects output/input overlap
before dereferencing the output token, and all alias cases pass. No native or
ownership gate was relaxed. The evidence retains both earlier failures.
