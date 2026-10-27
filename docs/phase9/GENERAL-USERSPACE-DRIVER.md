# General userspace driver: desktop OpenGL and GLES 2.0

The active target is hardware-accelerated desktop OpenGL and OpenGL ES 2.0
through a common Gallium backend, with an installable driver. X11 integration
comes later. The initial physical target remains antiX i686 on the Dell Mini
12, PCI 8086:8108, SGX535 rev121. No OpenGL version/conformance or general
acceleration is currently claimed.

Video acceleration is a subsequent user-requested milestone after general
3D/2D acceleration and the installable driver. Decoder capability, its separate
hardware/firmware interfaces, API integration and physical playback must be
qualified independently; SGX rendering does not establish video decoding.

The user requires paired 1280×800 and 640×640 validation for rendering
candidates, on both desktop OpenGL and GLES 2.0. Exercise 641×479 occasionally
for odd dimensions, pitch, tile boundaries and clipping. Record each API/size
result separately: a pass at one size cannot approve another, and the retained
1280×800 gears results do not establish the other sizes. Initial checks remain
offscreen; X11 window integration is later work.

The required rendering matrix for each candidate is:

| API | 1280×800 | 640×640 | 641×479 |
| --- | --- | --- | --- |
| Desktop OpenGL | Required | Required | Occasional edge case |
| OpenGL ES 2.0 | Required | Required | Occasional edge case |

Use the same candidate build for the paired sizes. For every executed case,
retain GPU identity, submission/completion/retirement, correctness readback,
buffer integrity and cleanup results. A resource-only allocation test cannot
fill any rendering cell in this matrix.

## Reuse the physical results

The restricted gears path establishes real GPU execution, vertex processing,
lighting, depth, retirement and a read-only GEM-FD presentation contract. Its
fixed geometry/shader/profile admission is a regression fixture, not the
normal submission API for arbitrary applications. Preserve the physical
artifacts and previously qualified encoders; do not replace GPU transforms
with the legacy CPU transform/clip path.

The latest constant-normal upper-clamp proof used the exact 336-byte shader
already present in the gears profile and produced full COLOR 51/255/51 with
matching Z24 over three retired operations. These local records remain at:

`/home/gama/sgx535-offline/gears-state-upper-qualification-20261027T025000Z/PHYSICAL_RESULTS.json`

That component result does not qualify a general shader compiler or a complete
OpenGL implementation. The bounded 600-frame result published earlier also
remains valid; subsequent window-resize refusals are not general window support.

## Concrete missing contracts

An independent [original SDK USSE oracle](USSE-VENDOR-ORACLE-20261027.md) now
decodes retained instructions and 17 existing generated-encoder cases (122
instructions) using the SDK's SGX535/rev121 descriptor. This is offline compiler
evidence only; it changes no rendering admission or physical qualification and
does not resolve the separate PDS event CODE84 contract.

The [unbound private-PD auditor](PRIVATE-MMU-AUDIT-20261027.md) now passes
native/UBSan/i386 CPU-model tests and compiles in the original antiX MMU
translation unit. It is not wired into the selected module and provides no
context binding, submission permission or physical isolation qualification.
An [unbound-PD owner](PRIVATE-MMU-OWNER-20261027.md) now copies the page bindings,
retains backing pages and rolls back failed construction before releasing them;
its CPU fault tests and real kernel-object build likewise grant no execution
permission and change no selected runtime module.

| Layer | Current evidence | Required next behavior |
| --- | --- | --- |
| Shader IO and compilation | Typed encoders and restricted NIR profiles; old semantic pipeline links position + constant FS only | Common desktop/GLES linkage, then broader NIR operations and IO/resource allocation |
| Geometry and resources | Raw pre-VS snapshots and owned dynamic scene composition exist | Application-supplied VB/IB, dimensions and state reach the GPU through owned allocations |
| Physical submission | FAST fixes eleven BO roles and a twelve-draw scene; only bounded parameter updates are mutable | Typed dynamic scene submission with range, permission, PFN, isolation and lifetime validation |
| Completion | Physical receipts, retirement, cache ownership and read leases are demonstrated | Reusable BO/fence ownership for application draws, including failure/quarantine |
| Textures and render state | CPU contracts do not establish physical sampling or arbitrary blend/raster behavior | Physically qualify sampler/texture, blend, clipping and preservation features before advertising them |
| API entry | The DRI screen deliberately rejects general OpenGL admission | Both desktop GL and GLES 2.0 use the same qualified Gallium backend, initially offscreen |
| Installation | Mesa loader/EGL overlays exist; current qualified runtime is private | Reproducible i686 build and isolated package with an explicit matching DRM dependency and uninstall/rollback |
| 2D and X11 | X11 presentation currently copies completed GPU pixels | GPU 2D drawing/blits and later window-system integration; copying pixels to X11 is not desktop acceleration |

The current user priority is the driver/2D path rather than further
disassembler searches. [Native2D copy preparation](TWOD-DRIVER-COPY-20261027.md)
now matches the original kernel FIFO packets and is included in future Mesa
overlays. The typed kernel endpoint, hardware completion/readback qualification
and Gallium acceleration dispatch remain pending; CPU packet tests are not a
new2D hardware-acceleration claim.
The [retained GEM preflight](TWOD-GEM-PREFLIGHT-20261027.md) now authenticates
that request against held kernel allocations and reconstructs the shared packet.
The [separate 2D GEM lifecycle](TWOD-JOB-LIFECYCLE-20261027.md) now implements
issue, owned-event receipt validation, visibility and retirement around trusted
native adapter callbacks, with uncertainty holds and buffer reuse. CPU tests
and real kernel-object compilation pass; no native MMIO adapter is installed.
Native2D FIFO emission, pre-ACK IRQ capture and physical validation
remain pending; the existing3D lifecycle mask is unchanged.

## First common implementation increment

`remesa_shader_linkage_prepare()` owns validated VS/FS semantic snapshots and
checks POSITION/COLOR0 producer-consumer compatibility. It distinguishes
constant, smooth and flat fragment input; an absent COLOR0 producer rejects a
varying FS. Revalidation compares complete canonical programs and derived IO,
not a digest. The old pipeline and all execution guards retain their behavior.

This interface does not yet support arbitrary GLSL varyings, NIR instructions,
shader BO allocation or physical draw submission. In particular, accepting a
semantic link is not GPU execution permission. Flat raster/provoking state
still needs its separate draw contract. The next integration must carry this
common linkage into program/resource preparation instead of selecting a
historical shader or the gears-specific callback.

With the project's pinned Mesa cache already prepared, the focused check is:

```sh
python3 mesa/sgx535/tests/qualify_shader_ir.py \
  --test linkage --cache "$MESA_CACHE" --output /tmp/remesa-linkage
```

It exercises native, UBSan and i386 builds plus actual Mesa NIR builder
producer/consumer graphs. It creates no GL/EGL context and performs no GPU
submission; it is an offline prerequisite, not the physical driver result.

## Installation and qualification direction

Keep shader compilation, scene construction and resource policy in userspace.
The matching minimal DRM/GEM/MMU, memory isolation and synchronization support
remain kernel dependencies; this work cannot turn the stock unextended gma500
kernel into a general SGX driver through a library name alone.

Build the Mesa DRI/EGL driver in a separate prefix first. Do not replace the
system libGL, alter the default boot, install the old private gears launcher or
advertise capabilities before their physical qualification. Exercise desktop
OpenGL and GLES contexts independently through the same backend; a successful
CPU fixture, context creation, software fallback or X11 image is not physical
GPU validation. Initial offscreen tests must prove submit, retirement, expected
readback and released backing on the SGX535 before packaging is promoted.

## Linked program CODE ownership

`remesa_persistent_linked_program_prepare()` now accepts the common semantic
link, generates CODE through the existing selected-resource compiler, and
publishes an immutable PROGRAM-domain GEM allocation through the existing
owner/VA/lease machinery. Its metadata snapshot performs no ioctl or buffer
comparison. Destruction still defers while a CPU lease survives; uncertain
provider effects retain quarantine and prevent forced cleanup.

For the currently admitted producer, VS CODE is 64 effective bytes in 144
reserved bytes. The constant FS has 8 effective/reserved bytes; the varying
COLOR0 FS has 8 effective bytes in 16 reserved bytes. The second entry begins
at byte 160 after alignment. Reserved bytes and padding are zero, not extra
instructions. These sizes do not claim how many bytes an untested GPU launch
would fetch.

The focused owner regression includes the older persistent-shader regression:

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py \
  --source-only --test test_linked_program_owner.c \
  --cache "$MESA_CACHE" --output /tmp/remesa-linked-owner
```

Native, UBSan and i386 source tests passed, as did a private archive variant
using the actual Mesa compilation model. The original Mesa cache/archive and
its referenced objects were retained unchanged. The resource probe is
`tools/remesa/linked_program_resource_probe.c`; it is a separate qualification
tool, not an installed renderer or launcher.

On the physical Mini 12, the exact probe completed 84 memory ioctls, full CODE
readback, PTE ownership checks, two final empty GEM INFO results and context
close. A varying program remained allocated while COLOR/Z storage was created
and destroyed for all three selected sizes:

| Size | COLOR stride | COLOR allocation | Z allocation |
| --- | --- | --- | --- |
| 1280×800 | 5120 B | 4,096,000 B | 4,096,000 B |
| 640×640 | 2560 B | 1,638,400 B | 1,638,400 B |
| 641×479 | 2688 B | 1,290,240 B | 1,290,240 B |

This was **resource-only**: zero GPU submissions and zero GL/EGL contexts.
It does not approve rendering at these dimensions or either API. The 400
program metadata snapshots generated no additional ioctls. Independent cleanup
confirmed the client had exited, N/N/N controls, quiet source state, stock next
boot, unchanged legacy capsule and no new kernel warnings.

Local evidence is retained under
`/home/gama/sgx535-offline/general-userspace-driver-20261027T040000Z/`:
`LINKED_CODE_PHYSICAL_RESULTS.json`, `RESOURCE_TRANSCRIPT_RESULTS.json`,
`hardware/resource-ended/FINAL_AUDIT.json` and
`hardware/RESOURCE_EVIDENCE_MANIFEST.json`. The transcript auditor and nine
negative tamper cases are in `tools/remesa/verify_linked_program_resources.py`
and `mesa/sgx535/tests/test_linked_resource_transcript.py`. Their scope is
memory ownership/upload and cleanup, not instruction execution or conformance.

Next, derive PDS and raw geometry bindings from these live owners, retain the
complete fetched allocation set until retirement, and connect typed dynamic
submission. Do not lift the fixed gears-profile guard, activate the historical
CPU transform route, or advertise general GL/GLES capability to skip that work.

## Raw geometry and dependent PDS storage increment

The next storage implementation now copies application vertex records verbatim
for the existing three-float4 profile, checks triangle-list u16 index bounds,
and derives PDS relocations from live CODE and geometry owners. PDS ownership
retains both dependencies, including when callers release their original
references. CPU read leases defer final destruction. No CPU transformation,
lighting or viewport calculation is introduced.

The VS PDS image has 64 effective bytes in 192 reserved bytes; the FS image has
60 effective bytes in 64 reserved bytes. Padding remains zero. No CODE
instructions are added to fill capacity, and these preparation sizes do not
establish physical GPU consumption. The existing selected WHITE_LAMBERT3/COLOR0
pair restriction and native execution guards remain in place.

`mesa/sgx535/tests/test_linked_geometry_pds_owner.c` passed native, UBSan and
i386 source builds, including the inherited persistent/linkage regressions,
unchanged vertex bytes, bounded indices, encoded PDS bytes, zero padding,
metadata with no provider IO, dependency lifetime and ordinary failure rollback.
The expanded lifetime/failure regression also covers two simultaneous PDS
closures, foreign-owner rejection and retention of all three allocations after
uncertain map/unmap or failed unpin. Results are at
`/home/gama/sgx535-offline/general-userspace-driver-20261027T040000Z/geometry-pds-owner-v2/result.json`.
The older CODE owner regression also passed all three source variants under
`geometry-pds-preregression-v1/result.json` in the same artifact directory.

An isolated private archive variant passed the i386 test using the actual Mesa
compile model (`private-mesa-geometry-pds/RESULT.json` in the same directory).
This is not a complete rebuilt DRI driver and has not been tested physically.
The earlier physical resource evidence belongs to the earlier CODE-only build.

`remesa_persistent_linked_index_encode()` now feeds the existing compiled
POSITION4+COLOR0 TA serializer from that live PDS closure. It derives both GPU
references and descriptor fields from owned geometry/program metadata. Draw
subranges must fit the immutable index payload, rather than spare allocation
capacity. It emits exactly 20 bytes, without IO, transforms or execution
permission; the caller must retain the PDS owner while building the scene.
Its descriptor tail matches the retained whole-original stride12/output32
oracle in `lambert-vs-minimal-20261023T221500Z/TA12_ORACLES.json` under the
offline artifact root. That existing shader profile also has separate retained
physical light-direction evidence in
`native1280-white-lambert-depth-bif-candidate-20261023T223200Z/hardware/LIGHT_DIFFERENTIAL_CONFIRMATION.json`;
it does not validate these new allocations or commands on the GPU.

The combined owner/index regression passed native, UBSan, i386 and the actual
Mesa compilation model. Evidence is `geometry-pds-index-owner-v1/result.json`
and `private-mesa-geometry-pds-index/RESULT.json` in the current artifact
directory. The latter independently rechecked the original Mesa objects and
the candidate/fixture source identities. Previous artifacts remain unchanged.

The common link now reads Gallium's bound shader objects, and owned draw state
and the current three-input attribute transport are described below. Next,
assemble initial bounds/clear/draw/close state and connect typed
dynamic submission with complete job holds and fences. The current
physical stream ABI still admits only the fixed eleven-role, twelve-draw gears
scene. Neither this index encoder nor the private archive lifts that admission
guard or establishes desktop/GLES rendering.

## Bound Gallium shader linkage

`remesa_context_shader_linkage_snapshot()` copies the actually bound,
context-owned VS/FS into the common link. It accepts the existing constant,
smooth COLOR0 and flat COLOR0 semantic forms without passing through the old
constant-fragment draw ABI. It rejects missing stages, foreign handles,
overlapping output and unsynchronized uniform specialization; it does not
erase a prior compile error, change context state or enable a draw route.

The expanded actual Mesa NIR builder/Gallium callback test exercises smooth and
flat links, output aliases, foreign contexts, failed replacement and snapshot
lifetime after shader/context destruction. The old pipeline still rejects the
varying pair. It passed with the real Mesa compilation model in
`private-mesa-bound-linkage-v2/RESULT.json` under the current artifact directory.
The no-NIR Gallium regression also passed native/UBSan/i386 in
`bound-linkage-no-nir-regression-v2/result.json`. Its first build exposed an
older NIR-only `gears_reject` helper compiled outside its conditional; the
helper is now conditional too. The original failed build log is preserved.

## Owned linked draw state

The PDS owner now includes the existing empty secondary constructor: 4 CODE
bytes at offset 256 in a 32-byte reservation. The primary VS/FS offsets and
effective/reserved sizes stay unchanged. The complete storage reservation is
288 bytes; the three effective images total 128 bytes. Their remaining space
is zero, without artificial instructions. This does not measure GPU fetching.

`remesa_persistent_linked_state_prepare()` derives fragment references from
that owner and composes the existing typed ISP, fragment, viewport and scalar
encoders into 56 bytes of draw DATA. It reuses the existing per-count state
auxiliary USE/PDS and TA-reference preparation: USE16, PDS60 and TA8, with
explicit aligned offsets and zero gaps. State bank conflicts are checked
against both shader entries. The linked PDS, CODE and geometry remain retained
through state leases and post-binding uncertain failures; no forced cleanup is
added. The old constant/clear/close interfaces keep their separate admission.

This state is a draw delta. It does not supply initial scene bounds, attachment
clears, scene close, native submission or retirement. MATRIX_WINDOW encoding
is still a CPU contract here. Shader selection is restricted to the already
selected WHITE_LAMBERT3/COLOR0 pair, and no additional capability is advertised.

`mesa/sgx535/tests/test_linked_draw_state.c` passed native/UBSan/i386 and the
actual Mesa compilation model. It compares the initial-zero F41 DATA, USE14
auxiliary and TA-reference descriptor against the retained WHITE64 witness,
relocating only the two fragment references. It also covers matrix-window
serialization at all three chosen sizes, zero padding, read-only publication,
100 metadata snapshots without IO, transitive dependency/lease lifetime,
ordinary rollback and uncertain quarantine. These size checks are state
preparation, **not rendering or API contexts**.

Results are `linked-draw-state-v1/result.json` and
`private-mesa-linked-state/RESULT.json` under
`/home/gama/sgx535-offline/general-userspace-driver-20261027T040000Z/`.
`linked-state-adapter-regressions/RESULT.json` additionally passes the older
geometry/PDS/index regression and actual bound-NIR callbacks against this same
new archive. Original Mesa objects and prior physical artifacts remain intact;
none of these tests submit to the GPU.

## Application attribute transport

`remesa_context_linked_vertex_inputs_snapshot()` now reads the application
resources bound through actual Gallium callbacks for the current three-float4
WHITE_LAMBERT3 link. The internal binding storage accepts a third VB/element;
the advertised caps and old draw admission remain unchanged. The existing
gears path still requires its exact one/two-element layout, and the old general
draw route still rejects expanded layouts.

The snapshot synchronizes source writers before any reads and maps each
distinct VB/IB once, including when all three attributes and indices share
one BO. Interleaved, separate and zero-stride sources are supported. It releases
all acquired read maps on success, allocation failure or partial map failure.
An active frontend map, malformed user-index range or output alias rejects
without reading those indices. Imported writer timeout remains an error;
tests exercise the later simulated retirement separately.

`remesa_linked_vertex_inputs_create()` performs byte transport only: it copies
the effective min..max source records once and rebases u8/u16/u32 indices into
a host u16 list. It preserves repeated references, primitive/provoking order,
all float component bits and source independence. Indexed bounds are checked
before bias and addresses after bias. Nonindexed draws ignore index hints and
bias, and incomplete triangle tails are ignored. There is no vertex shader,
normalization, lighting, clipping, viewport computation or CPU rasterization.

The current preparation policy bounds the source span to 4095 records and the
index list to 65535 entries independently. Only exact float4 attributes for
this selected shader layout are admitted; generic formats, instancing, restart
and broader shader IO remain work for later compiler/transport integration.
Successful snapshots feed the existing immutable GEM geometry, PDS and TA
index owners without treating spare allocation capacity as consumed data.

`test_linked_vertex_inputs.c` passed native/UBSan/i386, including NaN/negative
zero bit preservation, strides/broadcasts, sparse/repeated indices, bias,
overflow/aliases, high source IDs with zero stride, the independent index limit
and the actual GEM/PDS/TA owner code with fake GEM callbacks. Actual Mesa NIR
builder/Gallium tests additionally passed for three distinct VB sources, shared
VB/IB, source updates, OOM, failed/null/partial maps, active-map rejection,
writer timeout followed by simulated retirement, complete reference cleanup
and continued refusal by the old draw route.

Evidence under the current artifact directory is
`linked-vertex-inputs-v1/result.json`,
`private-mesa-linked-vertex-v2/RESULT.json` and
`linked-vertex-regressions/RESULT.json`. The last also runs the original gears
geometry regression, linked draw-state owner and bound smooth/flat NIR linkage
against the same new archive. The strict no-NIR Gallium regression passed in
`linked-vertex-no-nir-regression/result.json` before the final NIR-only early
user-index alias guard. All test providers are simulated: zero GPU submissions,
zero GL/EGL API contexts and no new physical rendering approval.

## Ordered dynamic TA draw body

`remesa_persistent_linked_body_prepare()` assembles owned draw-state references
and indexed commands in application order, then emits the existing owned
scene-close command. Each draw contributes exactly 28 bytes (state reference8
and index command20), followed by close12. One draw therefore commits 40 bytes;
sixteen commit 460 bytes, in a page allocation. The unused space stays zero.
The bound of sixteen draws is a preparation policy, not a GPU capability.

The constructor takes live state/close owners and index subranges, rather than
caller addresses or raw command words. It checks framebuffer dimensions and
all shader/state/close bank register values across the body before allocation.
It deduplicates dependency holds, supports multiple bodies sharing states,
and retains the full state→PDS→CODE/geometry chain through CPU read leases and
uncertain upload/disposal failures. A prior release of the public CODE/PDS/
geometry references does not invalidate the state owner's retained closure.
Writable mappings, legacy color submission and render-target reinterpretation
remain rejected for this new command object.

This is a **draw body**, not a complete executable scene: initial bounds/clear
commands, attachments, auxiliary raster programs and native submission are
still absent. Appending the original close command does not prove completion
or authorize a GPU launch. No fixed-profile kernel guard or capability flag
has changed.

`mesa/sgx535/tests/test_linked_body.c` passed native/UBSan/i386 and the actual
Mesa compilation model. It covers all three chosen dimensions as storage
tests, repeated and distinct states, exact ordering and command sizes,
deduplicated/shared owner lifetime, zero padding, metadata without IO,
dimension/bank conflict rejection before allocation, index limits, ordinary
rollback and uncertain retention. The first fixture failed compilation because
of a nested `main` macro; its original log remains preserved in `linked-body-v1`.
The independent corrected fixture results are `linked-body-v2/result.json` and
`private-mesa-linked-body/RESULT.json` in the current artifact directory.

## Owned full-target Z-clear prefix

`remesa_ta_zclear_single_geometry_encode()` now exposes the same covering
triangle used by the retained physical full-frame depth-clear correction:
`(0,0), (2W,0), (0,2H)`. It preserves the fourth reserved vertex and six-index
storage, while the command references three vertices and three indices only.
The original quad encoder is unchanged. At 1280×800 the new 64 vertex bytes
match the retained `typed-single-clear-native-verified.bin` artifact exactly.

`remesa_persistent_zclear_prefix_prepare()` owns the clear draw in one PROGRAM
allocation and generates all CODE through existing encoders: POSITION VS16,
green FS8, state auxiliary USE16, primary vertex PDS64, fragment PDS56, empty
secondary PDS4 and state-upload PDS60. It generates the existing special54c5
state DATA44 and TA28, resolves every address from the allocator and checks
bank compatibility. Logical storage ends at byte444 in a page allocation.
VBO capacity is64 bytes with48 referenced; index capacity is12 with6 referenced.
Remaining alignment gaps and page tail are zero, without padding instructions.

This is specifically a Z-clear draw. It does not clear COLOR, own attachments,
construct raster/background/EOT programs or establish all initial scene state.
The new relocated object is not physically qualified by the older triangle
result. It must be integrated with the dynamic draw body, complete scene
dependencies and a typed native submission before testing either API.

`test_zclear_prefix.c` passed native/UBSan/i386 and the actual Mesa compilation
model, including all pixel-centre coverage at the paired and odd sizes,
generated source references, exact special state/TA descriptor fields, zero
padding, immutable publication, metadata without IO, deferred read-lease
destruction, ordinary rollback and uncertain retention. It also runs the
5506-check original persistent-shader fixture. Providers are simulated and
perform no GPU operation.

Evidence in the current artifact directory is `zclear-prefix-v1/result.json`,
`private-mesa-zclear-prefix/RESULT.json` and
`zclear-single-retained-oracle/RESULT.json`. The last is a fresh exact comparison
against the previously qualified candidate's geometry, with source/reference
identities retained. These are prerequisites, not API rendering approval.

## Combined TA owner and referenced read allocations

`remesa_persistent_linked_ta_prepare()` now joins the generated Z-clear28 and
the ordered draw body with its close12. A one-draw stream commits68 bytes;
the current sixteen-draw bound fits488 bytes, separately from page capacity.
The constructor regenerates the clear references from its live owner and
copies the body's immutable command snapshot, without remapping/readback or
re-encoding shaders. All cross-component dimensions and bank values must agree
before allocation. No arbitrary command/VA input is accepted.

The combined allocation retains the prefix and body; their existing owner
edges retain states, close, shader CODE, PDS and geometry. Shared combined
streams survive release of all caller references until their last read lease
and owner are released. Uncertain upload or unpin retains the full closure.
The body keeps its original command backing for lifecycle, but the new GPU
stream references its copied bytes rather than that old command allocation.

`remesa_persistent_linked_ta_snapshot()` enumerates unique currently live READ
allocations reachable from the commands. The one-draw test has seven: combined
TA, Z-clear, close, draw state, PDS, CODE and geometry. It excludes the original
body command allocation. Each entry carries handle, generation, VA, read access
and allocation capacity. These are bounds for future job validation, not a
measurement that the GPU consumes every byte. Snapshotting performs no IO,
hashing or whole-buffer comparison and grants no kernel job hold or permission.

Attachments and raster/background/EOT/DPM allocations are still outside this
list; the eventual complete job must add their READ/WRITE uses and retain all
of them through authenticated retirement and release. A read list alone is not
a render submission. Neither rendering API has been enabled by this increment.

`test_linked_ta.c` passed native/UBSan/i386 and the actual Mesa compilation
model, including exact clear/body/close order, deduplicated references,
dimension/bank/type rejection, source-owner release, shared-stream/lease
lifetime, no-IO metadata, ordinary rollback and uncertain closure retention.
Its three size cases are CPU storage tests, not rendering. Evidence is
`linked-ta-v1/result.json` and `private-mesa-linked-ta/RESULT.json` in the
current artifact directory. Earlier private archive variants remain intact.

## COLOR/Z ownership around the command stream

`remesa_persistent_linked_targets_prepare()` now binds a combined TA owner to
normal linear BGRA8 backing and matching typed cpp4 Z backing. It checks the
actual logical COLOR byte extent, padded Z dimensions, pitch, current mapping
state and resource kind. Source GPU addresses are allocator-derived. It creates
an immutable72-byte control object: target DATA16 plus existing TA control56.
The generated control points at the effective command span rather than its
page capacity. This is command DATA, not direct register access or submission.

The binding retains TA, COLOR and Z, including after caller ownership is
released. It supports multiple bindings sharing the same attachments, defers
final destruction through read leases and preserves all dependencies after
uncertain upload/disposal. Its snapshot revalidates the complete TA read set
and attachment mappings, and reports separate conservative READ|WRITE access
for COLOR and Z. The binding object is rejected by writable mappings, legacy
color submission and render-target reinterpretation.

Attachment storage readiness is deliberately distinct from shader publication:
fresh undefined attachments can be bound without marking them CPU-readable or
writing their contents. This does not initialize them or implement a load,
clear or preservation contract. A later executable job must explicitly ensure
valid attachment contents where read access is required. The snapshot does not
grant fences, retirement, MMU visibility or physical BGRA/Z format approval.

The eventual job still needs raster/background/EOT/DPM resources and control,
with all READ/WRITE allocations retained and checked together. These bindings
are not the physical submission and do not approve any rendering matrix cell.

`test_linked_targets.c` passed native/UBSan/i386 and the actual Mesa compilation
model. It checks all three sizes, exact DATA16/control56, logical-size limits,
pitch, dimensions, undefined contents staying unreadable, metadata without IO,
shared attachment lifetime, deferred leases, ordinary rollback and uncertain
retention. The original fake provider's1MiB limit rejected the first run before
the full-size test could proceed. The corrected fixture supplies larger fake
attachment storage while retaining the original failure callbacks for the new
4KiB control object. Its first failure log remains in `linked-targets-v1`.

Evidence is `linked-targets-v2/result.json` and
`private-mesa-linked-targets/RESULT.json` in the current artifact directory.
`linked-targets-regressions/RESULT.json` additionally passes eight earlier
TA/clear/body/attribute/geometry/state/NIR tests against the same new archive.
The original Mesa archive and object members remain unchanged. These tests
use fake resource providers and simulated fences; there are no GPU submissions
or GL/EGL API contexts in this qualification.

## Owned background object and program

`remesa_persistent_linked_background_prepare()` now generates the existing
background constructor from the current target binding: auxiliary USE8,
primary PDS64 and empty secondary4 occupy100 logical bytes in a PROGRAM
allocation; the background object68 is in a separate RASTER allocation.
All addresses come from these owners and the bound COLOR allocation. The
pixel-bank word must agree with the complete TA shader/state closure.
The object uses exact logical target dimensions and depth1. It does not use
the earlier rejected oversized-background workaround or change geometry.

CODE is generated with the existing typed encoders and operands from the
original background producer. No historical CODE blob is transplanted or
extended with artificial instructions. Unwritten DATA cells retain their
conservative known-mask policy; alignment gaps and page tails are zero.
Program8, PDS64 and secondary4 sizes remain distinct from the100-byte logical
span and page capacity. This does not measure physical fetching or establish
general texture sampling.

The object retains its program and target binding, including after public
target/TA/COLOR/Z references are released. The program independently retains
the target binding as well: if parent disposal succeeds but the program's
unpin is uncertain, it still owns every COLOR/Z/TA dependency it might read.
Ordinary upload failure unwinds both tentative allocations; uncertainty retains
the closure and blocks close. Metadata snapshots perform no buffer IO.

Preparing background does not initialize COLOR, clear it, generate event/EOT
CODE or submit. Undefined COLOR stays undefined and unreadable. An executable
job must supply a valid initialization/load contract, complete event/raster/DPM
resources and full permissions/lifetime/fence handling before GPU admission.

`test_linked_background.c` passed native/UBSan/i386 and the actual Mesa
compilation model. It checks exact logical dimensions at all three selected
sizes, generated USE/PDS/object bytes, known masks, bank/domain admission,
shared/transitive read-lease lifetime, ordinary/uncertain uploads and program
unpin failure after parent disposal, including rollback. Tests explicitly
retain the entire targets/TA/COLOR/Z chain when that child program survives.

At1280×800, the generated USE8 matches the retained physical artifact exactly;
PDS DATA1..11 and CODE match the retained primary at role0+4560, with DATA0
independently relocated, and the24 geometry bytes match role3+44 exactly.
The fixture literals were separately checked against those retained files in
`BACKGROUND_RETAINED_REFERENCES.json`. This establishes byte correspondence;
the new allocation layout, relocated program and API rendering are untested
on the GPU.

Evidence in the current artifact directory is `linked-background-v2/result.json`,
`private-mesa-linked-background/RESULT.json` and
`linked-background-regressions/RESULT.json`. The last passes nine earlier
target/TA/clear/body/attribute/geometry/state/NIR regressions against this same
new archive. Original Mesa objects and protected physical artifacts remain
unchanged. These qualifications use fake resource providers and simulated
fences, with zero GPU submissions and zero GL/EGL API contexts.

## Event/EOT: identify the exact producer before ownership

The retained physical scene uses the psb_dri392ab event, with64 bytes DATA
followed by84 bytes CODE,148 committed bytes total. Its7 recorded relocation
contributions affect only three DATA words: end-task USE at offset0, target
DATA address at offset8, and transfer USE at offset16. The referenced auxiliary
programs occupy8 and16 bytes respectively. This is distinct from the Xpsb7eb0
DATA profile already supported by `remesa_pds_structure_encode_event_data()`.
Substituting that encoder would put references and constants in wrong words.

`remesa_pds_structure_encode_psb_event_data()` now provides the separate
original DATA64 constructor. It checks source16-byte extent and both lower19
numeric inputs, emits the original direct stores including DATA8=8, and marks
DATA6/7/15 as unknown/policy-zero.52 bytes are defined:40 direct-store bytes and
12 symbolic-reference bytes. It does not emit the84-byte CODE, add filler or
grant upload/execution permission. The original Xpsb API is unchanged.

A fresh bounded original-i386 replay of39537..39661 passed three symbolic
cases, with the target binary digest checked. All three relocation calls are
explicit mocked ABI cuts; the result proves direct DATA stores, not original
relocation execution or the whole constructor. A separate audit matches the
retained CODE84 to the earlier two-base original CPU replay, and records the
actual wire relocations and auxiliary spans. Retained program bytes remain
comparison oracles, consistent with the README's original-implementation
policy; no proprietary event program is embedded into this driver.

The focused test passes native/UBSan/i386,635 checks per variant, comparing
direct-store oracles, symbolic references, the retained physical DATA64,
unwritten/policy bytes, separate Xpsb layout, and transactional range/alias
failures. It also passes using the actual Mesa compile model. Ten earlier
owner/Gallium/NIR regressions pass against that same new archive. The first
test harness had a missing relative-path fixture, then one wrong expected
errno for an unaligned maximal offset; both failures are retained. The encoder
checks were not relaxed.

Evidence under the current artifact directory is:

- `psb-event-contract/ORIGINAL_DATA_RESULTS.json` and `REPLAY_DATA.py`;
- `psb-event-contract/RETAINED_EVENT_AUDIT.json`;
- `psb-event-contract/qualified-v3/RESULT.json` and `standard-runner/result.json`;
- `private-mesa-psb-event-data/RESULT.json` and `psb-event-data-regressions/RESULT.json`.

The standard focused command accepts the recorded three-row oracle input:

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py --source-only \
  --test test_psb_event_data.c --cache "$MESA_CACHE" --output /tmp/remesa-psb-event \
  < "$EVENT_EVIDENCE/original-data-oracles.txt"
```

Concrete remaining event work is the original complete CODE84, then owned
relocation/closure and native qualification. The auxiliary CPU codecs below
do not qualify a complete event or its execution.
The existing event forward-index encoder covers only the two demonstrated
position-relative words; it does not complete the other instruction classes.
Do not import the old binary as an independently implemented event, infer
unproved operands, or enable the native gate because DATA64 matches. DPM,
complete raster/submission and API rendering remain unqualified.

## Original event USE auxiliaries

`remesa_usse_psb_event_end_encode()` emits the eight effective bytes produced
by the original zeroing slot allocator (`2fabd`), writer (`30422`) and separate
finalizer (`30fb7`). The finalizer sets word1 bit18; no broader architectural
meaning is assigned to that bit. This producer differs from selector EE, whose
encoder remains unchanged.

`remesa_usse_psb_event_transfer_encode()` implements the CPU field arithmetic
of original PSB `391e0`. Only modes 0, 1 and 3 from caller `392ab` are accepted
(surface tags other, 2 and 1 respectively). In particular BGRA tag4 uses mode0.
The assembly derives a word0 field from `(t << 22) & 0x0fc00000`, with
`t=(mode << 6)|0x204`; the decompiled constant does not describe that general
arithmetic. The admitted modes currently produce the same value in that field.
No additional modes, operand meanings or physical bank permissions are inferred.

Both implementations build little-endian numeric fields, with no executable
blob imported into the product. Capacity is separate from the 8/16 effective
bytes: excess output remains untouched and is not additional CODE or proven
GPU-consumed data. Unsupported modes, insufficient capacity and invalid or
overlapping output/count ranges fail without mutation.

Evidence is under the current artifact directory, `event-use-auxiliaries/`:
`REPLAY.py`, `RESULT.json`, `oracles.txt`, `end-producer.asm`,
`transfer-producer.asm`, and `qualified/result.json`. The replay executes the
original writer/finalizer/commit and transfer constructor/commit with explicitly
mocked CPU allocation or supplied builder-init fields. It admits bounded PCs,
checks committed sizes and surrounding sentinels, and matches the retained
physical role1 spans at offsets 0x100 (end8) and 0xe0 (mode0 transfer16).
This is controlled CPU replay plus historical correspondence, not a new GPU
test. Native, UBSan and i386 tests consume the four original records on stdin.
Each variant passes 653 checks. A private seven-object variant built with the
actual Mesa compilation model also passes the auxiliary test, followed by ten
existing owner/Gallium/NIR regressions. Receipts are
`private-mesa-event-auxiliaries/RESULT.json` and
`event-auxiliaries-regressions/RESULT.json`. The original Mesa thin archive and
every referenced original object remain unchanged; this is not an installable
DRI build or a physical rendering result.

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py --source-only \
  --test test_psb_event_auxiliaries.c --cache "$MESA_CACHE" \
  --output /tmp/remesa-event-auxiliaries < "$EVENT_USE_EVIDENCE/oracles.txt"
```

No native submission gate, GL/GLES capability, geometry/shader profile or
physical boot configuration is changed by these auxiliary encoders. The next
event blocker is independently generating its complete CODE84.

## Owned event auxiliary resource

`remesa_persistent_linked_event_auxiliary_prepare()` now places the generated
transfer16 and end8 programs in one PROGRAM allocation, at offsets 0 and 32.
Used extent is40 bytes, including16 zero reserved bytes between entries; the
rounded4096-byte allocation is neither effective CODE nor a claimed GPU fetch
length. No instruction is added to occupy padding. Only BGRA8/mode0 is selected,
from the existing immutable COLOR/Z/TA target owner.

The resource holds a strong reference to that target owner and its complete
transitive closure. Both entry plans derive from its own live allocation and
must agree with the TA's pixel-bank bindings. Public write mapping and the old
color-submit ABI reject the typed resource. Ordinary preparation failures
unwind; uncertain mapping, synchronization or unpin effects retain quarantine
and the target/attachment/command resources. Existing CPU lease handling delays
final disposal until the lease is released.

`remesa_persistent_linked_event_auxiliary_snapshot()` reports allocation and
effective spans separately, plus the two computed USE fixups and the live
target DATA backing. It performs metadata checks without readback, hashing,
provider ioctls or synchronization. Its fixups can feed the existing DATA64
encoder without accepting caller-supplied executable bytes or raw GPU addresses.
Preparation uses the existing one-time upload/verification/cache-ownership
helpers; no diagnostic work is added to a rendering loop.

The focused resource test consumes24 historical auxiliary bytes on stdin
(role1[0xe0:0xf0] then role1[0x100:0x108]). Its three sizes test storage and
lifetime only, not physical rendering or either graphics API. Two initial
fixture failures are preserved: an unexposed private fake-provider pointer
was incorrectly expected to be recognized as an output alias, and uninitialized
COLOR hits EBUSY before the typed-target check. The corrected fixture tests
owner-metadata alias rejection and preserves the existing live-state gate order.

`owned-event-auxiliary-v3/result.json` records native/UBSan/i386 passes with
19,543 checks in the new ownership fixture, plus its reused persistent and
linkage checks. `private-mesa-owned-event-auxiliary/RESULT.json` records the
same test against a private archive using actual Mesa compilation flags. The
original cached thin archive and its referenced objects are unchanged.
`owned-event-auxiliary-regressions/RESULT.json` records ten passing existing
owner/Gallium/NIR tests against that same archive. These fixtures inject a GEM
provider; none is a physical submission or GL/GLES rendering test.

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py --source-only \
  --test test_linked_event_auxiliary.c --cache "$MESA_CACHE" \
  --output /tmp/remesa-owned-event-auxiliary < "$EVENT_FIELD_EVIDENCE/auxiliary-oracle.bin"
```

The separate field search in `event-code-fields/IMMEDIATE_SCAN_REVIEW.json`
and `FIELD_SEARCH.md` found no new parameterized constructor with its narrow
register-OR-immediate pattern. The additional Windows class17 candidate is jump
table data selected at1010acfc, not executable OR arithmetic. This does not
rule out shift/table-based producers or other modules and does not justify
embedding the old complete CODE84. The event, native admission and API rendering
therefore remain incomplete despite owned auxiliaries.

## Owned DPM load plan with reusable storage

`remesa_persistent_linked_dpm_prepare()` now binds the demonstrated fixed32MiB
table and32MiB parameter profile to live BOs from the same endpoint. It accepts
BO identities, not caller raw addresses or register/value pairs. Existing
original CPU info/load arithmetic derives the13 cookie words and22 load pairs.
The table must lie in40000000..ffffffff and the1MiB-aligned parameter storage
in30000000..3fffffff; capacity, registry generation, pin/map state, ownership
and leases are checked before publication. Broader capacities remain unsupported.

A separate immutable RASTER allocation holds240 bytes of plan DATA, rounded
to one page. Only that small allocation is uploaded, verified and synchronized
once at preparation. The64MiB storage is neither mapped nor cleared by this
operation. No per-frame allocation, full-buffer comparison, hash or readback
is introduced. Snapshots use metadata and original CPU arithmetic only, with
no provider I/O. Multiple plan owners may share storage; final disposal waits
for all references and CPU leases. Uncertain plan map/unmap/sync/unpin effects
retain the table and parameter backing in the existing quarantine mechanism.

The plan and storage are distinct: this resource does not initialize table
contents, execute MMIO, start DPM loads, establish requester permissions or
create native job/fence holds. Those are still required before submission.
Both storage roles are conservatively marked READ|WRITE. Original CPU wait
argument7 remains separate from retained completion mask15; the former cannot
weaken the qualified four-completion requirement. Effective240-byte DATA and
page capacity do not establish a hardware fetch extent or imply that all
reserved64MiB is GPU-consumed.

The new typed plan rejects public write maps, the legacy color-submit ABI and
render-target reinterpretation. Its references are dropped only after confirmed
unmap/unpin/destroy, using the same dependency-disposal path as the other owners.
General event CODE84, COLOR initialization/load, complete raster/native job
admission and GL/GLES rendering are still incomplete.

The focused test consumes the retained original Xpsb CPU oracle text from
`/home/gama/sgx535-offline/depth-dpm-load-20261023/dpm-oracles.txt`. It checks
two relocated original info/load results, shared and lease-delayed disposal,
zero-I/O snapshots, untouched storage sentinels, padding, ordinary rollback
and uncertain quarantine. The first fixture failed only its immediate EOF
expectation on the newline-terminated text input; that log is preserved in
`owned-dpm/`. It was corrected to skip trailing whitespace.
The actual Mesa flags additionally required a prototype for the renamed
fixture entry point; the failed test compile is retained in
`private-mesa-owned-dpm/`. Only the fixture declaration changed. Final source
qualification is `owned-dpm-v3/result.json`: native/UBSan/i386 pass14,101 new
DPM-fixture checks each, plus the reused persistent checks.
`private-mesa-owned-dpm-v2/RESULT.json` records the passing i386 test against
a private archive compiled with actual Mesa flags, preserving the original
cached archive and every referenced object. `owned-dpm-regressions/RESULT.json`
records eleven passing existing owner/Gallium/NIR regressions against that same
archive, including event auxiliaries, background, attachments and TA. All use
injected providers; no physical DPM load, GPU submission or FPS result follows.

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py --source-only \
  --test test_linked_dpm.c --cache "$MESA_CACHE" --output /tmp/remesa-owned-dpm \
  < /home/gama/sgx535-offline/depth-dpm-load-20261023/dpm-oracles.txt
```

The separate `event-other-producers/` scan pins five additional original
EMGD API/service modules and extends the earlier search with MOV-register
class bases. Its Windows candidates10010d80 and101065f8 are jump-table bytes,
selected at10010bbd and1010618b respectively; OGL140d49 emits the already known
E6 record-last arithmetic. No new CF/C7/F7/87/event-lowE0 encoder follows.
These narrow static patterns do not exclude shift/table-driven generators or
other modules and do not justify importing the original complete event blob.

## Target-bound region storage and scene register metadata

`remesa_persistent_linked_region_prepare()` now obtains dimensions from the
live COLOR/Z/TA target owner, uses the existing original option0 scene-layout
encoder, and allocates region storage in the SURFACE address domain. It retains
the target and its full dependency closure through ordinary ownership changes,
CPU leases and uncertain map/unmap/sync/unpin disposal. Public write mapping,
legacy color submission and render-target reinterpretation remain rejected.
No caller raw VA, dimensions or cookie are accepted.

Preparation preserves the existing scene allocator's one-time full zeroing,
verification and synchronization policy. These are storage bytes, not CODE or
proof of generated tile lists. The original requested clear-page extent, logical
required bytes and rounded allocation capacity remain separate metadata. This
does not add per-frame clearing or readback and does not initialize COLOR/Z.

`remesa_persistent_linked_region_snapshot()` reports the live region binding,
READ|WRITE access, target metadata, seven TA register/value pairs and the separate
408 raster pair. In particular21c and408 use base+cookie7, not a fixed1000
offset. Snapshot checks and pair construction do not invoke provider I/O,
hashing, readback or synchronization. Word15 of the historical cookie is still
absent, not invented or submitted as zero.

The independent original CPU replay is
`linked-region-contract/REPLAY.py` and `RESULT.json`. It reuses the pinned Xpsb
loader, executes the revision-selector fixture and whole scene-info producer,
then the bounded fresh TA prefix. A separate raster slice41f0..4202 receives
explicit reaching register/frame values and writes408 only in private RAM.
Neither the revision fixture nor those RAM writes are physical GPU evidence.
At1280x800 the original region offset is10000;640x640 and641x479 use4000.
The three original cookies0..14, allocation/clear extents and TA/raster values
are supplied on stdin to the owner test, with region VAs matching live BOs.

`linked-region-qualified/result.json` records native/UBSan/i386 passes. The
combined fixture reports7,430 checks, including its reused persistent fixture;
it also runs the existing common linkage fixture. It checks all three storage
sizes, exact original CPU results, untouched COLOR/Z, zero-I/O repeated snapshots,
typed/alias/alignment refusal, shared references, lease-delayed disposal,
ordinary rollback and uncertain retention of target/attachment/command owners.
No API rendering or physical result is implied by those storage dimensions.
`private-mesa-linked-region/RESULT.json` records the same fixture passing i386
against a private archive built with actual Mesa flags, with the original
cached archive and every referenced object unchanged. Twelve existing
owner/Gallium/NIR regressions pass against that archive, including DPM and
event auxiliaries, in `linked-region-regressions/RESULT.json`.

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py --source-only \
  --test test_linked_region.c --cache "$MESA_CACHE" \
  --output /tmp/remesa-linked-region < "$REGION_EVIDENCE/oracles.txt"
```

The fresh normal scene action planner explicitly does not consume an external
XHW descriptor or unwritten cookie15. The old frozen XHW bind/fire wire is a
different, deliberately unqueued CPU request contract. Allocating or copying an
XHW page would not establish general context ownership, MMU admission or GPU
execution. Full event CODE84, native context/job ownership, complete raster and
COLOR initialization/load remain required; all execution gates stay closed.

## Coherent scene GPU-reference footprint

`remesa_persistent_scene_footprint()` integrates the existing background,
event-auxiliary, region and DPM owners without allocating another BO or host
object. It borrows four live frontend owners under external serialization;
it does not create job/fence holds. Background, auxiliary and region must
reference the identical COLOR/Z/TA target owner. The endpoint, registry
generation, pin/map/lease state, dimensions, USE bank bindings, capacities and
non-overlapping VA ranges are validated from the actual retained objects.
Callers cannot supply raw addresses, access flags or executable bytes.

The result is a deduplicated conservative allocation-level GPU-reference set
with role and READ versus READ|WRITE masks. Logical bytes and rounded capacity
remain separate; neither is claimed as measured GPU consumption. The current
single-draw fixture has16 unique references across10 roles:11 READ allocations
and5 READ|WRITE allocations. Two draws sharing state/program/geometry still
have16 unique references while draw_count becomes2. The old copied body BO
and CPU-only DPM plan BO remain resident through their owners but are not
misreported as GPU references. The partial event's complete PDS allocation is
still missing; the result therefore cannot be a complete job footprint.

Successful queries always retain five explicit unmet requirements: complete
event PDS, COLOR load/initialization, DPM initialization, full raster control
and native admission. Undefined attachments and DPM storage remain undefined.
No PFN/requester validation, MMIO, submission, retirement or GL/GLES capability
is granted by this metadata result. The native path still needs authenticated
kernel validation and actual job holds before any execution.

The query performs no allocation, provider call, hash, readback or buffer
comparison. Its validation and result are transactional. The focused test uses
the three requested storage dimensions, exact permissions/roles, same-target
and duplicate-role refusal, foreign endpoints, active CPU leases, stale owners,
alignment/output aliases, reference deduplication and preserved incomplete bits.
Parent ownership retains the chain after frontend children are released. The
shared test fixture's original budget/ranges remain defaults; only this combined
DPM test selects its larger fake allocation budget/range via test-only macros.

`scene-footprint-qualified-v2/result.json` records native/UBSan/i386 passes,
with7,364 checks reported by the combined fixture. The earlier single-draw
qualification is retained in `scene-footprint-qualified/`. These are injected
provider tests without GPU or API rendering.
`private-mesa-scene-footprint/RESULT.json` records the same final fixture
passing i386 against a private archive built with actual Mesa flags. The
original cached thin archive and every referenced object remain unchanged.
`scene-footprint-regressions/RESULT.json` records thirteen passing existing
owner/Gallium/NIR regressions against that archive, including region, DPM,
event auxiliaries and the old geometry path.

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py --source-only \
  --test test_scene_footprint.c --cache "$MESA_CACHE" \
  --output /tmp/remesa-scene-footprint
```

The separate event investigation is retained in `event-memory-fields/` and
`pds-decoder-locator/`. Seven nearby memory-write candidates do not modify the
reviewed F7 word: subsequent stores advance the CODE cursor and mutations hit
frame/stack counters. This local-slice result does not exclude later fixups.
ASCII/UTF16 identifier scans found only PDS buffer labels, not an identifiable
decoder. One selected compiler path was a dangling symlink and was recorded
as missing, not silently replaced. No unlabeled decoder is ruled out and no
event CF/C7/F7/87/lowE0 encoder was admitted from these bounded searches.

The next bounded event experiment is documented in
[LAB-EVENT-OPERAND-PROBE.md](LAB-EVENT-OPERAND-PROBE.md). The original finalization
audit executed CODE stores, real commit and metadata publication at two private
RAM bases with148/160/400 capacity cuts:21 CODE writes, zero later CODE writes,
148 effective bytes and field0c16 in each case. Only400 is the original requested
capacity; the other cuts do not qualify a different allocation policy or GPU
fetch length. The retained event relocation journal modifies DATA0/8/16 only.

The separate LAB tool generates twelve pinned-oracle hypothesis edits with
copy-only and original-zero controls. Its four test methods passed, including
rejection of all148 one-byte reference corruptions and refusal to overwrite
evidence. The generated files are not launchable or physically approved and
cannot supply a production instruction encoder. No Mesa/kernel code, FAST
runtime, geometry, shader implementation or execution gate changed in this step.
