# Kernel-bound raster and TA controls

`remesa_gem_scene_controls_prepare()` connects the retained scene action plan
to raster208/TA56 command DATA derived from actual held GEM objects. This is
part of the general native consumer, not permission to issue a scene.

The kernel-only immutable command section supplies dimensions, pitches, depth
format/clear bits, five held IDs and three committed object windows. It accepts
no register arrays, external GPU addresses, CPU pointers or CODE. The action
and control sections must be disjoint and agree on dimensions. REGION must
already have the action preparer's reservation, and the five control roles
must be distinct from each other and all three scratch roles.

The kernel validates current backing, complete mappings and exclusive holds;
COLOR/Z surface ranges and image extents; background/event placement; and
TA/background/event alignment and committed windows. TA source placement
retains the existing encoder's full address range rather than inventing a PDS
bank restriction. Background needs at least68 bytes, the selected fixed event
needs its committed148 bytes, and TA needs a DWORD-aligned stream window.
Reserved event capacity400 is not substituted for consumed object148.

The current selected profile is linear BGRA8, undefined Z of cpp2 or4, no depth
finalization, no stencil and fixed event field0c16. Clear-depth IEEE binary32
bits are checked with integers: positive finite0..1 and negative zero are
accepted, preserving the existing userspace producer. No kernel floating-point
operation is introduced. COLOR/Z initialization and actual command/program
contents still require separate validation before execution.

`scene_control_words.h` shares the exact existing register/value serialization
between Mesa and this kernel consumer. Mesa retains all original validation,
depth-prefix construction and little-endian output. The shared helpers accept
already validated inputs and do no admission or MMIO. This small extraction
does not replace the validated producer with an approximate implementation.

The bound-root check first validates the complete private loan and original
action plan, reconstructs controls from immutable commands and current held
backing, then authenticates COLOR/Z read-write and TA/background/event read-only
permissions with the expected cache policy. The small control arrays must
match exactly. No buffer hashes, framebuffer comparisons, readback, allocation,
clear, cache maintenance or additional synchronization is added here.

## Evidence and next integration

Evidence is stored in
`/home/gama/sgx535-offline/native-scene-controls-20261101/v1`.
The native/UBSan/static-i486 kernel models compare raster208/TA56 against the
actual Mesa producer at1280x800,640x640,641x479 and1600x768. They cover cpp2/4,
zero/negative zero/subnormal/one clear values, invalid floats, insufficient
image extents, immutable-section disagreement, aliases, alignment, window
overflow, permissions/cache policy, tampering and post-publication checks.
These remain CPU models with zero GPU operations.

Original independent x86 raster and TA oracles are reused from
`/home/gama/sgx535-offline/raster-control-recovery-20261023`.
An initial strict build exposed an old overlap test's undersized destination.
The test now uses a union large enough for both overlapping arguments, keeping
the same required `EINVAL`; the original compile failure is preserved.

The existing fixed eleven-role executor/source path is unchanged. A complete
native consumer still needs image/program/TA-reference validation, USE bank
ownership, proper initialization/publication, ordered source-attributed issue,
completion/retirement and restoration. These controls alone do not clear any
scene-incomplete or advertised-capability bit. Physical qualification and
normal frontend/loader/X11 integration remain necessary for the complete driver.
