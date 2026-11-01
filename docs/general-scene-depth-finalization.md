# General scene depth finalization

The initialized general executor previously produced the exact 1280x800 COLOR
oracle while Z remained zero. Its raster register 0x480 was 0x00054f00; the
qualified depth profile used 0x00454fd0. This was an omitted caller contract,
not a missing ISP, USSE, PDS, DATA encoder or new geometry problem.

`remesa_persistent_finalized_targets_prepare()` retains an immutable caller
flag. Existing target preparation remains byte-identical and unfinalized.
Capture uses the existing `remesa_raster_control_encode()` and depth finalizer;
the internal 80-byte control section uses schema2/depth_finalize1. Schema1
requires zero. Invalid combinations are rejected by both metadata and bound
kernel consumers. Actual depth backing, role, complete mappings, extents,
permissions and exclusive holds are validated before the shared finalizer
contribution is applied. Backing is not promoted to DEFINED by this operation.

The pure conditional word helper was extracted from the existing validated
finalizer; neither its compound bits nor its two predicates were reinvented.
Z-clear and all GPU programs remain unchanged. Selectors1/2 preserve legacy,
3/4 preserve initialized controls and 5/6 add finalized initialized 1280x800
and 640x640 LAB profiles. Public probe requests remain144 bytes, packets2264
and compact commands448. No arbitrary caller packets, VA or MMIO are admitted.

Evidence root:
`/home/gama/sgx535-offline/general-scene-depth-finalizer-20261101/v1`.
`BYTE_COMPARISON.json` proves all12 READ images and placements identical to
initialized controls; only packet bytes320/396 change. Legacy and initialized
captures remain identical to their preserved evidence. Tests include12 actual
producer runs over native/UBSan/i486,18 actual-image kernel replays,12 deep
image/index refusals,48 controller cases per variant,25 legacy LAB controller
cases and the original x86 depth finalizer oracles on three variants. Kernel
models also compare both cpp2/4 finalization and reject mismatched schemas.
Models make no GPU claim. The first two producer attempts are preserved:
one runner environment-key error and one obsolete unfinalized-only test
expectation; neither required weakening a driver gate.

Physical validation is pending. This component does not establish complete
OpenGL/GLES, continuous rendering, FPS or whole-desktop acceleration. The
existing physical GLXGEARS path is preserved. No hashes, readbacks or other LAB
diagnostics were added to the normal FAST loop.
