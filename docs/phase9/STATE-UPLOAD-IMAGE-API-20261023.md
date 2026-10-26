# Reusable state-upload PDS image codec

Status: IMPLEMENTED_OFFLINE. Physical depth and unrestricted GLX draws remain
unsupported. This change does not alter admission, protected gates or submission.

`remesa_pds_structure_encode_state_image()` promotes the previously verified
`pds_primary_image --state-image` composition into the existing driver library.
The CLI now calls that API, rather than keeping a second implementation.
It emits 48 DATA bytes and 12 CODE bytes using the existing DMA, transfer-pair,
pair-single and contractual AF codecs. It accepts no historical CODE input.

The request distinguishes the source STATE allocation, byte offset and extent
from the auxiliary USE lower19 relocation. The auxiliary must correspond to the
source DWORD count; it is not the fragment shader. The existing
`auxiliary_usse_image --state N` constructs it through the typed USSE codecs.
Ownership, bank assignment, visibility and retirement remain the provider's
responsibility. A structurally valid request does not authenticate those facts.

Only the original single-descriptor CPU profile, 1..16 source DWORDs, is emitted.
This does not validate ISP/MTE payload contents or admit a one-word TA state
update. The separate TA reference codec has its own source-count restrictions.
Address arithmetic is checked before truncation. Invalid lengths, overflowing
extents, unaligned addresses, insufficient backing, overlapping buffers and
unknown counts fail without modifying either output or the written length.

Original psb_dri.so SHA256:
`74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8`.
ELF28ef1 writes the source relocation; 28ef6 writes the independently derived
DMA control; 28fa4 writes the distinct cached auxiliary relocation with
background0x200000. ELF28fba..28fdb supplies the already implemented CODE grammar.
Ghidra addresses add0x10000. ELF28301 only allocates CPU metadata; the USE
emission is the following existing 30e22/30a09/finalizer path.

CONFIRMED_CPU: DATA cells0..2 and all CODE bytes are determined by those rules.
Cells3..11 retain zero known masks. They are initialized to zero by host policy;
the observed zero values are not assigned architectural semantics. AF remains
a demonstrated CPU trailer contract, without a new physical termination claim.

Validation: three independent retained PDS objects match all60 bytes, and their
generated USE auxiliaries match all16 bytes. Explicit masks remain unchanged
despite full-byte agreement. Four CPU variants (native, UBSan, i386/QEMU and
i386/UBSan) pass the new extent/alias/transaction tests. Each variant also
preserves32 prior CLI results (state/vertex counts1..16) and matches four original
x86 auxiliary USE oracles (N2/3/11/14). The actual Mesa archive test and static/DRI
build pass. The first focused harness had a link failure from unused USSE entry
points; function-section garbage collection, already used by the prior utility
tests, fixes the harness without stubs or production changes. Failed logs remain.

The API is compiled into Mesa; the restricted CLI consumes it. Gallium does not
automatically submit this image. Remaining depth work is a fully owned clear and
ordered multi-draw scene, source-state/cache/index ordering, Z attachment and
capture of actual completion/retirement/Z/color results. The existing one-shot
triangle profile must not be widened or reused for that purpose.

Evidence: `sgx535-offline/state-upload-image-api-20261023T003000Z/` contains the
focused results, generated components, independent matches, archive receipt,
patch, provenance and hashes. Zero new GPU submissions occurred in this increment.
