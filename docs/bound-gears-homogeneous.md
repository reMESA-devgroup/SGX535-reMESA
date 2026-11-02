# Bound Gallium homogeneous scene and shared fence

`remesa_context_prepare_gears_homogeneous_scene` connects the common state15
scene to actual bound Gallium/NIR inputs. The caller supplies banks and clear
depth; `options.state` must be empty. ISP is derived from the bound opaque
blend and depth/stencil states. MATRIX_WINDOW values come from the bound
viewport and typed Z24 contract. Attachments, dimensions, linkage, constant
block and raw position/normal/index inputs come from their existing owners.

This prevents a manual viewport or ISP option from disagreeing with the live
context. Missing bindings, invalid viewport state and manual overrides are
rejected before resource IO. The existing snapshot accepts its bounded
negative-Y window convention, including subviewports and supported near/far
intervals; other conventions remain unsupported.

`remesa_context_submit_gears_homogeneous_scene` uses the same submission
implementation as the older scene APIs. COLOR, Z and the context share one
fence. Definite NOT_ISSUED rolls back reservations and permits a retry.
Uncertain allocation, dispatch or receipt poisons both attachments and the
context, preserving backend retention and suppressing resubmission. Writers,
flush and teardown follow the existing retirement/publication contract.

The new path copies raw P/N bits and initializes unused fetch padding. It
performs no CPU matrix, vertex, lighting or pixel rendering. It does not change
normal draw_vbo, shader capabilities, Mesa's viewport capability, FAST or a
kernel gate. Existing native catalogues do not qualify these state15 images;
execution requires a separately matching, qualified native backend.

## Offline validation

Evidence: `/home/gama/sgx535-offline/bound-gears-homogeneous-20261102/v1`.

- Actual Mesa/NIR i486 tests cover four dimensions, user/resource indices,
  constant-buffer snapshots, alias/mapping errors and construction failures.
- Twenty-seven submission cases cover bound-state selection, missing/invalid
  state, manual overrides, depth disabled, subviewport/depth interval,
  definite nonissue, uncertain receipts, one shared fence, waits, flush and
  teardown. The current screen and fixture also pass UBSan.
- The old bound preparation/submission tests and 26 linked frontend cases
  pass. Four old bound captures remain byte-identical.
- Four new bound captures pass the current retained kernel validators in
  native, UBSan and i486 models. Hardware completion and retirement callbacks
  are modeled. This does not provide new physical qualification.
- Builds without Mesa retain ENOTSUP stubs for the new APIs.

All scene checks remain separate from normal FAST rendering. Hardware testing
stays stopped during the thunderstorm. Complete OpenGL 2.1/GLES2 admission,
normal draw routing, continuous production execution, clipping/interpolation
and broader shader/texture coverage still require implementation and validation.
