# PDS/USSE implementation priority

**PDS_USSE_PARTIAL_IMPLEMENTATION.** Concrete driver changes compiled and tested. No complete PDS program and no GPU execution claimed.

## Implemented

1. `remesa_usse_cpu_three_source_encode()` now emits the selected original eight-byte, three-source CPU format from the existing typed record. Previously only its decoder existed. All14 numeric fields have explicit limits; invalid classes, numbers, extensions, controls, aliases, capacity and pointer wrap fail transactionally. No historical instruction bytes appear in the encoder. Field construction is derived from the original x86 writer at psb ELF30a09/Ghidra40a09, allocator2fabd and four helpers. A fresh827-byte audit verifies those producers. This encoder is incorporated into the actual Mesa archive; it does not assign hardware banks, operation mnemonic, VS export or END meaning.
2. The existing packed constant fragment-body encoder now also admits **opaque yellowffffff00**, already accepted by the NIR semantic frontend. It generates8bytes by the same established payload-field rules as green/magenta. NIR admission remains three colors. Gallium create_fs_state/report/encode and immutable VS/FS host storage use the new case. Yellow no longer falls back to the56-byte literal/pack candidate; pixel-task and structural PDS pipeline preparation now accept its eight-byte body.
3. Pipeline tests cover that transition, retain the former oversized56-byte binding as explicit EPROTO rejection, and preserve ENOTSUP/unchanged-output coverage using a genuinely unsupported half-float constant. Thirteen other endpoint colors remain rejected. Existing resource/storage/relocation/fence/draw mechanisms were reused.

## Independent evidence

- Selected three-source field ranges are supported by actual original helper mappings, not just an inverse decoder hypothesis. Source0 class0..1/ext0..1 and source1/2 class0..3/ext0..1 are all representable by their CPU descriptor tags. Register numbers are7-bit. Two authenticated historical records are supplied only as external validation oracles; no bytes are embedded or transplanted into the implementation.
- Original initializer30ffd→payload writer30d15→finalizer30fb7 commits eight bytes for16 endpoint payload probes. Current C driver output for the three admitted colors matches these original producer results byte-for-byte. These probes stop before PDS construction. They establish CPU packing, not16 hardware color semantics or production shader execution.

## Validation actually executed

Native, UBSan and i386/QEMU all pass:

- Three-source codec:282 checks per variant, two independent reference records,14 fields,23 fixed/unmodeled-bit rejections.
- Packed constant:453 checks per variant.
- NIR/Gallium fragment encoding:179 checks per variant, including actual yellow FS creation and eight-byte encoding.
- USSE/PDS semantic regression:377455 checks per variant (previous377452 retained, plus three positive checks).
- Pixel pipeline:343 checks; structural pipeline:1461 checks per variant.
- Twenty legacy PDS oracle cases per variant,588 DATA/304 known CODE bytes preserved. AF/initial unknown holes remain explicitly excluded; masks not weakened.
- Existing pair/single encoder tests:70 per variant.

Mesa25.2.0 archive and DRI built fori386, no installation. Actual-archive tests include the new three-source282 checks, constant453, fragment179, pair-single70, allocator113, class64115, structural1461, pixel343, host207 and32 real NIR builder cases. Initial full build and second real Ninja stability check passed; the later API-comment correction was rebuilt and all compiled artifacts remained byte-identical. Complete receipts/logs are linked by TEST_RESULTS.json.

An initial regression execution failed on the old yellow-ENOTSUP expectation. It was corrected to assert the new positive behavior and retained transactional rejection of unsupported inputs. The initial failure log remains preserved and is not counted as final approval. Preliminary interrupted agent test directories are similarly not authoritative; final receipts are constant-final/encode-final.

## Exact remaining blockers

**PDS:** no qualified independent definition/decoder establishes AF instruction class, required-zero fields, implicit reads or termination behavior for SGX535. A legitimate zero-operand fixed instruction definition would suffice; artificial parameter variation is not required. Neither literal-copying nor a new numeric field partition is justified by existing observations alone. AF remains a hole in the structural emitter, and remesa_pds_encode remainsENOTSUP. DATA→physical DS and task-output dispatch qualification are also still separate UNKNOWNs. No PDS source was changed merely to report success.

**USSE:** the new three-source codec establishes the CPU writer format, not its role as a complete VS export. Physical input/output bank mapping, vertex consumer contract and task termination remain unqualified. The generic USSE encoder is restricted to the admitted constant FS bodies; unsupported programs stayENOTSUP. The new codec is available offline and archive-linked, but is not wired into the default VS path on an invented interpretation.

**CONFIRMED CPU:** producer expressions, bounds, independent byte comparisons. **IMPLEMENTED_OFFLINE:** typed instruction emitter, yellow FS and existing-pipeline integration. **UNKNOWN:** hardware semantics just listed. **BLOCKED:** complete executable PDS/VS. FirstPC is untouched and is not used as a blocker.

## Next implementation prerequisite

Priority remains the missing instruction semantics, not RenderCb/resource plumbing. For PDS, obtain an admitted SGX535-compatible instruction definition or decoder establishing AF's effects; do not repeat constant-store scans. For USSE, trace the selected three-source form's valid VS inputs/outputs to the original export-state producer before using it as an export. Newly implemented code can directly encode its proven fields when that semantic route is qualified.

Eleven pre-existing source/test files changed plus this report copied into docs/phase9. All other captured sources and all28 gates/ADMISSION remain hash-identical. Zero SSH, GPU submissions, MMIO, kicks, kernel/sudo changes. Historical evidence directories were read-only; cached Mesa build files were backed up before regeneration. GPU=ENOTSUP.
