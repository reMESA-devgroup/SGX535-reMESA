# The existing gears program in common CODE/PDS owners

The combined gears vertex body is already implemented and has a physical
positive witness. This change integrates that exact profile with common
program, geometry, PDS and parameter ownership. It does not reimplement the
shader encoders or claim a newly qualified draw.

The archived physical experiment at
`/home/gama/sgx535-offline/native1280-gears-lighting-positive-20261024T164800Z/`
used the attribute-normal body312 + typed tail8, eleven numeric temporaries,
and source37/transfer40. Its retained 48-byte input record supplied POSITION
and NORMAL in the first two float4s. The same existing compiled primary PDS
producer wrote DATA+20=`e0000000` and DATA+36=`12`. The complete selected scene
produced 118 white pixels on the physical SGX535 rev121 and retired. The
corrected oracle, original failed oracle and raw COLOR/Z evidence remain
preserved. This is a bounded positive affine/unit-normal-light witness, not
proof of arbitrary W, clipping, all shader inputs or general task formulas.

## Explicit selection and closure

The default selected-resource and linked-program APIs still reject gears.
The separate `remesa_shader_program_gears_attribute_resources` accepts only
the canonical attribute-normal program and reports its exact 320-byte CODE.
Its resource record retains the semantic layout: eight input words and eight
output words. The separate PDS profile retains the physical 48-byte fetch
record, including the unused third float4; it does not silently replace the
shader's input semantics or synthesize transformed vertices.

`remesa_persistent_gears_attribute_program_prepare` encodes that VS and the
existing smooth COL0 fragment shader into immutable PROGRAM storage. Both
offsets, USE entries and zero gaps derive from the live allocation. The vertex
reservation is 320 bytes, all effective CODE; the FS keeps its existing
8-byte effective/16-byte reservation contract. No padding instruction is added.

`remesa_persistent_gears_attribute_pds_prepare` requires a live raw geometry
owner and the complete immutable 36-coefficient source37/transfer40 owner.
It derives both primary PDS images and the existing empty secondary image
from their owners. The new PDS retains program, geometry and parameters.
It rejects incompatible parameter counts and parameter/VS/FS bank bindings
before allocation. Uncertainty retains the complete closure rather than
freeing possibly consumed storage. A read lease on the PDS defers destruction
of all three dependencies even after their public references are dropped.

The dedicated snapshot reports the entire closure and its selected
eight-input/eleven-temporary/eight-output requirement without provider IO.
These values describe the archived selected program; no general mapping from
arbitrary temporary counts to task fields is introduced.

`remesa_context_prepare_vertex_parameters` now connects the currently bound
gears VS constants to the common immutable parameter owner. It reuses the
existing source resolver: user data comes from its bind-time snapshot and
resource data is synchronized and read at use. The temporary host prefix is
discarded; the final source prefix is encoded from the new live allocation.
The full 36/40-word blocks remain intact. No shader, matrix, normal or color
evaluation happens on the CPU. This explicit helper requires a resource-only
persistent endpoint and does not run from normal rendering.

## Qualification boundary

Evidence is under `/home/gama/sgx535-offline/gears-common-pds-20261101/v1/`.
Tests exercise real builder NIR through the existing recognizer/program
encoder and common CODE/PDS constructors, both bound constant profiles,
immutable publication, metadata inspection without IO, parent deletion,
deferred destruction, bank/count refusals and ordinary/uncertain failures.
Native, UBSan and i486 ownership checks accompany regressions of the old
program/PDS owners, selected pipeline and white Lambert NIR path.

The constructor output is compared with the prior physical CODE320, EE8,
source37/transfer40 and primary/parameter PDS bytes. Only the live source and
USE relocation words differ. This is offline reuse of physically exercised
components, not physical approval of the new closure.

The legacy index, draw-state and scene builders still refuse this PDS profile.
The next executor integration must include the owned parameter TA reference
in order before the draw and carry this extra source through the complete
kernel use-set, bank/extent validation, job holds and retirement. It must also
preserve the appropriate coordinate/export and raster contracts. A successful
CODE/PDS preparation cannot replace those checks. Ordinary draw/capabilities,
the production FAST route and the working GLXGEARS path are unchanged.

No hardware operation was performed for this integration. The failed
clip/MATRIX_WINDOW experiment remains rejected and the last verified Mini 12
state remains stock. Its consumed GPU boot must never be reused.
