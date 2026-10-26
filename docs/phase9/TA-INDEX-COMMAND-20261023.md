# Pending indices and original TA index-command encoder

Status: IMPLEMENTED_OFFLINE / CONFIRMED_CPU. No new GPU execution. Physical depth and glxgears remain unqualified by this increment.

## Recovered producer/consumer

Binary psb_dri.so SHA256 `74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8`; ELF addresses below equal file offsets; existing Ghidra bias +0x10000.

Original27fa0 receives render, primitive class, source indices and input count. Triangle-list class0 preserves count. Its helper27e08 reads low16bits from successive 4byte source records, adds render+5c4->field0c, writes u16 indices. Strip/fan classes0c000000/10000000 select separate helpers and normalize to triangle class0; those topologies are not implemented here.

| Field | CPU producer/consumer meaning | Exact evidence |
|---|---|---|
| render+5b0 | current normalized primitive class | compare28015; flush28020; store2802e; header3b95f |
| +5b4 | accumulated pending indices | add2825f; comparison27f28; reset27f77 |
| +5b8 | count embedded when command first created | store28111; comparison27f28; reset27f81 |
| +5bc | CPU pointer to already-created command header | 28166 outparam to3b890; write3b974; patch27f69; reset27f8b |
| +5c0 | first-index cursor / batch start | store28108; reference28181; advance27f71 |

Contiguous append is asserted at281c9: current pending count + start must equal newly allocated start. When pending count exceeds initial count,27f03 patches only the earlier header to81400000|class|pending. It then advances cursor by pending and clears three pending fields. Six whole-original finalizer executions verify equal/increased/absent-header cases.

**Ordering matters:**28712 calls27f03 at28735 BEFORE replacing front/back ISP words and marking group1dirty. Four whole-original setter executions use existing original ISP results for LESS/ALWAYS × write0/1. Pending indices3→6 close to header81400006, cursor7→13, pending fields reset; only then are new ISP words written. This refutes treating a mutable ISP delta plus one combined index count as equivalent to separate state/draw pairs.

## Twenty-byte command

Whole original3b890 + original3b430 topology/count validation + original37854 relocation arithmetic/record writer replayed. Only arena allocation/commit and BO handle services are private CPU substitutions, logged individually. Eight independent raw-descriptor ABI cases vary count, first index and both descriptor size domains.

| DWORD | Expression demonstrated in original x86 |
|---|---|
|0|81400000 OR primitive_class OR index_count (3b95f..3b969)|
|1|index_BO_gpu_base + index_descriptor.byte_offset + 2*first_index (3b9bf..3b9db; maskffffffff/right0/left0)|
|2|arg6 (3b9e0); caller28188 passes0; new encoder only supports0|
|3|(descriptorB.BO_gpu_base+descriptorB.byte_offset)>>4 (3ba26..3ba46; mask0fffffff/right4/left0)|
|4|`((B.field0c*4+15)>>4)&63` OR `((B.field38+3)>>2)<<25` OR `((A.field10*4+15)<<4)&0x1ff00` (3ba4b..3ba83)|

A=render+5c4, B=render+3f4. These are different descriptors. B's cached producer path29e39..29f2b selects/creates it using40355/4252d. B.field0c is PDS DATA count. Architectural meanings of A.field10 and B.field38 are UNKNOWN; raw names deliberately retained. No count is called PDS CODE size.

The encoder implements class0/u16/word2zero only for the original40355 single-DMA branch: DATA12DWORDs, A.field10=B.field38 in1..16, complete primary-vertex PDS object>=64bytes (48DATA+16CODE). The separately composed287ff state-upload PDS60 is rejected and must never be used as this reference. Five original DMA-planner executions and descriptor store slices confirm the relationship:40727..40730 copies A.field10 to B.field38;4066f..4067b writes A.field10*4 to PDS DATAword8.381a0 selects exactly one DMA for N<=16.

The encoder rejects truncating field values, validates the two referenced byte spans and address alignment, rejects overlapping live index/PDS ranges, snapshots request for legal alias, and commits output only on success. Positive errno matches existing modules. Pointer arithmetic wrap is rejected before memory access; host mapping validity, GPU ownership/pinning/cache visibility/retirement and fetched vertex-index bounds remain mandatory provider responsibilities. A raw serialization API cannot authenticate a BO or admit GPU execution.

## Independent carrier comparison

Existing physically qualified carrier role2+68 contains `81400006 20004344 00000000 02000428 02000103`; +84 contains `81400003 20000200 00000000 02000422 04000203`. Independently generated encodings match all40bytes. Packed command fields alone constrain only equivalence classes. Independent PDS DATA resolves N here: role0+4280 word8=0x10/control80600003 and role0+4220 word8=0x20/control80e00007. The original two stores/planner independently establish N4 and8 and the equality of the descriptor fields in the40355 profile. This corroborates CPU construction, not physical ISA semantics. No carrier bytes occur in production source.

## Validation

55 focused checks in native, native UBSan, i386/QEMU and i386 UBSan/QEMU. Eight whole-original raw ABI constructors (four accepted40355 profile cases, four unsupported packing-only cases), six original pending closes, four original setters and five DMA/descriptor cases. Per variant,80bytes from accepted original constructors +40bytes from two independent carrier commands compared by semantic tuple. The Mesa qualifier and generated Meson driver source lists include the new module; parent-built Mesa25.2 static archive and DRI completed, and the final actual-archive i386/QEMU test passed55checks. Exact parent receipts and hashes are retained in mesa-receipt/.

No SSH, MMIO, kick, GPU submission, gate or ADMISSION change by this increment. No physical depth claim.

## Remaining exact integration boundary

The state-update8byte source, auxiliary USE16, PDS60, TA state-reference8 and now TA index20 encoders exist. A complete depth scene must bind owned vertex/index/PDS objects, serialize ordered state/reference + index commands for each draw, retain all referenced BOs until retirement, and prove full scene initialization and final state/termination. The current module deliberately does not submit or manufacture the rest of a job.40355 descriptor provenance is now independently corroborated by its DATAword8 and DMA controls; B is the primary vertex PDS object, not the ISP state-upload PDS. The next CPU point is TA close2a46b and scene stream allocation/commit boundaries: close the ordered state-reference/index stream and prove vertex-source/index ownership before any new physical experiment.
