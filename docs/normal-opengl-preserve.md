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
