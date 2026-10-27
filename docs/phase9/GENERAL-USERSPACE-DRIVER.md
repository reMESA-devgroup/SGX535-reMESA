# General userspace driver: desktop OpenGL and GLES 2.0

The active target is hardware-accelerated desktop OpenGL and OpenGL ES 2.0
through a common Gallium backend, with an installable driver. X11 integration
comes later. The initial physical target remains antiX i686 on the Dell Mini
12, PCI 8086:8108, SGX535 rev121. No OpenGL version/conformance or general
acceleration is currently claimed.

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
