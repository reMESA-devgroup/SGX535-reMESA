# TA state-object reference serialization

Status IMPLEMENTED_OFFLINE. Zero new SSH or GPU operations. This adds an8byte TA reference, not a complete state-upload PDS program or depth job.

## Two DATA domains proved independently

Original psb_dri.so SHA25674ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8; Ghidra bias+10000. In serializer287ff, the first source-state allocation outputs its descriptor at frame-1c. Original allocator38762 stores that exact pointer in arena+38 (38a49), then writes BO/byteoffset/zero into its three words(38a52..38a60). Original commit384ab loads arena+38 and writes end-current into descriptor+8(384cb..384d6). This is precisely frame-14. At28e67..28e6d the committed byte count is divided by4 into output+10. That field is ISP/MTE SOURCE DATA DWORD count, not CODE size. Independently,28e60 fixes output+0c=12, the state-upload PDS DATA bank size. Six original allocator/commit/store replays with8,12,16,32,64,96 source bytes demonstrate the alias; preallocated private CPU arena and only BO-bookkeeping443d1 admitted. No shader validity or GPU claim follows from arbitrary allocator lengths.

## Complete CPU command producer

3b73b allocates8bytes and emits an original37854 relocation into word0. Its descriptor+18 comes from the source header, so bit13 contributes background bit29. The first word is:

`((effective_PDS_VA >> 4) & 0x0fffffff) | 0x40000000 | (header_bit13 ? 0x20000000 : 0)`.

Word1 is:

`0x0002a200 | ((PDS_DATA_dwords << 24) & 0xfc000000) | ceil(ISP_state_DATA_dwords / 4)`.

The raw producer requires PDS_DATA_dwords multiple4; implemented profile fixes12 from287ff. The opaque0x2a200 invariant and bit13 architectural meaning remain UNKNOWN. Original serializer bounds source DATA to24DWORDs (28c13); the implemented profile requires2..24, with zero/one rejected and larger unsupported. The separate source-DMA control still supplies the exact word count; this coarse TA count does not replace it.

Whole original3b73b plus37854 arithmetic/record writer executed24 controlled CPU ABI cases: six source counts2/3/4/5/16/24, two bit13 values, two BO offsets. Arena allocate/commit and BO lookups/bookkeeping were explicit CPU service cuts. Records confirm right4,left0,mask0x0fffffff, backgrounds40000000/60000000. These are original CPU rules, not inferred hardware semantics.

## Implemented and tested

`remesa_ta_state_reference_encode()` in existing remesa_render_target.c/h generates both words from the resolved GPU address, source DATA count and bit13 predicate. It accepts nonzero16-byte aligned32bit effective addresses, checks lengths before arithmetic and commits LE output only after success. Ownership, full object extent and relocation lifetime remain mandatory caller checks; the encoder itself does not submit or advertise depth. New test covers input and overflow rejection without output mutation.

Native, UBSan, i386/QEMU, i386/UBSan all pass. Every variant matches24 original command outputs by semantic tuple plus three independent historical carrier commands,216bytes total. Actual Mesa archive test and static/DRI rebuild pass. Prior ISP/render-target regressions are retained; no huge unchanged suite rerun.

Carrier oracles (not implementation templates): role2+60={42000438,0c02a203};+7c={42000440,0c02a204};+98={62000446,0c02a201}. Referenced PDS objects are at VA20004380/20004400/20004460. Their independently interpretable CPU DATA controls8140000a/81a0000d/80200001 encode11/14/2 source words; source pointers are VA20004350/200043bc/2000443c. The last source header is2000, corroborating the background predicate. No PDS CODE is copied into the encoder.

## Remaining source object dependency

Existing PDS CODE codec already supports the one-descriptor state-upload grammar (test_pds_state_code). The missing object join is DATA and ownership, not another CODE decoder. Original28ef1 writes the source STATE BO relocation at PDS DATA+0;28ef6 writes DMA control at+4. PDS DATA+8 has a distinct relocation28fa4 with background200000 from a per-count cached auxiliary object at render+18c+N*4. N is the source state DATA count. If absent,28c5e calls28301(render,12) to create it. This is a separate symbol from source STATE BO and must not be silently substituted with a fragment shader. Its producer/contract is the next investigation, preferably N2/3. Do not claim a complete independent state-upload object until that auxiliary dependency and deterministic bank contents are justified.

Reproduce REPLAY_ALLOCATION_ALIAS.py and REPLAY_TA_REFERENCE.py; REPRODUCE.py --output FRESH; COMPARE.py --focused FRESH --output NEW_JSON. Archive qualifier test_ta_state_reference.c. All current physical gates and ADMISSION remain unchanged.
