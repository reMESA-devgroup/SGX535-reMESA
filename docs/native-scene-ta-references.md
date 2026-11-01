# Actual TA bytes and retained references

The next native gate now parses the actual retained TA input rather than only
accepting numeric action/control/USE descriptions. It does not yet admit GPU
execution or certify every indirect PDS/USE/vertex fetch.

`include/remesa/ta_references.h` recognizes exactly the current linked-TA CPU
producer: Z-clear state/index prefix, one to sixteen linked state/index draw
groups, and the original state-reference/close tail. It uses the existing
copy and compiled-position/color descriptor profiles. This is a restricted
command grammar, not a general TA/PDS/USSE instruction disassembler.

All syntax, byte counts, supported descriptor fields, u16 triangle counts,
zero fields, address representation, nonwrapping spans and the exact committed
close tail are checked before any reference callback runs. An extra reserved
tail DWORD is rejected. The parser then visits at most52 ordered references.
Callback failure preserves returned counts; callers retain their own temporary
result until all resolution succeeds. Input must remain immutable throughout.

The parser deliberately reports `minimum_bytes`: state PDS60, vertex PDS64,
and exactly twice the encoded u16 index count. These describe what this
selected grammar can establish, not a generic full PDS fetch length. Actual
producer object size, reserved capacity and measured hardware consumption
remain separate facts. No CODE or padding instruction is emitted.

## Userspace graph binding

The linked TA owner now retains a CPU shadow of exactly the commands it
already emits and uploads, up to488 bytes. This does not change GPU bytes or
add a readback. Its existing reference lifetime and uncertain-upload handling
also retain/free this shadow.

`remesa_persistent_scene_ta_snapshot()` walks that immutable shadow and resolves
references against the frozen read-only footprint. It checks current identities,
the exact Z-clear owner/offsets, body draw-state membership, the corresponding
PDS/geometry chain, index windows and the actual close owner. It returns both
minimum spans and committed producer object extents. For the current graph,
the primary vertex PDS is committed64 bytes despite reservation192. Public
frontend destruction does not invalidate retained captures. No provider I/O,
allocation, readback, hash or execution/capability admission is added.

This CPU shadow is not a GPU-visibility or readback certificate. The native
kernel gate independently reads the actual retained input mapping.

## Kernel gate and validation boundary

`remesa_gem_scene_ta_check_locked()` first authenticates the complete private
loan and matching action/control/USE plans under the existing endpoint/MMU
locks and bound private root. It requires the actual TA borrowed CPU view,
parses its exact control-bound byte window, and resolves every reference to
complete job backing with READ/cache1 permission. RW destinations, CPU-only or
unmapped backing, missing TA input views and out-of-bounds spans reject.
Output is transactional; no allocation, default-PD walk, synchronization,
framebuffer/pixel readback, hash or MMIO occurs here.

The kernel result still contains minimum PDS spans. It does not know the full
typed userspace object graph or establish PDS DMA/task contents, USE instruction
validity, vertex fetch bounds, initialization or complete indirect closure.
Those checks remain necessary before a general native executor can issue.
The old fixed11-role executor/source/retirement and LAB gates stay unchanged;
no incomplete bit or advertised capability is cleared.

Evidence directory:
`/home/gama/sgx535-offline/native-scene-ta-references-20261101/v1`.
Parser tests use the unchanged original CPU encoders for1/2/16 draws and cover
malformed groups/tails, capacity, wrap/alias guards, full syntax before callbacks
and transactional counts under native/UBSan/static-i486. Mesa tests resolve the
actual seven-reference graph across existing capture slots, dimensions, legacy
and raw REGION profiles and complete public-owner release. Kernel models parse
actual borrowed TA bytes at four dimensions and reject malformed tails, RW
references, unmapped targets and missing views, including after publication.
All are offline/model evidence, not physical GPU/API qualification.

The first kernel negative test zeroed a byte already zero in the valid close
literal. Its failure is preserved; the test now zeroes the nonzero high byte.
No production gate or valid command was changed to accommodate the test.
