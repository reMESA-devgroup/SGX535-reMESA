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

Both1280x800 and640x640 passed in separate fresh recoverable boots with the
same module. Physical evidence is in `v1/PHYSICAL_RESULT.json` and
`v2-640/PHYSICAL_RESULT.json`. Each verifies core01130000/rev00010201, one
submission, phase9/ledger7, completion/retirement, unchanged12 READ inputs,
4 guards and21 object INFO records. The unchanged prelaunch analytical oracle
finds zero COLOR and Z24 mismatches across1024000 pixels at1280 and409600 at640.
The640 case has320 exact edge ties, checked for coherent COLOR/Z coverage;
defined Z24 permits one quantization unit and ignores undefined X8. Source
closure, all seven controls N, memory cleanup and return to stock are verified.
The640 raster208/TA56 trace also matches the prelaunch output of the existing
Mesa encoder, whose output reproduces the physical1280 trace exactly.

| Size | CPU publication flush | TA completion observed | Raster completion observed | Execute ioctl |
| --- | --- | --- | --- | --- |
|1280x800|55.782303 ms|1.549080 ms|16.395381 ms|185.990970 ms|
|640x640|52.244353 ms|0.785784 ms|6.132065 ms|163.655448 ms|

These are isolated cold LAB operations. Completion intervals start before the
corresponding dispatch and include host/observation effects; they are not
hardware GPU counters, sustained throughput, a before/after performance result
or GLXGEARS FPS. The normal FAST path received no new diagnostic cost.

This component does not establish complete OpenGL/GLES, continuous rendering,
FPS or whole-desktop acceleration. The existing physical GLXGEARS path is
preserved and was not remeasured. Normal frontend/provider dispatch and its
production session/resource lifecycle remain to be integrated with this now
physically qualified general-scene path; do not reopen the completed encoders.
