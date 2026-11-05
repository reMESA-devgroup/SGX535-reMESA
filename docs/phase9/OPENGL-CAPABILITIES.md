# OpenGL capability gates (Mesa 25.2.0)

No OpenGL version is currently advertised. OpenGL 3.0 feasibility remains **UNKNOWN**. The first real context remains the implementation priority. This is a capability inventory, not a conformance claim.

The independent encoder increment adds scalar virtual-register lowering,
canonical **host-only** USSE serialization/decoding, symbolic PDS relocations,
and immutable plans owned by actual Gallium shader callbacks. The plans reject
stale current bindings and return `ENOTSUP` for hardware emission. No SGX opcode,
instruction width, register field or PDS launch encoding was admitted. These
modules therefore do not change the OpenGL admission mask `0x1f` or establish
GPU shader execution. See [the independent evidence inventory](independent-encoding-evidence-20261009.json).

Latest context increment: the complete Mesa25.2.0 i386 EGL/GBM/DRI runtime
now reaches the real reMESA constructor on the Mini12. Identity, transport and
diagnostic screen allocation pass; OpenGL admission rejects with ENOTSUP and
missing mask0x1f. Only DRM version queries were permitted; zero SGX submissions.
Restricted fragment UBO0 specialization is implemented as synchronized host
selection of existing constant-color profiles, not GPU uniform execution.
See [the exact trace](OPENGL-CONTEXT-TRACE.md).

| Target | Driver status | New barriers beyond earlier versions |
|---|---|---|
| 2.0 | BLOCKED | General vertex/fragment shaders, textures, blend/stencil, draw buffers and inherited GL functionality |
| 2.1 | BLOCKED | GLSL 1.20, sRGB textures and pixel buffers |
| 3.0 | BLOCKED / feasibility UNKNOWN | GLSL 1.30, integer/float formats, FBOs, texture arrays, transform feedback, multisampling and normative limits |

Mesa-side object/state management and lossless data conversion can be valid emulation. Planned emulation is not implemented support. No software rasterizer or CPU shader execution counts as SGX acceleration.

Mesa 25.2.0 accepts four color attachments in its GL 3.0 gate with an explicit nonconformance comment. reMESA must use the normative eight-attachment requirement, not this shortcut. Geometry shaders first belong to later versions and are not a 3.0 prerequisite.

| Functionality | Version | Native evidence | Mesa emulation | Current driver | Missing contract |
|---|---|---|---|---|---|
| Programmable vertex shader / GLSL 1.10 | 2.0+ | UNKNOWN general ISA; CONFIRMED diagnostic position program only | OFFLINE QUALIFIED lowered and variable-IO identity-position NIR, bounded scalar channel reconstruction and CPU coordinate preparation; no general CPU shader execution or CPU rasterizer | PARTIAL | General shader operations, uniforms/linkage and independently justified native ISA; restricted identity admission does not meet GLSL1.10 |
| Programmable fragment shader / GLSL 1.10 | 2.0+ | CONFIRMED fixed observed constant-color profiles | Restricted NIR constant and aligned FS UBO0 host specialization OFFLINE QUALIFIED; general shader execution not emulated | PARTIAL | ALU, varyings, uniforms, texture operations and general output semantics |
| GLSL 1.20 nonsquare matrices | 2.1+ | UNKNOWN | Mesa can lower matrix algebra once scalar/vector shader operations exist; not qualified here | UNSUPPORTED | General shader operations and GLSL conformance |
| GLSL 1.30 integer/unsigned operations and stage interfaces | 3.0 | UNKNOWN | No valid emulation established; float approximation is insufficient | UNSUPPORTED | Exact integer semantics, shader IO and independently justified ISA |
| Vertex/index buffer objects, attributes and primitive modes | 2.0+ | CONFIRMED one indexed diagnostic triangle; wider geometry OFFLINE QUALIFIED | Host packing/conversion possible; full OpenGL semantics not implemented | PARTIAL | General attributes, formats, primitive modes and reusable hardware lifecycle |
| Pixel buffer objects and pack/unpack | 2.1+ | UNKNOWN GPU transfer path | Host bookkeeping/copies can be valid without CPU rasterization; only restricted Gallium transfers qualified | PARTIAL | GL pixel-store/PBO bounds, synchronization, arbitrary formats |
| Textures: 1D/2D/3D/cube, NPOT, samplers, mipmaps, shadow compare | 2.0+ | UNKNOWN | No qualified texture sampling emulation | UNSUPPORTED | Addressing, formats, filtering, sampling instructions and shadow semantics |
| sRGB texture sampling | 2.1+ | UNKNOWN | Shader decode could be valid with filtering/precision validation; not implemented | UNSUPPORTED | Texture sampling and correct sRGB filtering |
| Viewport, clipping, scissor and raster modes | 2.0+ | CONFIRMED restricted 32x32 triangle path; broader behavior UNKNOWN | OFFLINE QUALIFIED canonical32 viewport plus six-plane homogeneous CPU clipping and projection through recording-backend Gallium callbacks; native profile remains inside-volume w=1 only | PARTIAL | General clipping/perspective, viewport/scissor, raster rules, points/lines/culling |
| Blend equations/separate RGB-alpha, color masks and logic operations | 2.0+ | UNKNOWN | No qualified emulation; CPU framebuffer raster/composite is not SGX acceleration | PARTIAL | Disabled/fullmask CSO admission qualified; actual blending, masks and logic operations unsupported |
| Depth/stencil including two-sided stencil | 2.0+ | UNKNOWN | No valid qualified emulation | PARTIAL | Disabled-tests CSO admission qualified; enabled depth/stencil formats and operations unsupported |
| Multiple draw buffers | 2.0+ | UNKNOWN | Multipass may be valid only with ordering/side-effect validation; not implemented | UNSUPPORTED | Multiple color outputs and attachment limits |
| Occlusion queries and query lifetimes (inherited pre-2.0) | 2.0+ | UNKNOWN | No valid qualified emulation | UNSUPPORTED | Attributable sample counts and query lifecycle |
| Framebuffer clear and readpixels | 2.0+ | CONFIRMED diagnostic readback; GPU clear UNKNOWN | OFFLINE QUALIFIED CPU BGRA8 clear with writer-fence gating; not GPU clear, and native draw still replaces the target | PARTIAL | General formats/rectangle readback and genuine clear encoding |
| Legacy fixed-function compatibility requirements | 2.0+ | UNKNOWN | Mesa may lower legacy state to shaders; blocked by general shader backend | UNSUPPORTED | Fixed-function emulation shaders, legacy draw/pixel semantics |
| Framebuffer objects and renderbuffers | 3.0 | CONFIRMED single BGRA8 32x32 COLOR resource | Object validation/bookkeeping possible in Mesa; cannot supply missing render formats | PARTIAL | Completeness, depth/stencil attachments, blits and general targets |
| Vertex array objects | 3.0 | No new GPU instruction needed for object bookkeeping | Valid Mesa-side state management once underlying draw contracts exist; not qualified end-to-end | UNSUPPORTED | Real context and complete vertex state |
| Float color/depth and half-float vertex formats | 3.0 | UNKNOWN | Conversion possible only with precision/range proof; not qualified | UNSUPPORTED | Formats, rendering and conversion semantics |
| Float/RG/integer/array textures, RGTC, packed/shared-exponent formats | 3.0 | UNKNOWN | Compressed-format decode/conversion may be valid with sampling support; not implemented | UNSUPPORTED | Format storage, sampler semantics and exact integer sampling |
| sRGB framebuffer writes | 3.0 | UNKNOWN | Shader encoding insufficient alone for correct blending; not implemented | UNSUPPORTED | Linear blending, conversion and framebuffer semantics |
| Per-draw-buffer masks, indexed enable and integer outputs | 3.0 | UNKNOWN | No valid qualified emulation | UNSUPPORTED | Draw buffers and independent state |
| Explicit texture LOD in shaders | 3.0 | UNKNOWN | No qualified lowering | UNSUPPORTED | Shader sampling and LOD semantics |
| Transform feedback | 3.0 | UNKNOWN | Host execution of shaders would not establish native SGX shader support; no qualified implementation | UNSUPPORTED | Capture of actual vertex results, bounds and ordering |
| Conditional rendering | 3.0 | UNKNOWN | Waiting on a genuine GPU query on CPU can preserve semantics; genuine query unavailable | UNSUPPORTED | Occlusion queries and conditional submission ordering |
| Range mapping | 3.0 | CONFIRMED two-page GEM/MMU CPU transfer; Gallium leases OFFLINE QUALIFIED | Host range/flags bookkeeping possible | PARTIAL | GL mapping flags, invalidation and complete reusable hazards |
| Multisample render targets / resolve | 3.0 | UNKNOWN | No qualified Mesa emulation; do not enable FakeSWMSAA to claim support | UNSUPPORTED | Sample coverage/storage/resolve and required limits |
| Reusable fences/resource ownership | implementation prerequisite | CONFIRMED single retained operation retirement; repeated physical reuse UNVERIFIED | Lifecycle tests OFFLINE QUALIFIED; mocked completion is not GPU proof | PARTIAL | Fresh bounded admission, two-operation physical verification |
| DRI/OpenGL contexts and presentation | implementation prerequisite | CONFIRMED CPU/Xorg publication of prior SGX output; direct scanout UNKNOWN | Actual target DRI artifact loading PASS; a narrow truthful Gallium screen-capability contract now passes native/UBSan/i386 tests, but state tracker GL admission remains closed | PARTIAL | Vertex shaders, textures, framebuffer preservation, truthful GL caps and real GL context; synchronized presentation |

Machine-readable inventory and exact pinned Mesa gate expressions: [opengl-capabilities.json](opengl-capabilities.json). This historical reference is missing from the repository; no corresponding generator or tracked copy was found during the reMESA integration audit. The table above remains available, but the JSON cannot currently be reproduced from this checkout.

Normative references: [OpenGL 2.0](https://registry.khronos.org/OpenGL/specs/gl/glspec20.pdf), [OpenGL 2.1](https://registry.khronos.org/OpenGL/specs/gl/glspec21.pdf), [OpenGL 3.0](https://registry.khronos.org/OpenGL/specs/gl/glspec30.pdf). Public specifications are requirements references; no disputed SGX implementation is used.

Restricted vertex increment (2026-10-09): lowered position pass-through NIR and canonical clip-to-window host preparation are offline qualified. This does not establish general hardware vertex shader execution. Disabled blend/depth/stencil and solid/no-cull CSO ownership now exist; Mesa default half-pixel raster semantics remain rejected. No GL version or extension is newly advertised. See the machine-readable inventory for updated per-component evidence.

Framebuffer increment (2026-10-09): the driver now carries explicit load/store
intent and rejects populated-target reuse through the native REPLACE-only path
before dispatch. Diagnostic discard remains explicit. This is a safety contract,
not proof of SGX framebuffer preservation; native load/store behavior and the
required PBE encoding remain UNKNOWN. GPU clear is still BLOCKED because the
only admitted clear is a counted CPU BGRA8 transfer with writer-fence gating.

The pinned Mesa DRI artifact now builds and loads through the real i386 DRIL
entry under QEMU, with invalid-device and missing-artifact rejection covered.
This does not admit a state-tracker OpenGL context: general shader translation,
textures, depth/stencil, framebuffer preservation and truthful GL capabilities
remain missing. No OpenGL version or extension is advertised.

Resolution increment (2026-10-09): backend-limited, aligned transfer storage and
constant-output CPU geometry preparation are qualified through 1920x1080,
including 1280x800. Larger storage is tested through the persistent provider and
kernel memory-service source with injected kernel APIs. This does not change
GPU texture/render-target limits, sampler support or OpenGL admission. Dynamic
PRESERVE/DISCARD observation checks exist, but physical COLOR-load behavior and
larger render-surface command/state remain UNKNOWN.

Current integrated increment: real Gallium transfer callbacks support bounded
CPU raw-texel clears and overlap-safe resource copies across arbitrary admitted
storage sizes. Per-target reservation/publication/retirement gates protect
reuse; native PRESERVE still fails closed. Constant FS admission now accepts
scalar-constant vec4 construction and identity-copy chains only. Fragment
interpolation and coordinate-convention capabilities are disabled. Direct XCB
CPU publication is row-tiled and tested offline through1920x1080. No OpenGL
version, context, GPU sampling or larger GPU surface is established by these
changes. See the current driver-contract qualification receipt.

Admission increment (2026-10-09): actual Gallium sampler views now retain checked
RGBA/BGRA transfer backing with immutable level/layer/swizzle descriptors and
allocation cookies. Standard Mesa view release remains safe after context
destruction. Nonempty sampler/view bindings still reject before dispatch and GPU
sampling caps remain disabled. Recording-backend identity draws with explicit
near/far depth clipping use homogeneous CPU primitive preparation and semantic
packet compaction; production dispatch is unchanged. Neither component supplies
unknown GPU texture or raster encodings.

The exact new i386 frontend was loaded on the Mini12 under the qualified
VERSION-only syscall filter. PCI, DRM transport and diagnostic screen creation
passed; OpenGL admission remained ENOTSUP with mask0x1f. No SGX/memory ioctl or
GL call was issued. A complete baseline profile remains unavailable: restricted
NIR profiles and texture object bookkeeping do not satisfy inherited OpenGL
shader, sampling, raster, render-state and load/store requirements.


### Independent host IR and seeded evidence increment (2026-10-09)

Actual Mesa shader callbacks now retain a typed, validated semantic IR for
identity position and constant endpoint color profiles. Its register IDs,
opcode names and serialization/dump are original **host** definitions; no
USSE/PDS bit layout or hardware register encoding is implemented. Unknown
hardware encoding fails with `ENOTSUP` and leaves output untouched.

Texture metadata preparation is connected to context-owned Gallium views and
polls the real frontend writer before querying retained mapping metadata.
Canonical `RMTD` bytes describe host diagnostics only and cannot be submitted
as an SGX texture descriptor. Provider claims still require independent
mapping/completion authentication. No GPU sampling capability is advertised.

A separate seeded-session validator requires complete ownership, completion,
visibility, acknowledgement, ticket retirement, attributed captures and
cleanup records. It validates transcript correspondence, not physical truth.
The preparation card is non-executable: current STOCK has no experimental
endpoint, the source-authentication/capture adapter is missing, and historical
proprietary shader-derived profiles are not admitted under the current policy.
The forbidden 440-byte original Poulsbo shader image was not accessed or used.
No corresponding asset path is guessed from unrelated recorded file sizes.

OpenGL admission remains `0x1f`. Mesa's version computation assumes a working
baseline compatibility implementation; its fallback version is not a test of
our conformance. Host IR and descriptor validation do not establish any OpenGL
version, GPU COLOR preservation or general shader/texture execution.

The user permits `psb_dri.so` as a non-extractive reference but prohibits code
extraction from it. No `psb_dri.so` or `Xpsb.so` binary was accessed for this
increment. New shader encodings must remain independently authored and verified.
