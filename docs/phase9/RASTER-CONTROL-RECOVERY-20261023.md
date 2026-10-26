# Original raster and TA control generation

IMPLEMENTED_OFFLINE / CONFIRMED_CPU. No GPU submissions, SSH, MMIO writes or kernel changes. This is control DATA for a future native job, not new physical qualification.

The driver now independently constructs the complete208-byte raster prefix and56-byte TA prefix. The LAB composer links all264 bytes to live color/Z/background/PDS/generated-stream BO metadata and checks immutable generated scene contents before returning the combined result. It does not upload these bytes or authenticate the auxiliary programs; the actual CONTROL BO remains uninitialized until an explicit staging path exists.

## CPU producer evidence

psb_dri.so SHA25674ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8. Ghidra image base0x10000. The full raster producer is ELF2ab70/Ghidra3ab70. REA evidence ev_a1d2eaa391c4c6f8ea83eb8a35cacaf4551b87f3b84cb6755acf9623ba0cce3e confirms the complete body. Selected canonical pseudocode/assembly and limitations are preserved; full high-Pcode is not claimed. The provider session was closed normally.

Eight bounded executions use the unchanged whole original raster constructor, actual37854 relocation helpers and384ab commit. Original surface constructor/status routines create Z descriptors. The actual39742..3975a post-constructor metadata stores supply descriptor fields16/0/0. Allocation, lock, BO mapping and validation services are explicit synthetic ABI cuts, not hardware or whole shader-compiler evidence. They preserve the constructor's208-byte commit and untouched tail. Cases isolate dimensions/orientation, depth presence/cpp, background offset, event offset, depth clear and stencil. No shader program words are extracted for the encoder.

| Field | Demonstrated CPU expression / source | Classification |
|---|---|---|
|410|width>>4 in high16, height>>4 in low16,2ac71..2ac85|CONFIRMED_CPU|
|41c|literal0x0da24260,2abef/2ac10 ->2acab|CONFIRMED_CPU; hardware meaning UNKNOWN|
|4c4|((effective_background_address>>4)&0x00ffffff) OR0x02000000,2af9e..2b004|CONFIRMED_CPU|
|a5c|effective event address, original right4/left4 relocation,2b06d..2b0d3|CONFIRMED_CPU for aligned input|
|a60|(event.field0c>>2)&0x3f,2b0ea..2b0fc|CONFIRMED_CPU; architectural unit UNKNOWN|
|480/cb0/484/488/48c/490|existing independent depth-prefix rules, kept in original slots10..15|CONFIRMED_CPU|
|4bc/4b8|0x200 OR low-byte stencil / float clear bits|CONFIRMED_CPU|
|Other controls|direct register/control stores of2ac41..2b106, preserved as CPU protocol constants|CONFIRMED_CPU; no architectural names invented|

REFUTED_CPU:41c is supplied by a BO relocation in this producer. This says nothing about its physical hardware interpretation. The omitted address nibble of4c4 is handled by the restricted RAST30000000 window; it is not a full32-bit byte pointer. The event address at a5c uses a distinct encoding.

The event post-constructor descriptor slice demonstrably writes field0c=16,field10=0,field14=0. This is the only profile accepted by the new API. The interprocedural connection from that producer to the exact live event descriptor remains separate from these producer/consumer observations; no event execution or DATA-load semantics are asserted. Other metadata and later constructor-finalization paths remain ENOTSUP. The current encoder covers the pre-finalization2ab70 output, not an arbitrary final scene lifecycle.

The TA producer ELF2b258 requests96 bytes but commits56. Three whole-original replays execute its actual relocation and commit, including a4-byte-aligned non16-aligned stream address. Width/height parameters are not consumed by this constructor. Six pairs are CPU protocol constants;238 receives the effective command-stream address. This is neither a PDS entry point nor a CPU pointer. The native composer derives it from the actual generated page's TA offset/length and revalidates ownership/generation/content. It does not invent arbitrary padding up to the requested96 bytes.

## Implementation and validation

remesa_raster_control_encode and remesa_ta_control_encode emit little-endian fields with object-span/aperture/alignment checks and transactional/disjoint outputs. Symbolic-reference ranges remain distinct from authenticated ownership. remesa_lab_scene_raster_encode/command composition re-snapshot live BOs, reject stale receipts and leases, and preserve output on unknown metadata or invalid ranges. The LAB integration is preparation-only and does not register a backend or change capabilities.

Native, UBSan, i386/QEMU and real Meson archive validation cover eight original208-byte oracles (926 checks), three original56-byte oracles, and the owned1280 composition (113 checks). Existing340-check storage and render-target regressions run against the final archive. Static Mesa and DRI are rebuilt. Pure render-target source tests compile their actual standalone module rather than unrelated NIR/Gallium sources; integration still links the full actual archive.

The independently replayed32x32 raster208 + TA56 match all264 bytes of the preserved physical-reference INPUT role4.bin, SHA2565809a93a974864f6a22b199b10cd6fb96c1981097d2bf9267da255d229ac30cb. This is an input-oracle comparison, not new physical execution. The new1280 raster is compared in full against the original constructor, and its TA address is derived from the owned generated stream. No historical packet enters the implementation.

## Remaining physical boundary

Control DATA is now generated; upload/readback of its owned BO, auxiliary/background/event materialization and DPM initialization remain. The event/background program contents still require separate authentication and explicit retained-LAB dependency isolation. Complete XHW/load/fire sequencing, requester coverage and the held multipage native capture consumer are not admitted by this increment. The existing exact32 executor/card and28 gates are unchanged. Depth physical and original glxgears remain unqualified. A prepared264-byte control image is not a complete physical job or OpenGL support.
