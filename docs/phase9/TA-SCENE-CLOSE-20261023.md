# TA close: committed bytes and final-state DATA

IMPLEMENTED_OFFLINE; CONFIRMED_CPU. No new physical experiment.

Original binary psb_dri.so SHA25674ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8. ELF addresses equal file offsets; Ghidra image bias+10000.

## Exact producer and caller

G0003a46b is ELF/file2a46b, inside original function2a39a. Original caller2673d calls3f492 (CPU vertex reservation/cache handling), passes `!arg1` and context+13c78 to2a39a at26786, then clears context+13c78. No callback or PDS ISA investigation was needed.

2a39a optionally patches an earlier stream word at render+5cc with004000d0 when arg1 and context+13ae8 are nonzero. That earlier-word modification is outside the new trailer encoder. All callers must account for their earlier scene state; the new API is not a replacement for the whole original function.

Then the original:

1. Calls27f03 at2a3d1 to close pending indices.
2. Releases its existing reserved budget of12bytes/one relocation through382ce at2a3ff.
3. Emits the8byte TA reference to render+57c through3b73b at2a41f.
4. Calls38762 at2a45e requesting8bytes, zero relocations, alignment4.
5. Writes only DWORDc0000000 at2a46b.
6. Advances the cursor by4 at2a471 and passes that end to384ab at2a47b.
7. Continues toward CPU transport37b51 at2a50e. Replays stop at2a480, before transport, never executing submission machinery.

**New confirmed distinction:** allocator reservation8 is not serialized extent8. The tail commits4 and the final reference+literal stream commits12 total. No padding DWORD is generated or read from a historical template. Four whole original close-prefix executions use actual original27f03/382ce/3b73b/37854/38762/384ab; only BO lookup/bookkeeping services are private CPU stubs. Two PDS addresses and pending index counts0/6 are varied. All preserve a20byte canary beyond the12byte result.

The new `remesa_ta_scene_close_encode()` reuses `remesa_ta_state_reference_encode()` with source DATAcount2/headerbit13, then emits the invariant original CPU literal. It accepts only a bounded/aligned PDS BO reference with at least60bytes of object extent. This object must be the final-state-upload PDS48DATA+12CODE, not the primary vertex PDS48DATA+16CODE consumed by the index command. BO ownership, source contents, pinning, visibility and retirement are provider obligations. Output bytes and bounds are proved offline; GPU completion semantics are not.

## Producer of render+57c

Original2a57b zero-initializes a local state record, writes header2000 at2a5c0, and constructs:

`extent = ceil(width/16)<<16 | ceil(height/16)`

at2a5ca..2a5e7, using scene+0c width and scene+10 height. It passes that record/header to287ff at2a60e, with output descriptor render+57c. The real serializer emits exactly8source DATAbytes `{2000,extent}` and the real384ab commits8. Seven executions cover1×1,16×16,17×33,32×32,64×64,1280×800,2048×2048. Replays stop at28c10, before auxiliary USE or PDS generation. An initial harness stop28c0f was corrected to actual instruction boundary28c10; no result from the failed attempt is used.

The new `remesa_ta_scene_close_data_encode()` emits those8bytes for dimensions1..2048. Bounds are an intentionally restricted CPU profile, not a GPU size declaration. Each test also checks equality with the existing render-target `termination_extent`, preserving ceil rounding. Raster-prefix floor rounding remains separate.

## Independent comparisons and limitations

Historical physical carrier role0+443c contains `{2000,20002}`, matching32×32. Its separately generated state-upload PDS is atrole0+4460. Existing role2+98..a3 contains the8byte TA reference followed byc0000000; all12bytes match the new command encoder. Historical bytes are oracles only and do not appear as implementation payloads.

CONFIRMED_CPU: exact DATA construction, header predicate, two-word source count, reference relocation, pending-index closure, 8reserved/4committed,12byte stream extent.

UNKNOWN: architectural interpretation ofc0000000, whether this literal alone causes hardware completion, physical depth and full reconstructed scene execution. No PDS opcode, first-PC or AF rule is changed.

## Validation and integration

64 new focused checks and55 unchanged index checks in native, native UBSan, i386/QEMU and i386 UBSan/QEMU. Per variant80bytes compared with original producers plus20bytes compared with the independent carrier. Actual Mesa25.2 static/DRI build passed, and actual-archive i386/QEMU tests passed64close and55index checks; logs and freshness receipts are retained in archive-0/ and archive-1/. ADMISSION and28gates unchanged against the retained baseline; no SSH/MMIO/GPU operations.

## Remaining direct dependency

The next owned-object join needs vertex source descriptorA and primary vertex PDS descriptorB, separate from ISP/final-state upload PDS. Original3f610 is the producer ofA: cdecl(owner,strideDWORDs,vertex_count,&CPUwriteptr,&descriptorA). In its preallocated branch3f7f0..3f839 it returns a CPU write cursor, reserves stride*count DWORDs, stores BO via450a7, byteoffset0, byte-end extent, first-vertex bias, strideDWORDs and BO generation.29dd9 bindsA and selects/createsB through40355. The scene builder must retain the owned VB/IB and primary PDS, serialize ordered state-reference/index commands, then append this final-state reference/literal. These serializers do not supply a full TA job, Z attachment or admission.
