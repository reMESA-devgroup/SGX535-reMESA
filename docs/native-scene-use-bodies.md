# Selected vertex and fragment program bodies

The existing native vertex/fragment checkers now inspect the actual held USE
bytes after matching each PDS task to an immutable entry. They retain all prior
loan, command, state, range, index and ownership checks. This is a selected
program construction whitelist, not a general ISA validator or execution gate.

`scene_use_bodies.h` checks the existing prefix POSITION body16 and packed-green
fragment8, selected three-input white-Lambert body64 and COLOR0 MOVE8. The word
predicates follow the existing typed CPU field expressions. They do not emit
instructions, copy a shader template into a render buffer, or replace any
encoder. The prefix's16 bytes include its existing export tail; the compiled
body's64 bytes include all eight existing records. Reserve144 is not CODE.
All committed bytes are checked without a hash or full-allocation comparison.

The selected white-Lambert profile fixes POSITION/NORMAL/LIGHT at numeric input
bases0/4/8 and its output/temporary bindings. Its vertex PDS already declares
three float4 attributes and an exact48-byte stride. Prefix uses one float4 and
a16-byte stride. These are numeric producer/ownership contracts; hardware
register-bank interpretation and runtime fetch semantics are separate. The
compiled COLOR0 fragment shares the exact ordinary MOVE8 constructor used by
the background, while its PDS separately fixes input COLOR0.

No class3 parameter source is present in these selected bodies. The larger
gears/MVP profile remains separate and rejected by the existing selected-resource
planner until its actual parameter upload exists. This work does not replace
matrices with CPU transforms, advertise uniform support or narrow the final
GLES2/GL2.1 objective to this profile. Broader shader compilation and parameter
bindings remain required for that objective.

Kernel readers use the same job-held READ/cache1 windows already validated by
the USE plan. A borrowed CPU view is now required for each selected program.
Actual bytes must match before the final vertex/fragment result is committed;
errors preserve that final output. Prerequisite scratch may change. No new
allocation, publication flush, synchronization, wait, pixel readback/hash,
MMIO operation or source admission occurs. The canonical eleven-role executor
and all production shader/PDS/geometry encoders are unchanged.

Evidence root:
`/home/gama/sgx535-offline/native-scene-use-bodies-20261101/v1`.
Pure tests use the unchanged actual prefix candidate, white-Lambert, constant
fragment and input-color encoders. The unchanged numeric `remesa_constant_rgba32`
function is extracted with source pinning for a standalone test build; there is
no NIR or rendering stub. The compiled64-byte body also matches the independent preserved original-compiler
`full-original-use64.bin` oracle. No binary analysis or physical operation is
rerun. All768 single-bit mutations across96 effective bytes
reject. Tests also cover reserve versus effective length, short inputs and a
different otherwise-supported attribute mapping. Native, UBSan and static i486
runs are CPU evidence, not fresh physical shader validation.

The17-resource scene model retains four project dimensions, ten USE entries,
all existing negative cases and publication checks, and adds actual selected
body fixtures plus per-byte tampering with transactional-result checks. The
LAB controller and full i486 kernel build are separate regression evidence.
Stack-usage recompilation checks executable-section identity with the normal
objects; it is not a complete future executor/caller/IRQ stack proof.

The remaining integration is consumption of the existing ISP/DATA and other
validated components by the general native service. See the
[current integration audit](general-executor-integration.md); do not restart
ISP/DATA encoding or generic PDS/USSE research for this handoff.
The hardware PDS window, initialization/publication, completion, retirement and
root restoration still need bounded physical qualification. Normal Mesa
frontends/loader, full GLES2/desktopGL2.1, whole-desktop/X11 acceleration and
installation/removal remain unfinished. No advertised capability is raised.
