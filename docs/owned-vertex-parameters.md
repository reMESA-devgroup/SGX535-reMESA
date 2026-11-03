# Owned vertex parameters for viewport integration

This document records the initial parameter-owner and affine viewport work.
Its rejected MATRIX_WINDOW experiment and remaining-work discussion below
describe that experiment, not the current status of every scene path. The
later captured frontend/general-session path has physically passed at
1280×800 and 640×640 with the existing homogeneous scene, parameter owner,
PDS/USE/TA encoders, retirement and exact COLOR/Z24 expectations. See
[the joined session results](captured-general-session-client.md).

Do not rebuild those encoders to address the next lifecycle gap. Published
parameter objects and captured READ inputs are currently immutable. Updating
them between submissions needs a coordinated frontend, Linux transport and
kernel ownership/visibility contract; an idle fence alone does not authorize
a CPU write. The finite physical results do not qualify arbitrary uniforms,
programs, clipping or complete OpenGL 2.1/GLES2.

The physical clip/MATRIX_WINDOW experiment retired but produced only clear.
That result remains rejected. The next missing piece is executable vertex
transformation and its owned parameters, rather than another state encoder.

An earlier physical result is already available at
`/home/gama/sgx535-offline/parameter-mad-physical-pair-20261024T071000Z/PHYSICAL_PAIR_RESULTS.json`.
It qualifies the original typed MUL+ADD program with PARAM0=(1,1,1,1) and
PARAM1.x changing from 0 to 2.125. The 136 green pixels moved two columns in
two independently authenticated boots, with retirement and cleanup. It does
not qualify non-unit scales or all other components. Those checkpoints and
their manifests were inspected read-only; no physical replay was requested.

## Common memory owner

`remesa_persistent_vertex_parameters_prepare` now builds an immutable object
through the existing allocation, pin, VA reservation, visibility, rollback and
quarantine machinery. It reuses the existing parameter source, selector EE,
PDS parameter-image and TA-reference encoders. No executable template, caller
address or new instruction encoding is accepted. The block constructor
`remesa_persistent_vertex_parameter_block_prepare` accepts initialized blocks
up to 127 coefficient words through the same source/DMA encoder. Unsupported
two-descriptor transfers are refused before allocation. The sixteen-word
wrapper preserves the old physical policy.

| Span | Effective or committed bytes | Reservation |
| --- | ---: | ---: |
| Coefficients | 64, sixteen exact uint32 float bit patterns | Part of source |
| Source including self-address prefix | 68, seventeen DWORDs | 80 transferred bytes |
| Parameter auxiliary EE | 8 | 16 bytes |
| PDS parameter object | 60, including 12 CODE bytes | 64 bytes |
| TA parameter reference | 8 | 8 bytes |
| Complete object | 168 bytes including zero gaps | One 4096-byte allocation |

That table describes the sixteen-coefficient wrapper. The real gears profiles
retain all 36/40 coefficient words: 37/41 effective source words and 40/44
transferred words. Their offsets derive from those extents within the same
one-page owner; they are not truncated to the viewport example. Tests also
cover smaller and maximum-size blocks and refusal of unsupported counts.

The three transferred words after the effective source are initialized zeros.
Reserved CODE space is also zero; no artificial instructions fill it.
Allocation capacity, initialized source extent, transferred count and shader
consumption are different contracts. In particular, the existing MAD encoder's
nine-DWORD minimum does not authorize shrinking the physically exercised
twenty-DWORD transfer policy. No claim is made that the GPU consumes every
transferred word.

Every address and the USE fixup derive from this owner's live allocation. The
snapshot reports source17/transfer20, effective EE8 and the distinct PDS/TA
spans without provider reads, hashing or synchronization. Legacy ISP, close
and fragment-state snapshots reject the new object kind. Public CPU writes
are refused; read leases defer destruction. Uncertain failures retain backing
and prevent endpoint close, as for the existing owners. Whole-scene bank
compatibility and authenticated job holds still belong to scene composition.

## Bound viewport values

`remesa_context_window_viewport_parameters_snapshot` derives two dense vec4s
from the retained, validated window viewport: scale.xyz/1 and translate.xyz/0.
Eight remaining coefficient words are zero for the existing upload policy.
The operation copies state bits; it does not evaluate or replace vertex data.
Rejected bindings and output aliases leave the output unchanged. Failed
submission/attachment state is refused. No mapping or provider operation runs.

These are affine parameters. They do not implement reciprocal W or clipping,
and do not establish general OpenGL position semantics. A W=1 experiment must
remain explicitly bounded until a full perspective contract is integrated.

## Verification and remaining work

Evidence is under
`/home/gama/sgx535-offline/owned-viewport-parameters-20261101/v1/`.
The new owned upload matches the previous physical shifted-MAD source bytes,
EE8 and PDS60, apart from live source/USE relocations; the TA count word matches.
This compares a new constructor with old physical inputs, not execution of
the new owner on hardware.

The coherent current Mesa/NIR build tests the owner, viewport binding, existing
contexts and legacy context regression. Two additional actual Gallium cases
connect the bound viewport directly to the common parameter owner at 1280×800
and 640×640, without scene dispatch. Native/UBSan/i486 tests exercise the
new owner and old linked draw-state preparation, including rollback and held
uncertain failures. The four previous capture trees remain byte-identical.

The real pinned Mesa `nir_lower_viewport_transform` was also exercised:
it is a no-op on the current client's already lowered `store_output` shader;
before I/O lowering, it inserts viewport loads, reciprocal/clamp, multiplication
and addition around a position `store_deref`. The current semantic compiler
refuses that transformed graph rather than emitting the old passthrough body.
That is the next compiler/program/export boundary to integrate with these
owned parameters and the existing arithmetic encoders. This change does not
add the parameter reference to a scene, admit a new shader or widen normal
draw/capability gates. No new physical test, FPS or API approval follows.

The maintained ownership test can be selected with
`mesa/sgx535/tests/qualify_shader_program.py --test test_vertex_parameter_owner.c --source-only`
using that tool's existing pinned cache/output options. The artifact scripts
record the exact commands used for this qualification.
