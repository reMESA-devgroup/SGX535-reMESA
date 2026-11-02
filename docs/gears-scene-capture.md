# Complete common gears captures and kernel closure

The existing attribute-normal gears program can now be assembled into a
complete common scene capture. This connects the previously integrated
CODE/PDS/parameter/TA owners to attachments, background, region, DPM and event
owners. It does not implement a new shader or change the production FAST loop.

`remesa_persistent_gears_attribute_scene_prepare` takes the existing scene
request and a live source37/transfer40 parameter owner. It reuses the ordinary
scene builder and selects the explicit gears program, PDS, state and ordered
body constructors. Successful assembly retains the caller's parameters
through the graph without consuming their public reference. Ordinary failures
unwind generated owners and preserve attachments; uncertain failures retain
the existing endpoint quarantine. The old scene API continues to reject gears.

The complete selected capture has 18 retained buffers, 11 USE entries, two
vertex windows, and 104 effective TA bytes, including the initial state,
clear, draw-state/PARAM/index commands and close. Allocation capacity is not
serialized as extra commands. Neither the attachments nor the two 32 MiB DPM
pools are cleared on the CPU by assembly. Submission remains separate.

## Vertex and fragment authentication

Both userspace vertex-window snapshots and the kernel vertex checker now
find index/PDS references by authenticated kind and draw number. The old
positional triple calculation is incompatible with the additional PARAM
reference and is no longer used. Prefix and legacy draws retain their exact
existing program/fetch checks.

For the parameter grammar, the kernel requires the actual held vertex USE
entry to cover exactly 320 bytes and checks every word against the existing
attribute-normal program. The predicate was derived from CODE312 plus its
typed tail8 in the preserved Oct24 positive physical input and matched against
the current common encoder output. It does not emit CODE, recognize arbitrary
programs or replace address validation. The existing PDS64 decoder checks the
compiled producer fields, its source and stride48. Committed raw vertex and
index extents, READ/cache1 permissions, overlap checks and USE ownership remain
mandatory. The preceding state stage still authenticates PARAM160 and EE8.

The fragment checker locates each prefix/draw state's PDS reference in the
authenticated TA and its corresponding checked upload. Initial-state and
PARAM uploads cannot shift that selection. It then applies the existing
fragment binding, PDS, source/USE ownership and fragment-body checks.

## Evidence and remaining physical boundary

Artifacts are under
`/home/gama/sgx535-offline/gears-kernel-fetch-20261101/v1/`.
The coherent Mesa/NIR build tests the real gears builder and bind-time constant
callbacks, then assembles and exports the common capture at 1280x800, 640x640
and 641x479. Its raw vertex records are explicit test inputs, not snapshots
of a bound vertex buffer. The test covers 13 success, invalid input, ordinary
failure, uncertain retention, alias and modeled submission cases. The ordinary
Gallium draw callback remains closed to this new scene profile.

The exported packet, placement and READ images pass the actual retained kernel
core and all vertex/fragment/background/event validators in native, UBSan and
i486 models. Modeled execution reaches completion and retirement with cleanup.
The separate kernel fixture rejects all 2,560 single-bit CODE mutations at
four dimensions, including 1600x768. These are model results, not GPU results.
Previous captures and protected physical evidence are preserved.

The existing closed physical profile generator and controller currently
describe 17-buffer, 10-USE scenes. They do not authorize this 18-buffer graph.
Before hardware, the bound geometry path must supply the explicit 48-byte
record packing, and a separate exact profile/client/oracle set must carry the
complete gears inputs without changing prior profiles. Generic fixture
coefficient values are not physical rendering expectations. Reuse the
preserved positive geometry and coefficient witness for that experiment,
qualify the whole client/controller offline, then use fresh recoverable boots
with identity, submission, completion, retirement, readback and integrity gates.

No new physical draw, FPS, general perspective/clipping/texture support,
OpenGL/GLES conformance, desktop acceleration or installable-driver completion
is claimed by these changes.
