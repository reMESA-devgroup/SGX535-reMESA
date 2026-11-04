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
rotation, shear and general clipping remain explicit unsupported cases.

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
