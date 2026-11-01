# Fragment state references and committed PDS windows

The selected native scene validator now follows the group40 reference triple
in each validated prefix/draw state upload into the held primary and secondary
fragment PDS objects. It validates the existing encoder's known fields and CODE,
then binds the task to a retained pixel-master USE window. It does not execute
the scene or certify the USE shader body.

## Selected construction contract

`include/remesa/fragment_fetch.h` decodes the existing compressed references,
12-dword DATA size and scheduler fields. Addresses require 32-byte alignment,
complete committed ranges, no primary/secondary overlap, and a matching 256 MiB
PDS address window. The native checker currently expects `0x20000000`. This is
an explicit selected CPU contract, not an observation that the active hardware
register has that value. A future executor must establish and verify that
window under source ownership before issuing the scene.

The existing no-R primary consumes DATA48/CODE8, totaling56 bytes. The COLOR0
primary consumes DATA48/CODE12, totaling60 bytes. Both have larger reserved
capacity. The empty secondary consumes only CODE4, despite its32-byte reserve.
The checker never fills capacity with instructions or reads reserve as CODE.
Unknown DATA fields keep their original masks; they are not silently declared
zero or qualified. Some constant profiles share the same no-R reference triple;
the triple alone cannot distinguish those profiles.

## Native ownership and transaction

`remesa_gem_scene_fragments_check_locked()` first checks the existing private
loan, actions, controls, USE metadata, actual TA, state uploads and vertex/index
windows. It uses the caller's endpoint/MMU locks and bound private root. Each
primary and secondary must fit a held read-only cached backing with a borrowed
CPU view. The primary task must match an immutable pixel-master USE entry with
exactly8 committed bytes. Overlap of that task with its PDS windows rejects.
The result returns held IDs and offsets rather than trusting raw addresses.
The final result is unchanged on error; caller-provided prerequisite scratch
may be updated.

No allocation, default-root walk, MMIO, publication flush, wait, readback, hash
or execution admission is added. The canonical eleven-role service remains
separate. Existing safety/lifetime/cache checks are retained.

## Kernel stack

Actual i486 Kbuild objects were recompiled with `-fstack-usage`; all executable
sections matched the corresponding normal objects. The initial nested wrapper
chain retained about6.5 KiB of compiler-reported frames before counting deeper
MMU work, the executor/caller and interrupt requirements. That is insufficient
margin to assume safety on an8 KiB kernel stack.

TA, state, vertex and fragment validators now finish their prerequisites before
entering separate non-inlined transactional builders. Large temporary results
are no longer live while those prerequisites run. No validation or transactional
output requirement was removed. The evidence directory records before/after
compiler frames: the explicit fragment-to-private-MMU source chain sums
6528 bytes before and2640 after. These sums exclude unlisted paths and are
conservative where tail calls occur. All executable sections matched normal
builds in both measurements. This is not a complete IRQ/caller worst-case proof: the final
executor still needs an explicit stack budget, with large scratch and retained
owner state allocated before entering the MMU writer interval.

## Evidence and remaining work

Evidence directory:
`/home/gama/sgx535-offline/native-scene-fragment-fetch-20261101/v1`.
The pure constructor tests compare the unchanged original encoders for both
selected profiles, three address windows and four task fixups. A preserved
12-byte no-R reference triple from the old physical carrier is an independent
reference-decoding oracle only, not proof that its complete scene matches the
current namespace. Tests also distinguish committed bytes from reserve and
exercise alignment, overlap, unknown masks and transactional errors.

Kernel models exercise reference/primary/secondary tampering, pixel-master
identity, RW-target rejection before parsing and post-publication stability,
at1280x800,640x640,641x479 and1600x768. Native, UBSan and static i486 runs are
CPU models. Full module builds remain uninstalled and physically unqualified.

Remaining closure includes USE bodies, background/event inputs, ISP semantics
and unknown-field qualification, followed by ordered general native execution
and bounded physical validation. Normal loader/frontends, complete GLES2/GL2.1,
whole-desktop acceleration and installation are not established by this work.
