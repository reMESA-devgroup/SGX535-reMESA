# Explicit initialization before Z-clear

The optional initialized prefix composes the preserved initialization DATA11,
the existing Z-clear primitive, linked draws and scene close. The old Z-clear
constructor remains available and produces byte-identical legacy images.
The initialization record is copied from the unchanged sparse frozen input and
checked against the qualified physical ROLE0+0x4350 bytes. It is not substituted
for Z-clear and introduces no new interpretation of ISP or USSE instructions.

`remesa_persistent_initialized_prefix_prepare()` stores the additional DATA44
and PDS60 in the same 4 KiB prefix allocation. Effective initialized content is
556 bytes, versus 444 bytes for the legacy prefix. The original first 444 bytes
and every USSE instruction are unchanged. The additional PDS code is the existing
state-upload encoder's required three-instruction task, not capacity padding.
Both uploads use the same existing count-11 USE auxiliary.

The initial primitive's entire 20-byte INDEX descriptor must equal the following
Z-clear descriptor. It therefore shares the exact authenticated index, vertex
PDS and vertex DATA window. Only its distinct state upload adds a reference.
This keeps the existing seventeen GEM bindings, ten USE entries and two vertex
windows for the one-draw fixture. The parser preserves sixteen compiled draws,
with at most 53 references. Legacy streams still have their original grammar.
All syntax is checked before visitor callbacks; mismatched shared descriptors
and repeated state pointers are refused. The native state checker requires the
complete preserved initialization record, including zero fragment references,
and the matching READ backing/PDS/count-specific USE auxiliary. Fragment
validation indexes the subsequent Z-clear and draw uploads explicitly.

Closed LAB selectors 1/2 retain the prior 1280x800 and 640x640 inputs. Selectors
3/4 add the initialization predecessor at those sizes. The generator requires
both catalogues and admission asserts that its four-entry table matches the
UAPI bound. No normal FAST loop, capability advertisement, shader encoder,
canonical eleven-role probe, memory permission or retirement rule is changed.
The new stream still needs physical pixel/depth qualification.

Offline evidence is under
`/home/gama/sgx535-offline/general-scene-initial-predecessor-20261101/v1`.
It includes three producer variants with legacy and initialized exports,
byte-exact legacy comparisons, eighteen initialized producer-to-kernel replays,
twelve deep refusals, parser tests through sixteen draws, and the closed
controller's 46 cases in native/UBSan/i486 builds. Hardware callbacks in these
models do not execute a GPU. `PHYSICAL_INPUTS_INITIALIZED.json` pins only future
LAB inputs; it is not physical rendering approval.

## First initialized physical result

The initialized 1280x800 profile was submitted once on SGX535 rev121 with the
actual native backend and complete retirement/source/root closure. All 128000
white triangle pixels and 896000 zero background pixels match the analytical
oracle fixed before launch. COLOR has zero mismatches across the full plane.
The Z plane remains zero: all 1024000 defined Z24 values fail the depth oracle.
This is selected-profile COLOR success, not complete COLOR/Z rendering approval,
API conformance, glxgears FPS or sustained throughput.

The initialization predecessor therefore fixes the previous empty COLOR output.
Comparing actual prepared raster words to the preserved qualified profile
isolates register 0x480: current 0x00054f00 versus 0x00454fd0. The other difference
is the expected relocated event address. The missing compound contribution is
the already implemented caller finalizer 0x004000d0; its physical zero-to-defined
Z effect is documented in `phase9/DEPTH-LIGHTING-PHYSICAL-20261023.md`.
`remesa_depth_surface_finalize()` and `remesa_raster_control_encode()` already
support it. The general immutable controls currently select no finalization.
The next change must connect the explicit caller predicate to those existing
encoders and the native consumer, preserving the old no-finalize profiles.
No new ISP bit meaning, depth shader or geometry encoder needs to be invented.

Evidence: `PHYSICAL_RESULT.json`, `VERIFY_PHYSICAL.py`, `PIXEL_ORACLE.json` and
`PRELAUNCH_PIXEL_ORACLE.json` in the evidence directory above. Raw request,
response, trace, inputs and readbacks remain under `hardware/physical`.
The pure comparison tool is `tools/remesa/verify_initialized_triangle.py`;
it never produces or presents a software frame. A preserved real empty frame
was its negative control before the initialized launch. Foreground Z24 permits
one quantization unit; undefined X8 is ignored. The 1280x800 fixture has no
pixel-centre edge ties.

All twelve READ inputs, four guards and twenty-one mapping INFO records passed.
The seven admission controls returned to N, all memory cleanup completed, and
the machine returned to its verified stock boot with original GRUB/default.
Observed TA/raster host intervals were 1.770/14.969 ms, CPU publication flush
59.850 ms and complete cold execute ioctl 218.246 ms. These are host intervals,
not isolated GPU counters or valid-frame FPS. Diagnostics remain outside FAST.
