# Bound Gallium inputs into the complete gears capture

`remesa_context_prepare_gears_attribute_scene` connects the bound canonical
attribute-normal NIR program, constants, vertex and index buffers, and cold
COLOR/Z attachments to the existing complete common gears scene constructor.
It shares the ordinary preparation checks but is explicitly selected; normal
`draw_vbo`, capabilities and the working GLXGEARS/FAST path are unchanged.

The helper reuses `remesa_context_gears_geometry_snapshot` for attribute
formats, instance/topology restrictions, synchronized resource mapping and
index-window expansion. Its immutable 32-byte POSITION/NORMAL records are
copied into 48-byte records with a zeroed unused third float4, matching the
existing physical PDS profile. Both attributes retain their original bits.
This is record packing, not CPU vertex, lighting or pixel execution. The
snapshot already expands the selected index window, so the common owner uses
its existing sequential triangle indices.

The existing parameter preparation helper reads the bound VS constant block
and creates the source37/transfer40 owner. User constants use the bind-time
snapshot; resource constants retain their existing synchronization and map
rules. The complete capture retains that owner after its temporary public
reference is released. All temporary geometry, CPU packing and map leases
are released before returning. Ordinary failures unwind; uncertain allocation
failures preserve the existing quarantine contract.

Both wrappers share attachment, size, linkage, caller-supplied state and
request validation. The gears path additionally rejects output/input aliases
with bound constant resource headers before mapping or allocation. It only
accepts the canonical attribute-normal selected profile on the persistent
resource backend. Existing-content attachments remain refused. The old
preparation wrapper continues through its previous three-input path.

Evidence is at
`/home/gama/sgx535-offline/bound-gears-scene-20261101/v1/`.
Actual Mesa/NIR tests cover four dimensions (1280x800, 640x640, 641x479 and
1600x768), nonindexed inputs, user u16 indices with a nonzero start, resource
u32 indices with negative bias, constants changed after binding, missing or
mapped inputs, aliases, incompatible banks and ordinary/uncertain allocation
failures. The modified Gallium screen and fixture also pass UBSan with the
real NIR libraries. The previous 26 context cases pass and four original
capture trees remain byte-identical.

Six complete exported packet/placement/READ graphs pass the actual retained
kernel core and the full TA/state/vertex/fragment/background/event chain in
native, UBSan and i486 models, followed by modeled completion, retirement and
cleanup. These tests use fake GEM/hardware callbacks and do not prove a new
physical draw. All protected physical evidence remains unchanged.

The remaining immediate physical boundary is the closed LAB transport:
profile generation, request/trace structures, controller and client still
describe 17 bindings. The integrated graph has 18 bindings and 11 USE entries.
Do not truncate the extra owner, reinterpret old reserved fields, weaken old
profile gates or use generic test coefficients as pixel expectations. A
separate qualified transport/profile/client/oracle set must precede a fresh
recoverable Mini 12 experiment. The canonical program and preserved positive
geometry/coefficient witness should be reused.

This does not yet add context-level submission for the new profile, ordinary
OpenGL/GLES admission, clipping/perspective/texture support or an installable
complete driver. No FPS or physical qualification is claimed here.
