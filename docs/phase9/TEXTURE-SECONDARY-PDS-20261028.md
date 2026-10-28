# Original constant-loader secondary PDS profile

The userspace PDS constructor now includes a selected original SGX535/rev121
constant-loader profile. It reuses the existing typed instruction encoders and
preserves the earlier ISP/PARAM/vertex constructors. This is CPU construction
and staging support, not physical upload, textured rendering or desktop
acceleration. Full general3D/2D and installable-driver integration remain open.

Evidence root:
`/home/gama/sgx535-offline/texture-secondary-pds-20261028/v1/`.
The pinned original GLES2 library remains SHA256
`7859b0fa0cd3cd44ed18a0c056607066c07312307f8c6d86fd8e211f9a617ed0`.
REA snapshots from the earlier work were reused. The new complete evidence is
`REA_CONSTANT_LOADER.json` and `GLES2_ANALYSIS_SNAPSHOT.json`; the native
session was closed. No Mini12/GPU/boot operation occurred.

## Trace and two distinct source images

A first reference with an identical field offset proved to be a GL state
getter, not an upload consumer. It was discarded; it remains in the evidence
bundle as a negative lead. Following the actual caller of the constant gather
routine identified the real source preparation path:

- Caller Ghidra0x465d0/ELF0x365d0:
  `ev_8ee967fc21c73b6306d7efca596271d3310c8c9a070ad8044de41ab7d0c0ca75`.
- DMA descriptor helper Ghidra0x46510/ELF0x36510:
  `ev_5273ce4c028a7f8b98c62589ad2feb978d4b324545018a9b9f25e29c82cde3a0`.
- Secondary PDS writer Ghidra0x27890/ELF0x17890:
  `ev_bd62c4ca25dc3b70a24cbbcb97374987afc9256cb8859deda69cf7421b83440a`.
- Interleaved DATA extent helper Ghidra0x27750/ELF0x17750:
  `ev_61c313c4311c16eed0b7295d38e977ede8dacdbaa36bf076d927ac7d6aaadc2d`.

The caller first invokes the previous literal/texture gather. It then allocates
and builds a **second** source image. Depending on compiler metadata, the
second image contains a pointer into the first, other source pointers, direct
literal patches and direct texture triples. The PDS DMA helper receives the
second image's address and committed word count. Therefore the previous
[constant/texture image](TEXTURE-SA-UPLOAD-20261028.md) must not automatically
be supplied directly as the new PDS source merely because both involve texture
constants. The existing generic image builder can pack bounded direct values;
the two allocations and their relocation/lifetime graph still need binding.

There is also a separate attribute-allocation requirement: the caller retains
the maximum of compiler-required attribute words and actually committed source
words, while the DMA helper receives the latter. The reserved source capacity,
effective source write, attribute allocation and program extent are separate
quantities. Hardware SA destination/base and exact DMA consumption still need
physical qualification.

## Controlled original replay

`replay_pds.c` maps only the original RX segment after exact file/ELF/hash checks
and calls the pure descriptor helper and PDS writer. No constructor, GL setup,
GPU allocator, MMIO, ioctl or shader executes. Guest alarm5seconds and parent
timeout15seconds bound the run. Original DATA is initialized to a sentinel so
actual CPU writes remain distinguishable from untouched padding.

The1152 cases cover source counts1..128, three addresses (including a backing
ending exactly at2^32) and three lower19 task-fixup values. Outputs, descriptor
counts, interleaved DATA size, CODE and untouched tail are checked.
`BUILD.json`, `REPLAY.json`, raw jsonl/strace and `VERIFY.json` retain evidence.
All original cases match the independently authored field model. These are CPU
observations, not a GPU fetch-extent or instruction-retirement trace.

The source-descriptor controls differ from the preserved PARAM profile. Counts
up to16 use count-1. For larger counts, the observed table selects the largest
divisor from16 down to2 with at most16 rows; otherwise it selects16 and emits
a remainder descriptor. The latter gets a source offset and destination offset
contribution. Neither the single-row control nor the remainder gets PARAM's
high control bits. The validated older control helper was not changed.

For the selected fallback-task profile the writer produces:

- DATA48 with one/two source-address/control pairs, the canonical lower19 task
  value, and the other two task values explicitly zero.
- One/two existing CPU_TRANSFER_PAIR instructions, existing PAIR_SINGLE(2/4,0),
  and the existing AF trailer: CODE12/16, total effective program60/64 bytes.

The real original caller chooses a corresponding default USE auxiliary for
this fallback profile. Its body, retained allocation and execution contract
must be established before submission; a syntactically valid task fixup is not
an authenticated USE binding. No auxiliary body was copied or invented here.

## Shared implementation and checks

`remesa_pds_structure_encode_constant_image` checks1..128 source words, backing
capacity, aligned/nonwrapping32-bit source range, offset, canonical fixup width,
complete host spans, aliases and capacity. Failures preserve bytes, masks and
written. Success emits only60/64 effective bytes and leaves excess capacity
untouched. Unwritten original DATA is initialized to zero with known-mask0;
these bytes are not claimed recovered or hardware-consumed. All CODE bytes
are computed by the existing typed encoders. No CODE filler is added.

The CPU CODE validator now recognizes only the exact additional secondary
sequence with source pairs0/[2], task pair2/4 and bank1 index0, then trailer.
This evidence-based CPU profile extension does not relax a GPU or API admission
gate. Unknown sequences, operands and profiles remain rejected.

`qualification/RESULT.json` records, in each native/UBSan/i386 variant:
78 transactional/range/padding checks,70 prior task-output checks and2304
program comparisons (the1152 original cases at source offsets0 and4). Known
bytes/masks match actual original writes; unknown DATA must stay mask0 and zero
by host policy. The original source buffer capacity is not substituted for the
committed count. `LEGACY_COMPARISON.json` compares192 old/new outputs and masks:
128 DMA partitions,32 ISP/PARAM images and32 vertex images, all unchanged.
All31 protected files remain unchanged. `MESA_OBJECT.json` binds the updated
unit compiled with actual Mesa flags and a fresh106-file isolated overlay.
No full new Mesa library was linked or installed; normal rendering does not
call this profile and general sampling admission remains ENOTSUP.

## Next required physical preparation

Bind the two source images, matching default or compiler-generated USE
auxiliary, explicit stage/attribute allocation and canonical retained GPU
addresses. Resolve the shader's constant/texture layout and coordinate/result
contract. Reuse the existing owned mappings and submission infrastructure;
do not conflate primary R/T PDS texture output with this constant-load route.
A controlled physical candidate must pass offline checks before testing and
retain identity, submission, completion, retirement, readback and buffer
integrity checks at required sizes. Existing gears and native2D qualification
remain preserved. Actual desktop/EGL/GBM/image ownership, general GL/GLES2,
multi-client isolation and recoverable installation remain part of completion.
