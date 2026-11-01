# Owned references for the preserved pixel event

The SGX535 pixel-event profile already has a documented, physically exercised
64-byte DATA section followed by 84 bytes (21 words) of CODE. This change does
not replace or modify that CODE, decode additional instructions, or repeat its
physical qualification. Independent general-purpose instruction emission remains
a separate task; its absence does not invalidate the working glxgears profile.

`remesa_persistent_linked_event_auxiliary_snapshot()` now serializes the existing
DATA encoder's request from the same retained dependency graph that authenticates
the auxiliary USE programs and targets. DATA0 and DATA4 derive from the live
end/transfer entry plans. DATA2 addresses the target owner's committed 16-byte
COLOR-state region, not the COLOR pixel plane or Z allocation. The encoder is
bounded by committed target DATA (72 bytes), not unused allocation capacity.

The returned 64-byte DATA and known-byte mask are CPU snapshot metadata. They
are not appended to the auxiliary allocation, whose effective USE bytes remain
40 (16-byte transfer at zero and 8-byte end at offset32) within page capacity.
Original unwritten words6/7/15 remain zero with mask0; this policy does not claim
the GPU consumes those cells. No filler instructions, PDS allocation, upload,
submission, synchronization, hash or rendering is added. All scene-footprint
incomplete flags and native admission gates remain unchanged.

Snapshots retain existing endpoint, ownership, dependency, generation, address,
entry-bank and CPU/GPU-access checks. Misaligned output is rejected before
writing. Tests verify exact output against the prior numeric encoder request,
the complete mask, target-versus-pixel address distinction, stable snapshots
without provider allocation/mapping, retained child lifetime, mapped-buffer
rejection, output preservation, original auxiliary bytes and failure rollback.

Evidence: `/home/gama/sgx535-offline/linked-event-data-binding-20261031/v1`.
`source-tests/result.json` records native, UBSan and static i486 tests.
`actual-mesa-event-data/RESULT.json` records a private one-object rebuild using
actual Mesa/Gallium/NIR flags, with simulated buffers/fences. The parent archive
and all other members remain unchanged. `REGRESSIONS.json` records scene
footprint/capture regressions; `FINAL_BINDING.json` pins source and receipts.
No GPU operation was performed by these tests. They do not qualify a newly
installed driver, general OpenGL/GLES execution or a new physical event program.
