# Retained COLOR reads through Gallium transfers

The device interface now has an optional `map_retired_color` /
`unmap_retired_color` pair. The persistent provider selects an actual idle
capture holding the requested BO as its published COLOR, then uses the
[explicit READ lease](retired-scene-color-read.md). It does not accept a caller
COLOR label in place of checking the frozen role, generation and ownership.
Shader, DEPTH and DPM backing cannot enter this route.

Gallium's normal texture READ transfer uses this pair for typed BGRA render
attachments. `ENODATA` alone means no retained capture owns the BO and permits
the ordinary mapper. An unissued capture instead returns `EAGAIN`; a role,
lease or ownership failure does not fall through to another mapping path.
Writes and unrelated resources keep their existing mapper and guards. A screen
with only one callback of the pair is rejected before creating resources.

The transfer remembers which lease it acquired and closes it through the
matching READ callback. It never turns that lease into an ordinary/writable
unmap. Close failure remains on the resource and context, prevents further
submissions, and reaches `remesa_present_snapshot` as an error instead of a
successful presentation snapshot. The backend retains the uncertain lease and
resources. No hash, pixel oracle, new input scan or extra write sync is added.

This connects the published capture lifetime to existing Gallium texture maps
and the presentation snapshot helper. It does not retain captures automatically
in ordinary draw dispatch or widen graphics capabilities. General native
session dispatch and dynamic input lifetime still need integration. It also
does not replace the presentation backend's copies with DMA-BUF or mmap.

## Verification

Evidence: `/home/gama/sgx535-offline/scene-color-frontend-20261102/v1`.

- `coherent/`: all current driver objects, including screen, persistent provider
  and presentation code, compile together against the actual Mesa/NIR libraries
  for the i486 target. Eight new cases cover Gallium/NIR preparation, retained
  submission, texture READ and snapshot transfer at 1280×800, 640×640, 641×479
  and 1600×768; unpublished/failing leases; paired callback validation; subrect
  access and exclusion; and repeated provider submission. The original 26
  context cases and linked-vertex context regression pass too.
- `device-hooks/`: nine cases across native, UBSan and static i486/QEMU check
  exact COLOR selection, shader/DEPTH/DPM refusal, read-only unmapping and
  retention after map/unmap uncertainty.
- `present-regression-v2/`: six existing XCB/presentation/scale mock regressions
  pass across the same three variants, without contacting a server.

Native completion receipts and GEM/CPU memory are modeled. The transfer tests
copy the fake allocation's initial bytes and preserve destination row padding;
those bytes are not rendered GPU pixels or physical oracles. No hardware, FPS,
continuous OpenGL or GL2.1/GLES2 approval follows from these tests.

Preserved build failures document an initial link command that omitted actual
NIR libraries and the presentation source, and an incorrect generated-header
include root in the focused presentation fixture. The final coherent build
recompiles current driver sources and links only the unchanged generic Mesa
archives. No old reMESA archive or replacement NIR stubs are mixed in.
