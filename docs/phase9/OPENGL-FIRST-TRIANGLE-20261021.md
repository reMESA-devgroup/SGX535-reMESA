# First OpenGL-origin physical triangle — SGX535-reMESA

**OPENGL_ORIGIN_TRIANGLE_PHYSICAL_LAB_PASS.** A real Mesa context compiled/linked its VS/FS, called `glDrawArrays(GL_TRIANGLES,0,3)`, reached Gallium `draw_vbo`, staged the independently encoded VS16/FS8, and completed one SGX535 submission. Actual `glReadPixels` returned the same 4096 bytes as the protected GPU memory capture and the independent public Mesa software oracle.

This is an offscreen, finite laboratory profile. Auxiliary historical PDS/other USE remain isolated in the laboratory carrier. This does not qualify a fully independent pipeline, general DRI execution or OpenGL 2.1. Mesa reports its actual computed desktop version 1.3; GLSL100ES inputs accepted by this context were used. No capability/version override was added.

## Physical evidence

| Observation | Result |
|---|---|
| Mini12 boot | `c03f0fbd-6299-49a9-b366-d676412a631b` |
| Actual client SHA-256 | `937526820308e51faaf73287bc55c523b0c37c44166d09c3654e1aa3aaba0cd7` |
| FBO | COMPLETE `0x8cd5`, 32×32 BGRA8 storage/RGBA8 renderbuffer, single sample |
| OpenGL draw | One nonindexed `glDrawArrays`; GL error0, pipe error0 |
| Protected job | context1/op1/slot0/generation1/serial1; phase9, ledger7, GEM6 |
| Shader source BO | handle42; independently compiled VS at0 (16 bytes), FS at16 (8 bytes) |
| USE delivery | VS copied/verified at64; FS at0; exactly one final green source write |
| Geometry source BO | handle43; 48 bytes of actual clip positions, remaining4048 bytes zero; no index source |
| Readback | 136 green, 888 zero, no other pixels;4096 bytes |
| SHA-256 | `d9a4ac05e454e11b73cffa17683bfeb47e6fc56a308c5694e2eb73b0028f05b1` |
| Lifetime | completion/retirement authenticated; no quarantine/timeout; final context CLOSE action13 returned0 |

`gl-hardware/actual-mini12/` contains raw OpenGL report, root authorization/binding, controller output, native observer,712 memory-journal records (5923840 bytes),18 sealed original files, readbacks and kernel logs. `ACTUAL-VERIFICATION.json`, `BOUND_SOURCE_DELIVERY.json`, `BOUND_GEOMETRY_DELIVERY.json` and `OPENGL_ORIGIN_VERIFICATION.json` separate hardware evidence from CPU fixtures. No selected fatal kernel signatures were found.

## Changes and exact blockers corrected

* Nonindexed start0/count3 is lowered through the existing backend, with actual source validation and no fabricated index BO. Other index/count/geometry combinations remain rejected.
* Mesa uses RECT renderbuffer storage when NPOT textures are unavailable. Narrow RECT BGRA8 render-target storage, transfers, clear, surfaces and framebuffer binding now work. Sampler binding, other shapes/formats/mips and multisampling remain rejected. Actual FBO status changed from incomplete to complete; it was not masked.
* The dedicated no-clear FBO quarter API avoids reading an unrelated, never-written dummy window attachment. The qualified backend correctly rejects that read; ordinary CPU-clear verification is preserved.
* An explicit default-off typed laboratory raster profile accepts half-pixel centers only for the separately validated quarter geometry. Original global raster validation remains byte-identical. Backend admission additionally requires exact input/output register mapping, bound original POSITION-only VS, constant green FS, authenticating callback, program-change requirement and exact source/window geometry. The irrelevant vertex-color clamp is normalized only here: the VS exports POSITION only, not COL/BFC outputs. Mesa's `nir_lower_clamp_color_outputs.c:is_color_output` independently confirms that distinction. Fragment clamp, two-sided lighting, culling, alternative bottom-edge rule and other unsupported states remain rejected.
* An unsupported `GL_RENDERBUFFER_SAMPLES` query was corrected to follow the actual ARB_framebuffer_object capability; GL errors and incomplete FBOs still fail explicitly.

## Sampling experiment and limits

Before the OpenGL draw, a separate fresh boot ran one controlled Gallium quarter triangle. Window positions were (8.25,8.25), (24.25,8.25), (8.25,24.25). There are no edge ties for the compared center candidates. Its complete136-pixel mask/hash matched actual public llvmpipe GL and the independent analytic half-center model. The integer-center candidate produced a distinct120-pixel mask/hash `9e2605aa502aa0a553acbcbaba77c13705a459bb21b1eebb0d2802aa8e199848`.

**CONFIRMED_PHYSICAL:** equivalence for this finite tie-free geometry. **REFUTED within the tested candidate family:** integer-center mask. **UNKNOWN:** universal subpixel/edge/origin rule, physical first PC, universal AF semantics. Those are not prerequisites to this finite result. See `sample-center-oracle/` and `quarter-hardware/`.

## Validation and reproducibility

Native, UBSan and i386/QEMU focused source tests passed:145 fixed-state cases and six retained-backend cases per environment; wrong geometry made zero interposed issues. The actual Mesa→GL→Linux backend CPU fixture passed11827 checks and was explicitly synthetic/GPU0. RECT storage and default-blocked OpenGL/CPU-clear regressions passed. Final Mesa25.2.0 static archive plus adjacent thin-archive objects and DRI built;11 archive-linked suites including32 real NIR cases passed. The unchanged377455-check encoder baseline was preserved, not rerun or counted as new validation. Prior checkpoint1624 artifacts verified unchanged.

`IMPLEMENTATION.patch` is incremental against `BASELINE.json`/`before/`, not the earlier checkpoint's original baseline. Exact patch replay, commands, focused logs and source hashes are retained. `REPRODUCE.py` only verifies/replays offline evidence; it does not run a hardware controller. Physical scripts have durable max-one markers and are not reusable as an automatic runner.

## Recovery and final machine state

The first v1 diagnostic initrd hit HOLD because this sprint's image assembly updated the first-loader script without updating its parent identity pin. This was an assembly error, not a PDS observation. The shell short-circuited before the custom module loader. The v2 image corrected the parent identity, preserved all unrelated members and passed five identity-chain CPU tests plus two actual first-load boots. Both images/logs remain preserved.

Two GPU submissions total in this sprint: one Gallium sampling experiment and one OpenGL experiment, each on a separate fresh boot. Two abandoned GL preflight bundles made zero GPU attempts: their blockers were found in CPU tests. There was no automatic retry. Final runtime controls are N/N,28 gate-policy files and ADMISSION unchanged. `custom.cfg` content and0600 mode, stock saved boot entry and empty one-shot entry were restored; no reboot was issued after the successful draw. The private module remains loaded until a normal reboot. No test process remains active.

## First remaining blockers and next action

**Framebuffer preservation:** the qualified native profile initializes COLOR to zero and replaces the render resource. Clear/prior-content preservation still fails before issue; a successful CPU clear is not evidence of GPU load/preserve. The next functional increment is a separately qualified COLOR preload/preserve path, with an exact nonzero preimage, one draw and full before/after comparison including every untouched pixel. Keep existing preservation rejection until that contract is implemented and tested.

**Independent auxiliary programs:** auxiliary PDS/event and other USE programs still prevent complete independence. Their producer/encoding recovery remains separate from this OpenGL-origin milestone. No historical carrier is incorporated as an original encoder implementation.

Window-system orientation/presentation, arbitrary triangles/raster edge ties, textures, depth/stencil, blending, broader GLSL and Desktop2.1 conformance remain unqualified. No generic admission was released.
