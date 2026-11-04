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
general perspective and clipping remain explicit unsupported cases. Bounded
rotation and shear were added subsequently as described below.

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
This does not establish sustained FPS, general perspective or OpenGL 2.1.
A separate ordinary `glFrustum` CPU probe confirms the next matrix-contract
refusal before native publication; it does not modify the physical test scope.
