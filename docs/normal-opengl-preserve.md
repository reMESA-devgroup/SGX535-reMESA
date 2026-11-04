# Ordinary draws without another clear

The normal Mesa/Gallium matrix path can retain COLOR/Z between draws on the
same authenticated session. `REMESA_MATRIX_PRESERVE_DEPTH` requires KEEP,
matching attachment identities and pitches, a previous retired draw, and no
intervening clear. CPU writes, remapping and destruction of the retained
attachments remain excluded. CLOSE releases the source before backing.
The experimental screen opt-in remains explicit; unsupported states still fail.

The TA stream consumes 76 bytes: INITIAL STATE/INDEX, DRAW STATE/PARAM/INDEX,
and CLOSE. The separate Z-clear primitive is omitted. The initial INDEX must
remain: the first 56-byte candidate retired but produced no new COLOR/Z.
Keeping that existing predecessor fixed the physical failure. Its index,
vertex fetch and USE program have explicit validation; exact INITIAL DATA has
zero fragment references. There are eight TA references, ten USE entries,
two vertex windows and one fragment task. Reserved prefix bytes are not extra
consumed tasks. No shader instructions were added to fill capacity.

Physical qualification passed on SGX535 rev121 at **640×640 and 1280×800**:
clear+B far, clear+A near, then B far without clear, through normal
`glDrawArrays` in one Mesa context. COLOR union, preserved near-overlap Z,
new B-only Z, four guards, immutable inputs and individual retirement passed.
The independent B baseline observed Z24 `0xa00000`; A observed `0x800000`.
Cleanup completed with all eight controls N and no new kernel messages.
Successful experimental boots were retained at the user's request; stock
configuration/default were preserved, without claiming a stock-return test.

Raw failed and passing captures, offline fault tests, source pins and audits:

- `/home/gama/sgx535-offline/normal-preserve-20261103/v1/PHYSICAL_FAILURE_RESULT.json`
- `/home/gama/sgx535-offline/normal-preserve-20261103/v2-initial-index/PHYSICAL_640_PRESERVE_RESULT.json`
- `/home/gama/sgx535-offline/normal-preserve-20261103/v2-initial-index/PHYSICAL_1280_PRESERVE_RESULT.json`

This is not OpenGL 2.1, desktop/app, sustained-FPS or installable-beta approval.
The existing closed LAB catalogues and their admission remain unchanged.

## Bounded affine W=1

Normal `glLoadMatrixf`/`glDrawArrays` also accepts positive constant W=1.
The existing diagonal-scale and translation bounds remain; every vertex/axis
additionally requires `abs(scale)*abs(position)+abs(translation) <= 7/8`.
Outward Q15 integer bounds enforce this in both frontend and kernel, without
floating-point operations or constructing transformed vertices in the kernel.
The multiplication and sum fit in 32 bits. Historical W=2 behavior is retained;
bounded rotation, shear and projective input checks were added subsequently
as described below. Geometry requiring clipping still refuses.

The W=1 projection obtained by halving all 16 cells of the qualified W=2
projection has exactly the same normalized coordinates. That input relationship
was checked rationally before reusing the existing pixel oracle. Real W=1 draws,
depth and no-clear preservation passed at 640×640 and 1280×800 on rev121.
All fences, MMU/ownership checks, guards and cleanup passed; the successful
experimental boot was kept idle. A Wi-Fi-ready notification captured before
GPU execution is classified separately in the 1280 audit; its initial strict
refusal is preserved. No GPU gate was changed for that notification.

Evidence: `/home/gama/sgx535-offline/normal-affine-w1-20261104/v1/` contains
`PHYSICAL_640_AFFINE_RESULT.json`, `PHYSICAL_1280_AFFINE_RESULT.json`, numeric
boundary/corruption tests and the unchanged W=2 archive regression.


## Bounded affine rotation

The shared frontend/kernel predicate now accepts all nine affine 3×3 cells
within magnitude one. For each input vertex, outward integer bounds require
`sum(abs(M[row,col])*abs(position[col])) + abs(translation[row]) <= 7*W/8`.
W remains positive constant 1 or 2; translation and input bounds are retained.
The maximum Q30 sum fits unsigned 32 bits. No transformed vertices are created
by this predicate. Shaders, PDS/ISP encoders, UAPI and resource ownership are
unchanged; unsafe or unsupported inputs still refuse before submission.

An exact 90-degree projection supplied through ordinary `glLoadMatrixf` and
`glDrawArrays` passed physically on rev121 at **640×640 and 1280×800**. Authored
raw geometry gives the same exact clip coordinates as the prior fixture, so the
existing pixel/depth oracle was reused. All three draws, including final
no-clear preservation, matched COLOR/Z with retirement, immutable inputs,
guards and complete cleanup. Neither run added kernel messages. The successful
1280 boot was retained idle with all eight controls N and stock recovery intact.

Native, UBSan and i486 tests cover 100,000 dense affine cases, both strides,
45/90-degree cases, overflow and W-row refusals; previous W1/W2 tests also pass.
The actual `glRotatef` entry point passed the Mesa CPU model only: hardware
qualification used the exact matrix, and does not claim arbitrary-angle proof.

Evidence: `/home/gama/sgx535-offline/normal-affine-rotation-20261104/v1/`
contains both `PHYSICAL_*_ROTATION_RESULT.json` reports and `OFFLINE_INDEX.json`.
That result does not establish sustained FPS or OpenGL 2.1. The subsequent
`glFrustum` component is described below; its initial refusal probe remains
preserved with the rotation evidence.


## Bounded projective matrices

Ordinary `glFrustum`/`glDrawArrays` now reaches the same native executor.
Matrices with a nonzero W row use signed outward Q20 intervals: every vertex
must prove `1/8 <= W <= 4` and `abs(X/Y/Z) <= 7*W/8`. Finite coefficients
remain bounded by four, and raw position/normal/padding checks are retained.
The proof performs no division, FPU operation or GPU vertex construction.
Products and clip comparisons fit signed 64 bits. The previous constant-W
branch remains intact. No shader, PDS/ISP encoder, UAPI, ownership or fence
contract changed; geometry requiring clipping continues to refuse.

Actual `glFrustum` calls passed physically at **640×640 and 1280×800**:
three draws, two clears, final no-clear preservation, exact COLOR/Z,
individual retirement, four guards, immutable inputs and complete cleanup.
The two authored frusta have the same exact NDC as the previous independent
oracle. Their raw vertices all have z=-3/8 and resulting W=3/8: this hardware
case does not qualify interpolation between vertices with different W.
Both runs added no kernel messages; the successful 1280 boot was kept idle,
all eight controls N, with stock recovery configuration preserved.

Offline coverage includes 200,000 IEEE interval cases and 100,000 dense signed
projective/cancellation cases per native/UBSan/i486 variant, both record
strides, prior affine tests, real Mesa positive/refusal cases, held GEM/MMU
lifecycle faults and capture-verifier faults. Previous physical archives still
produce identical verifier reports. Invalid W tests now use negative W;
a zero constant matrix cell is legitimate in a projective transform.

Evidence: `/home/gama/sgx535-offline/normal-perspective-20261104/v1/`, especially
`PHYSICAL_640_PROJECTIVE_RESULT.json`, `PHYSICAL_1280_PROJECTIVE_RESULT.json`,
`EXACT_NDC_EQUIVALENCE.json` and `OFFLINE_INDEX.json`. This is no claim of
complete OpenGL 2.1, arbitrary clipping, sustained FPS or desktop/app approval.


## Different W at each vertex

The same runtime and module subsequently passed ordinary
`glLoadMatrixf`/`glDrawArrays` with W=5/16, 3/8 and 7/16 at the three vertices,
at **640×640 and 1280×800**. Only qualification inputs and verifier cases
changed; no driver code, shader, encoder, kernel UAPI or LAB admission changed.
Authored raw vertices project to the same exact NDC as the prior independent
COLOR/Z oracle. Both three-draw sequences, including no-clear preservation,
passed pixels, retirement, guards, immutable inputs and complete cleanup.
Neither run added kernel messages. The successful 1280 boot was retained idle,
all eight controls N, with the original stock recovery configuration preserved.

This isolates geometric perspective division. NDC Z is constant within each
triangle and other attributes are constant; varying-attribute interpolation,
a depth gradient and texture sampling are not qualified by this case.
Offline real-Mesa models, held GEM/MMU lifetime faults and capture faults passed;
eight earlier physical archives still produce their exact previous reports.

Evidence: `/home/gama/sgx535-offline/normal-varying-w-20261104/v1/`, especially
`PHYSICAL_640_VARYING_W_RESULT.json`, `PHYSICAL_1280_VARYING_W_RESULT.json`
and `OFFLINE_INDEX.json`. The next normal texture API boundary remains refused;
this is not OpenGL 2.1 conformance, desktop/app or sustained-FPS approval.
