# ISP state DATA update recovery

Status: IMPLEMENTED_OFFLINE. No new GPU/SSH operation belongs to this checkpoint; the separate parent GLX hardware success is not a depth result.

## New direct evidence

Original psb_dri.so SHA-25674ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8. ELF and file offsets agree in these text ranges; Ghidra bias+0x10000.

The full original CPU producer2bfdf constructs two stored ISP triples and presence bits. At2c54b..2c590 it caches these at context+14128/+14134 and low-six-bit presence at+14124. If a render object exists,2c5b8 calls28712 with render=ctx+13c78, presence, front triple and back triple. Setter28735 first calls27f03, closing any pending index-list header before changing ISP state. It stores low-six-bit presence into render+414, front at+418..420, back at+424..42c, and marks dirty render+554 bit0. Those offsets are CPU fields, not MMIO register numbers.

Dirty consumer2934b..29365 calls28fff(render,render+414). The cache at render+4b4 compares stored presence and complete triples, suppresses equal groups, and calls287ff at29211 for nonzero pending mask. Only after successful state-object linkage2922d->3b73b does29236..29247 copy0x88bytes into the cache. Failed or merely planned uploads cannot advance this cache.

The serializer entry is287ff, not28800 (an interior instruction). At2889a it writes the selected mask as the first DATA DWORD, then presence bit0 selects word+4, bit1 selects+8, bit2 selects+0c; bits3..5 analogously select the back triple. This is actual CPU serialization, not an inferred hardware field name.

## Implemented subset and independent tests

`remesa_isp_opaque_triangle_data_encode()` in existing remesa_render_target.c/h generates LE{1,word0}, exactly8bytes, using the existing depth and color codecs. It admits LESS/ALWAYS, depth-test/write controls, and only full RGBA or color-off. It rejects unknown compares/masks and preserves output on error. Input is snapshotted before overlapping output is committed.

The selected CPU profile is triangle selector4, front orientation0x901, polygon+1e zero, stencil disabled/no stencil visual bits, no alpha/blend/logic-op, fragment metadata writing color only/noKIL/noDEPR, and context+13c84/+13c80 zero (their architectural meaning remains UNKNOWN). Its stored triples are front={word0,0,0x0e000000}, back={0,0,0x0e000000}; selected presence is1. Consequently only front word0 is serialized. This omission does NOT prove that hardware never needs the other words.

The8bytes are a STATE UPDATE/DELTA, NOT a self-contained initial state, complete MTE packet, PDS CODE or job. They do not initialize other ISP/MTE/PDS groups. Original28fff suppresses equal state; other required initial/drawable/shader state must already be established. No runtime path submits these bytes and no depth capability is advertised.

Ten bounded original-x86 cases execute full2bfdf,28712 and27f03, then the state DATA prefix of287ff through28bfc with only its arena allocation service admitted. They cover LESS/ALWAYS x write0/1 x colorfull/off plus disabled-test and absent-depth cases. Each case additionally executes three original28fff cache conditions: fresh->pending1, equal->pending0, changedword0->pending1; cache remains unmodified before actual upload. These are bounded producer inputs under the declared disabled-feature profile, not a whole shader compilation or GPU test. No PDS/USSE CODE was captured.

The final C test emits its semantic input tuple, not just an index. All ten tuples and80DATAbytes match the original producer in native, UBSan, i386/QEMU and i386/UBSan. The final actual Mesa archive test passes; static/DRI rebuild passes. Existing render-target regression passes four variants. Unchanged massive encoder/NIR suites were not rerun.

Independent physical carrier observation: role0+43bc=0x00000f41; low-six-bit ISP presence is1, next word role0+43c0=0x01d00000. The isolated delta has header1, so only the ISP subset/word0 agrees; claiming an8byte full-header match would be false. Remaining header groups are explicitly absent from this encoder.

An early replay report accidentally reused the local `mask` variable for cache pending bits and mislabeled semantic RGBA as1. Parent review caught it before sealing. Final replay uses a separate pending_mask, records correct0/15, and final differential comparison keys all five semantic fields. A subsequent dump formatting warning and a transient stale-header archive check are retained as failed attempts; corrected/rebuilt tests pass.

## Remaining exact block

The depth attachment prefix (reg480/address contributions) is separate from per-draw ISP compare/write, and neither alone creates a scene. Serializer287ff also constructs state-upload PDS and its source descriptor. 28fff then links that object through3b73b, and only afterwards updates its cache. Reproducing these lifetime/relocation/job contracts remains required for multiple draws and physical depth. The present one-triangle qualified carrier must not be patched or widened by this change.

Next producer3b73b allocates an8byte TA state-object command. Its first word is an original relocation37854 with right4,left0,mask0x0fffffff and background0x40000000 plus optional0x20000000 selected by descriptor+18 bit13. Word1 is0x0002a200 | ((descriptor.field0c<<24)&0xfc000000) | (((descriptor.field10*4+15)>>4)&0xff), and field0c must be a multiple4. Do not call field10 CODEsize without closing its provenance:28e67 derives it from the committed state-source allocation metadata. The next bounded experiment should execute the actual37854 arithmetic and vary those two fields/BO offset independently, preserving object ownership. This command is not implemented yet.

Reproduction: REPLAY_ISP_STATE.py; REPRODUCE.py --output FRESH; COMPARE_ORIGINAL.py --focused FRESH --output NEW_JSON. Archive qualifier test is test_isp_state_data.c. Use new output directories; preserve this checkpoint.
