# OpenGL frontend coordinates and the native draw callback

The earlier statement that `viewport_transform_lowered=true` asks Mesa to
insert viewport arithmetic was incorrect for the pinned Mesa 25.2 source.
`st_extensions.c` uses this hint to include POSITION in
`LowerBuiltinVariablesXfb`. The GLSL linker uses that information when
preserving outputs for transform feedback. The hint does not invoke
`nir_lower_viewport_transform`; the SGX535 driver never calls that pass.
Transform feedback remains refused by `create_vertex` when stream output is
requested.

`test_opengl_viewport_contract.c` observes actual state-tracker GL compilation
and `glDrawArrays` at the driver's `create_vs_state` entry. With the hint false
and true, the delivered passthrough VS text is byte-identical. Both versions
are recognized by the driver, with zero viewport scale/offset loads and zero
reciprocal instructions. The recording backend still refuses the draw. This
is frontend evidence, not binary delivery, pixel rendering or GPU approval.
The cap toggle exists only in this test; production capabilities stay unchanged.

Evidence:
`/home/gama/sgx535-offline/opengl-native-coordinate-contract-20261102/v1`.
The `current-driver` result links the current screen object. Earlier failed
test builds and the mistaken assumption that a recording-screen report must
contain a 16-byte binary are retained separately. The actual report gives
successful VS recognition and zero selected binary bytes; that is not a
claim of shader delivery.

## Context-local native scene draw

`remesa_context_enable_homogeneous_draw_lab` connects `draw_vbo` to the existing
homogeneous scene submission helper on an explicitly selected resource
context. Enabling performs no allocation, mapping or submission. The default
route and screen/API capabilities are unchanged.

The adapter uses the qualified banks 4/3/4 and Z clear 1. Bound shaders,
constants, raw position/normal data, ISP and viewport remain the inputs; the
existing constructors perform the work. It rejects indirect/multidraw/draw-ID
forms and unsupported raster or sampling state. Zero-count draws issue no
work. Existing attachment contents remain unsupported, so a second draw does
not silently discard the first one.

The submit helper already shares a single fence between COLOR, Z, the context
and caller. The adapter drops only its local reference. Definite nonissuance
preserves retry, uncertain issuance retains poisoned resources and suppresses
retry, and failed validation preserves any earlier valid fence. Index-owner
cleanup continues through the existing common draw cleanup.

The draw callback and direct helper produce identical packets and all 13 READ
images at 1280×800, 640×640, 641×479 and 1600×768. At the first two sizes these
inputs also match the sealed catalogues from the preceding physical runs.
Tests exercise actual Mesa/NIR and owned scene constructors with modeled GEM
and native receipts, including UBSan. The adapter itself has not received a
new physical approval.

Evidence:
`/home/gama/sgx535-offline/homogeneous-draw-vbo-20261102/v1/final`.

The demonstrated OpenGL/GLX frontends and original GLXGears remain available
through their earlier qualified paths. Wiring the complete OpenGL state
tracker to this new native scene path still requires explicit frontend
admission, supported shader/state forms and render-pass load/preservation.
Neither this adapter nor changing a capability bit establishes OpenGL 2.1,
GLES 2.0 or continuous production-driver completeness.
