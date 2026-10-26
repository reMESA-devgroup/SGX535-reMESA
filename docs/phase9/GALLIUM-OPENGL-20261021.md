# SGX535: bound Gallium shaders physically rendered; actual GL frontend advanced

## CONFIRMED_PHYSICAL

The opt-in laboratory client now delivers both actual bound NIR shaders to the physical SGX535. Exactly one draw_vbo reached the retained submission endpoint in boot bca65b95-0409-4db9-b8b4-b31e1b435013. Client returned0, authenticated1, quarantine0; context1/operation1/slot0/generation1/serial1 retired at phase9/ledger7/GEM6. Full readback:120 green,904 zero,4096bytes, SHA256 a5b77d47f7c9f2c626a02d59d13a6f816dd300259e9cc626ed6a9ddb9bef1508.

The capture and independent replay prove VS16 at CPU source offset0, FS8 at offset16, then VS at USE+64 and FS at USE+0. Exactly one final USE write matches that source. VS SHA7fc6ee96f766b39e90907985eb2e9ffd638a40381c5444c711547d8041d65b07; FS SHAbfc0401c5c4de0370bd7cb68c69a7eb9d019a2e7ace188d0d5dbe3d6ef900b8a. Sealed ownership/memory observations, native operation, completion/retirement and independent color observer agree. Actual renderer executable SHA4d449bb68f5c0b098bc1f1e35d1fadee823d7451632f771294a2907a69b570fe; its exact build is preserved separately from subsequent CPU-only changes.

This is a restricted real Gallium laboratory draw, using historical auxiliary PDS/otherUSE and fixed qualified geometry/state. It is not a fully independent pipeline, general submission backend, OpenGL draw, or OpenGL2.1 implementation.

## IMPLEMENTED_OFFLINE

- Default-off independent_vertex carrier option and explicit --execute-qualified-green-vertex-once CLI. Both bound NIR shader tokens supply original encodings in one immutable owned CPU program BO. Mismatched VS preimage rejects before USE write or retained call. Old CLI/capture format remains unchanged when disabled.
- Real Mesa state_tracker offscreen laboratory frontend, caller-supplied screen/resource, actual GL dispatch. It creates a context, clears/maps the resource, probes shader compilation/linkage and draw failures. No version or capability override; normal DRI admission unchanged.
- Missing null-safe sampler_view_release lifecycle callback and inactive set_polygon_stipple callback. Enabled polygon stippling remains rejected.
- MEDIUM FS output accepted only for direct exact0/1 constants, with no ALU. LOW, intermediate mediump arithmetic and0.5 remain rejected; precision semantics are not generalized.
- Shared viewport identity validation accepts Mesa's canonical{0,2,4,6} and the preexisting allzero convention. Permutations/sign changes remain unsupported. Applied consistently to screen, draw validation and clipping preparation.
- New CPU class17 E6 field rule from original Windows constructor: 0x170000e6|((even_cursor+1)<<9), cursor even0..66. Unknown E0/68 controls remain unencodable. No new physical ISA claim.

## Actual GL frontend limits

Mesa computes version1.3 and GLSLVersion0 from existing capabilities; this is an internal laboratory observation, not a compliance claim. GL2.1 context creation is rejected. GLSL110 compilation is rejected. The source alternative100ES compiles/links the minimal passthrough VS and constant-green FS, and driver compilation now succeeds for both.

Actual GL draw still does not reach physical submission. Diagnostics isolate unsupported OpenGL raster state (half_pixel_center and default drawable bottom_edge_rule/FlipY); the GL-clear-then-draw path additionally requires framebuffer preservation, which the retained replacement-only executor cannot provide. These are not suppressed or reset to force a draw. FBO object creation succeeded but status0x8cd6 (INCOMPLETE_ATTACHMENT); attempted GL draw returned1286 (INVALID_FRAMEBUFFER_OPERATION), with no shader submission. No incomplete framebuffer counts as success.

The analytic coverage comparator matches the physical image only for one of eight explicitly bounded candidates (.5center, unflipped, strict diagonal). This supports a narrow hypothesis; a single primitive does not establish general architectural edge/sample rules or justify opening raster admission.

## Validation and safety

New E6 test:288checks native,UBSan,i386/QEMU and physically native i686 on Mini12,34 original constructor oracles. Forty-one original bounded constructor runs cover partitions, data variation and empty-last-group counterexamples. Actual archive E6 linkage also passes. Gallium opt-in/negative/source-journal tests and actual archive tests are preserved in gallium-evidence. Actual Mesa25.2.0 static/DRI builds and11 archive suites passed after integration; newer focused GL/precision/viewport test logs are retained separately.

No previous test count is represented as newly executed. Physical submission count this sprint is one. GPU tests used the existing exact diagnostic module; no new kernel/UAPI variant was installed. Runtime gates opened only during the authorized one-case window, then returnedN. All28 tracked gate files and ADMISSION remain unchanged. Stock next boot/config content/mode restored, no automatic retry; private module remains loaded until next boot. Current boot is consumed and must not be submitted again.

The current-archive dynamic frontend bundle was also executed natively on the physical Mini12 CPU (i686), with its own isolated ELF loader/libraries and no installation or GPU device access. Exit0 confirmed context creation, actual GL clear1024green, minimal source100ES compilation/linkage, and expected draw/context/FBO rejections. Both runtime gates remainedN. See native-cpu/GL_NATIVE_RESULT.json. The standalone static GL frontend exposed unresolved static threading/lifetime crashes under QEMU; those failed attempts are preserved and are not counted as passing builds. The static driver archive and the separate physical renderer client are different artifacts and passed their reported checks.

## Next concrete work

Qualify the OpenGL pixel-center/edge/origin contract for one exact triangle through an independent coverage oracle and a fresh bounded physical case; then admit only that demonstrated laboratory profile. Preserve replacement-only rejection after glClear until load/preserve semantics exist. In parallel recover event E0 bits1/2 relative to proven E6 and remaining auxiliaryUSE; literals alone cannot establish those instructions. Do not reopen first-PC speculation or transport vtables.

Primary evidence: bound-vertex-hardware/ACTUAL-VERIFICATION.json, BOUND_SOURCE_DELIVERY.json, actual-mini12/, RESTORE_BOOT.json; events-evidence/class17-parameter/; Mesa build directories; TEST_RESULTS.json and MANIFEST.json.
