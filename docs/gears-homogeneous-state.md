# Reusing the physically witnessed homogeneous viewport state

The original 2026-10-24 W=2 viewport experiment already passed physically:
`/home/gama/sgx535-offline/native1280-gears-viewport-mode-20261024T205600Z/PHYSICAL_RESULT.md`.
It used the existing gears CODE, matrix and six viewport values, with the
original serializer's final `group1000=0`. That selected state contains
15 DWORDs. The preceding 14-DWORD attempt failed; both results are retained.
This is a bounded positive witness, not general clipping or OpenGL conformance.

The common persistent owner now has a separate constructor and snapshot:
`remesa_persistent_gears_homogeneous_state_prepare` and
`remesa_persistent_gears_homogeneous_state_snapshot`. They reuse the existing
ISP, fragment-binding, viewport, scalar, state-copy USE, PDS and TA encoders.
The constructor requires the owned gears PDS and `MATRIX_WINDOW` viewport.

The state has header `0x1f41` and a final zero group1000 word. Effective state
consumption is 60 bytes; its aligned source capacity is 64. The existing owner
derives MOVE count14, tail index15, PDS DMA15 and four reserved 16-byte TA
units. The USE remains the required 16-byte copy/tail program. No instruction
was added to fill space. Source padding stays zero.

The new owner has a distinct internal kind. Legacy linked/gears snapshots
reject it, and existing body/capture constructors still refuse it. This keeps
old physical profiles byte-identical while the new mode's complete closure is
integrated. No kernel ioctl, shader gate, capability or ordinary draw routing
was enabled by this change.

Offline tests cover 1280×800, 640×640, 641×479 and 1600×768. The original
physical proof bundle and uploaded source images were checked before extracting
the state/USE references. New USE bytes match the original state15 oracle;
viewport/scalar words match the retained physical input. Tests also check
PDS consumption, TA capacity, old/new kind separation, dependency lifetime,
no-I/O snapshots and refusal by old body construction. Actual Mesa/NIR i486,
native and UBSan tests include legacy regressions.

Evidence: `/home/gama/sgx535-offline/gears-homogeneous-state-20261102/v1`.
The initial test compile missed the existing decoder's explicit DWORD-count
argument; its log is preserved and the corrected build passed. No new GPU
operation was performed: physical testing stopped during the thunderstorm.

Next integrate this distinct state mode into the existing body/TA ownership
closure and kernel validation with exact 15-DWORD source/PDS/USE agreement.
Then connect its coordinate contract to normal Gallium submission. Reuse the
W=2 witness instead of inventing software viewport math or another shader.
Keep general reciprocal-W interpolation, clipping and API coverage explicitly
unproved until their respective contracts and physical tests pass.

The capability decision must follow that integration: the current
`viewport_transform_lowered=true` asks Mesa to insert viewport arithmetic.
The witnessed homogeneous state is a candidate hardware coordinate path, so
its eventual screen contract must avoid applying the transform twice. Trace
that choice through the existing state tracker and shader recognition before
adding an arithmetic epilogue or changing the capability. The flag stays
unchanged in this preparation-only component.
