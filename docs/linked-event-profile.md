# Preserved fixed pixel-event profile and owned upload

The original psb392ab pixel event already worked in the physically qualified
SGX535 glxgears profile. General PDS instruction semantics are not required to
preserve that exact construction. This component reconstructs its bounded CPU
store recipe, connects its DATA references to retained resources, and publishes
an immutable program allocation. It does not provide a general event assembler,
change CODE84, or admit a new native scene.

`remesa_pds_structure_encode_psb_event_profile()` reuses the existing DATA64
encoder and reproduces the original constructor's 21 CODE stores. The two
code-base-relative entries reuse the established forward-index helper at
ordinals2/7 with deltas3/13. Other words retain their original fixed values;
no new operand interpretation, register mapping or instruction mnemonic is
assigned. This explicit fixed recipe must not be presented as independent
recovery of a general PDS ISA. It accepts no caller-supplied CODE image.

The contract keeps separate:

| Quantity | Bytes | Meaning |
| --- | ---: | --- |
| DATA | 64 | CPU construction, including12 policy-zero bytes |
| CODE | 84 | Original21-word fixed program |
| Committed image | 148 | DATA followed immediately by CODE |
| Original reservation | 400 | Capacity, not additional instructions |
| Allocated backing | 4096 | Current page-rounded ownership allocation |

Output stops at148 even when capacity400 is supplied. DATA words6/7/15 remain
policy-zero/mask0; all CODE bytes have known CPU stores. Neither committed size,
reservation nor descriptor field0c=16 establishes the GPU's complete fetch
extent. No CODE filler or extra instructions are generated.

`remesa_persistent_linked_event_profile_prepare()` accepts one live auxiliary
owner, derives target/end/transfer links from its retained graph, and allocates
in the existing PDS/program VA class with32-byte alignment. It publishes148
bytes through the existing one-time immutable image write/verification and
sync-for-device path. Remaining allocation bytes follow the existing zero
initialization policy. The new owner retains auxiliary, targets, TA, COLOR/Z
and shader/geometry dependencies transitively. Public writes and old color
submission remain rejected. Uncertain disposal quarantines the owner and
preserves its dependencies rather than releasing possibly live GPU addresses.
Snapshots revalidate live ownership/mappings/leases and derive metadata without
allocation, mapping, hash, readback or synchronization.

This corrects the prior overly broad implementation boundary: lack of a general
instruction encoder should not prevent constructing the already preserved fixed
profile. It does not reinterpret the older DATA-only tests as event execution,
or claim arbitrary instruction changes are safe. Scene footprint/capture still
use their previous auxiliary-only contract and retain every incomplete flag;
connecting the new program owner and complete raster/init/native admission is
subsequent integration work.

Evidence root: `/home/gama/sgx535-offline/linked-event-profile-20261031/v1`.
`ORACLE.json` binds CODE84 at role0+0x4500 in the preserved physical profile and
the already retained REA constructor evidence
`ev_5e223fb07de81e08732f7623a5e0165938f2fb580f961f4c0e45e192421406f2`.
No binary analysis was rerun. Tests compare all84 CODE bytes,80 DATA-reference
combinations, mask/tail/capacity/alias rejection, three target dimensions,
transitive ownership after public release, leases, repeated zero-I/O snapshots,
ordinary rollback and uncertain failures. `source-tests/result.json` records
native/UBSan/statici486 results. `actual-mesa-event-profile-final/RESULT.json` records
an actual Mesa/Gallium/NIR two-object private archive rebuild with simulated
buffers/fences; `REGRESSIONS.json` covers the previous auxiliary owner and scene
footprint/capture contracts. `FINAL_BINDING.json` pins final source and receipts.

The CODE comparison uses historical physical evidence. New owner upload tests
are CPU/provider models, with no new GPU submission or retirement, installed
DRI qualification, framebuffer readback, general OpenGL/GLES or FPS result.

`FINAL_COMMENT_CORRECTION.json` records the sole header-comment correction
after source tests and verifies its preceding hash against the first Mesa
receipt. The final Mesa rebuild uses the corrected header. No behavior changed
after the native/UBSan/statici486 tests or the three archive regressions.
