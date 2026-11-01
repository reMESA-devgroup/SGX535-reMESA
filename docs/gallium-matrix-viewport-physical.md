# Clip positions and matrix viewport: physical negative result

The bound Gallium pipeline already renders window-coordinate inputs on the
SGX535 rev121. The next experiment tested whether the existing six serialized
viewport fields would transform clip positions with the same POSITION
passthrough program. They did not produce the expected triangle in this scene.

The test used source `f8b2db4`, the real Gallium/NIR/Linux memory client, and
closed LAB profile 9 at 1280×800. Only the three raw POSITION values and six
viewport floats changed from the qualified profile 7 graph. Packet, placement,
CODE, lighting and all other images stayed identical. No CPU transformation or
rasterization supplied the result. Profiles 1–8 were preserved byte for byte.

The original oracle was pinned before launch and rejected the result:

- The SGX identity was core `0x01130000`, revision `0x00010201`, with PDS base
  `0x20000000`.
- The real submission completed at phase 9, ledger 7, stages 15, with operation
  and cookie 1, error 0 and no retained failure.
- All 12 immutable inputs, three source/CPU-only buffers and four guards stayed
  intact. The exact seven remaining objects were verified and cleaned up.
- All 1,024,000 COLOR words were zero and all defined Z24 values were
  `0xffffff`. The expected 128,000 foreground pixels failed both COLOR and Z.
- The executed raster/TA controls matched the prelaunch oracle. The source
  closed, all seven controls returned to N and stock recovery was verified.

This is a rendering failure despite successful retirement. The post-failure
integrity audit does not override the pixel rejection or qualify the scene.
The 640×640 candidate was not executed after this failure. No normal draw or
capability gate was widened, and the working GLXGEARS path was untouched.

Evidence is under
`/home/gama/sgx535-offline/gallium-matrix-viewport-20261101/v1/`:
`FINAL_PHYSICAL_RESULT.json`, `PIXEL_FAILURE.json`, `AUDIT_FAILURE.stderr`,
`FAILURE_INTEGRITY_AUDIT.json`, `INPUT_CONTRACT_AUDIT.json`,
`PRELAUNCH_VERIFIERS.json`, and `hardware/physical-evidence.tar`.

The consumed GPU boot is `e82a3bfa-0dc9-4422-94b0-31447aab4ce0`; never rearm it.
The verified stock return is `3fc0bf39-25ab-4441-b72d-167351cb53e7`, with the
original stock module, saved stock entry, empty next entry and restored GRUB
custom configuration. No operation remains pending.

## Next integration boundary

The current screen declares `viewport_transform_lowered=true`. The pinned Mesa
25.2 documentation says that this requires `nir_lower_viewport_transform`;
the selected WHITE_LAMBERT3 vertex program used here passes POSITION through.
The test directly constructed that NIR shader and did not run viewport lowering.
This is evidence of a missing coordinate/export integration contract, rather
than evidence that the validated ISP, PDS, USSE or state encoders need replacing.
It does not yet identify the hardware interpretation of the six DATA fields.

The next source audit must trace that existing Mesa lowering and the existing
parameter upload and arithmetic encoders into executable vertex output. Keep
the qualified window-input route intact. Do not claim a hardware viewport
transform from serialized matrix bytes. If another physical isolation test is
needed, hold window-coordinate vertices fixed and change only the viewport
DATA on a separately authorized fresh boot; do not repeat this failed clip
scene or relax its oracle.
