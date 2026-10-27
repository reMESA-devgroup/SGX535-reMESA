# Phase 9 development update — October 14, 2026

> **Historical checkpoint — October 14, 2026.** Later restricted original GLXGears hardware results are documented in the [October 27 physical status](PHYSICAL-GLX-STATUS-20261027.md). Statements below about unsupported execution describe this earlier path/checkpoint; they are not a current project-wide hardware status or general OpenGL conformance claim.

SGX535-reMESA is getting close to release. This is a development estimate;
remaining issues, validation and optimizations are still being worked on.
The previous README commit predates the current Mesa implementation. A more
detailed public update will follow.

## CONFIRMED — historical hardware results

The SGX535 rev121 diagnostic path rendered the retained triangle and square.
The FIRE3 triangle produced exactly 120 magenta pixels and 904 zero pixels,
with completion, retirement and matching readback. A subsequent diagnostic
Gallium draw also has a retained hardware receipt. These remain valuable
milestones; they do not establish execution of the current NIR shader pair.
See [triangle reproduction](../reproduction/TRIANGLE.md) and
[square reproduction](../reproduction/SQUARE.md). Existing evidence and its
hashes have not been replaced by this update.

## IMPLEMENTED / TESTED OFFLINE — production Mesa path

The driver retains restricted NIR shader semantics, immutable shader images,
owned VS/FS staging, checked USE entry plans and PDS DATA relocations. Invalid
ranges, stale tokens, conflicting ownership and incomplete execution contracts
are rejected. CPU transfer and state validation are distinct from GPU features.

An explicit, default-disabled fragment laboratory connects the Mesa/NIR
fragment token to the existing retained job through the same memory endpoint
and borrowed owner. It stages only the original eight-byte magenta fragment
candidate into the retained fragment slot. Historical VS, PDS and other carrier
bytes stay unchanged. This is a deliberately partial shader integration.

The laboratory requires a trusted capture callback and exact profile checks;
uncertain completion quarantines resources. Its screen does not advertise
full acceleration. Offline tests enter real NIR and Gallium draw callbacks
with intercepted device calls. Their fixture output is never reported as GPU
rendering. Existing receipts record 11 cases across native, UBSan, i386/QEMU
and the actual Mesa-built i386 archive. Mesa archive/DRI builds succeeded;
30 current build artifacts plus the complete archive reproduced identically.

## INFERRED / UNKNOWN — execution qualification

The original four-MOV VS candidate is not yet a qualified replacement for the
retained vertex program. Physical operand/output mapping, final export and
bounded termination still require evidence. Original PDS fetch, DATA-to-DS
and task consumption are not established by host relocation arithmetic.
The ordinary executable PDS encoder continues to reject unsupported programs.

The next milestone is attributable hardware execution of the NIR-produced
fragment through the retained carrier, followed by an independently generated
VS/PDS path. A fresh execution/completion/readback receipt is required before
claiming either. No new GPU work was performed for this update.

Desktop OpenGL 2.1 remains the compatibility objective. No supported OpenGL
version or working accelerated application is claimed here. Admission remains
`0x1f`; publication of code and successful offline tests do not change it.

## Development boundary

Production code is independently authored. Historical binary observations
remain isolated research evidence; extracted proprietary shader programs and
instruction templates are not incorporated into the new implementation.
The separate Linux DRM RFC and private research notes are outside this update.

The alternative pre-cleanup README summary is preserved in
[the history archive](../history/README-20261006-SUMMARY.md).
