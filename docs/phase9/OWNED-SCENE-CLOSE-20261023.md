# Owned final-state preparation

IMPLEMENTED_OFFLINE. No scene is submitted or physically terminated by these APIs.

`remesa_persistent_scene_close_prepare()` composes the existing final-state DATA
codec, count2 USE auxiliary, state-upload PDS and12byte TA close command in the
existing pinned/mapped BO owner. It shares the memory preparation/readback path
with the word0-only ISP update instead of duplicating allocation or cleanup.
Both public entry points derive their addresses from the allocated VA.

Final-state DATA is `{0x2000, ceil(width/16)<<16 | ceil(height/16)}`. Its header
selects the demonstrated bit13 contribution in the TA reference. The source is
not the ISP update's `{1, ISPword0}`; appending a close literal to an ISP update
would refer to the wrong source. Snapshot APIs enforce this distinction.

Host packing remains STATE8@0, USE16@16, PDS60@32 and TA@96. For final state the
TA range is12 bytes, ending at108; the ISP update's TA range remains8 bytes.
Gaps and the rest of the page are zero-filled host storage. A command stream
must copy the reported TA range, never those gaps or the backing page.
Original reserve8/commit4 behavior emits only the final4byte CPU command after
the reference, without another padding DWORD. No new hardware interpretation
of that literal is claimed.

`remesa_persistent_scene_close_snapshot()` returns a distinct result type and
refuses an ISP update BO. The ISP query likewise refuses a final-state BO.
Both objects reject public write mapping and the color diagnostic submit.
Preparation authenticates local ownership and verifies provider CPU bytes;
it does not acquire a USE bank, prove instruction visibility or supply a fence.
Uncertain leases/publication retain quarantine as before.

Validation extends the existing persistent regression with final DATA for17x33,
derived PDS/TA addresses,12byte command extent, zero tail, cross-kind refusal,
write/submit rejection and invalid size handling. Native/UBSan/i386-QEMU and
i386-UBSan pass; the actual Mesa archive and static/DRI build pass. CPU models
and original x86 replays are not hardware depth validation.

Next: retain the initialized portions of owned vertex/index backing while
composing the ordered clear/draw state stream and Z attachment in a separately
qualified multi-resource profile. Original VB descriptor+8 is a reserved end,
not a valid vertex count; commit can consume only part of the reservation and
releases its temporary BO reference. The new driver must retain its own backing
through retirement rather than copy that temporary descriptor's lifetime.

The existing one-target diagnostic submit and all protected policies remain
unchanged. Physical depth and original glxgears remain unqualified. Evidence:
`sgx535-offline/owned-scene-close-20261023T020000Z/`. New GPU submissions:0.
