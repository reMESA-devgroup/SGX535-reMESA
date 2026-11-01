# Fixed event references, target DATA and auxiliary programs

The selected native scene validator now follows the preserved event DATA64 /
CODE84 profile into its held target DATA16 and two pixel-master USE programs.
It also checks the complete8-byte background auxiliary. These checks extend
construction and ownership validation; they do not admit the scene for GPU
execution or establish general OpenGL support.

`event_fetch.h` checks all known fields of the original392ab producer and the
complete84-byte fixed CODE, including code-relative indices5 and20. The original
encoder is unchanged. It returns the target address and two lower19 task links.
Only148 committed bytes are inspected, regardless of reserve400 or rounded GEM
capacity. DATA6/7/15 remain under the original unknown masks. Equality with this
fixed CPU construction is not a decoder for its general instruction semantics
or a measurement of GPU fetch extent.

`scene_auxiliary.h` checks the original ordinary background MOVE8, selected
BGRA8 transfer mode0 USE16, and original event-end USE8 arithmetic. All bytes
of these selected constructors are checked. Alignment gaps are not CODE.
Other transfer modes are not admitted by this BGRA8 contract. This is fixed
program identity, not arbitrary shader validation or a new runtime result.

The event's source must resolve completely within a held READ/cache1 backing
and borrowed CPU view. Its16-byte target descriptor must match the retained
COLOR address, pitch and height from the validated controls. The event extent
uses pitch as width; background uses logical width. Full COLOR allocation and
permissions are already checked by the controls plan. No COLOR pixels are read.
The remainder of the target owner's72-byte storage is not declared consumed by
this event DATA reference.

Both task links must match immutable pixel-master USE windows with exact16/8
committed bytes and borrowed views. The actual auxiliary bytes are checked.
The target, event and auxiliary ranges cannot overlap. A range spanning adjacent
held objects is refused even if their virtual addresses happen to be contiguous.
The event starts on a32-byte boundary and has exactly148 committed bytes.

`remesa_gem_scene_event_check_locked()` retains all preceding private-loan,
actions, controls, USE, actual TA, state-upload, vertex, fragment and background
checks. Endpoint/MMU locks, the private root and disjoint retained scratch/output
are caller requirements. Large results are built after prerequisite checks with
explicit non-inlining. The final result is transactional; prerequisite scratch
may change. There is no allocation, extra flush/wait, pixel readback/hash, new
MMIO operation, source admission or execution. The fixed eleven-role executor,
original producers and existing gates remain unchanged.

Evidence root:
`/home/gama/sgx535-offline/native-scene-event-closure-20261101/v1`.
Pure tests compare unchanged original DATA/CODE, render-target and USE encoders,
80 relocation combinations, four project dimensions, every fixed CODE byte,
known DATA fields, arbitrary masked fields and full auxiliary-byte tampering.
The independent84-byte CODE oracle is read from the preserved physical profile
at role0+0x4500; its identity is pinned by the earlier event-profile receipt.
That historical comparison is not a new physical submission.

Native, UBSan and static i486 models exercise17 held objects, ten USE entries,
all four project dimensions, read-only target policy, exact COLOR/pitch,
wrong-master links, full auxiliary tampering, event/target overlap, cross-object
span refusal, transactional outputs and stability after publication. The LAB
controller and full kernel module are checked separately. Compiler stack-usage
measurements compare executable sections with the normal build. The new wrapper
uses44 bytes, its builder176 and auxiliary resolver72; the explicit source
chain through private-MMU validation sums2728 bytes. These are not all paths
or a complete caller/IRQ budget. All are offline
results; a full executor/caller/IRQ stack budget remains necessary.

Remaining work includes prefix/compiled vertex and fragment USE bodies, selected
ISP semantics and unknown-field admission, background texture/pitch runtime
qualification and the ordered general native service. Hardware PDS window,
initialization/publication, source ownership, completion, retirement and root
restoration need bounded physical validation before admission. Normal Mesa
frontends/loader, whole-desktop/X11 integration, full GLES2/desktopGL2.1 and
installation/removal remain unfinished. No capability bit or API version is
raised by these construction checks.
