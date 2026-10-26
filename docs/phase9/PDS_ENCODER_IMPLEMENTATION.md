# Phase 9 — original bounded CODE emitter, 2026-10-19

**PDS_PARTIAL_IMPLEMENTATION.** There is new calculated CODE emission and a
Mesa/Gallium integration; there is no complete executable PDS program. The
missing initial/final instruction rules, rather than the physical first PC,
are the remaining emission blockers. No GPU execution was attempted.

## Implemented

`mesa/sgx535/remesa_pds_structure.{h,c}` now accepts a typed sequence of
`remesa_pds_code_instruction` and emits little-endian words through
`remesa_pds_structure_encode_code()`. Supported classes are A2 and class64,
with the independently reconstructed CPU expressions below. Unknown classes,
including initial/final holes, return ENOTSUP without modifying bytes or
`written`. Operands are bounded to 0..63 as an admission policy, not as a claim
about the hardware's maximum index.

`remesa_pds_structure_serialize()` emits the existing R/T DATA grammar and
calculated CODE at their canonical byte offsets, with a parallel known mask.
It validates counts, descriptor indices, slot fields, cached words, relocation
sites and independent last-R/last-T markers before publishing anything.
Unresolved addresses and initial/final slots have zero value AND zero mask.
These bytes are analysis holes, not encoded NOPs or usable instructions.

The existing structural R/T builder, resource allocator, DATA encoder,
USE lower19 relocation, immutable storage and host shader pipeline are reused.
The new APIs add byte emission and ownership to those components; they do not
reimplement them.

## Reconstructed field rules — CONFIRMED CPU encoding

```text
A2(j) = 0x070400a2 | ((j >> 1) << 13) | (((j & 1) + 2) << 11)
C64(a,b) = 0x07000064 | ((a >> 1) << 18) | ((b >> 1) << 13)
         | ((a & 1) << 11) | (((a + 1) & 1) << 9)
         | (((b & 1) + 2) << 7)
```

Examples from original constructors: A2(1)=070418a2, A2(3)=070438a2;
C64(2,2)=07042364 in psb; C64(2,1)=070403e4 in Xpsb. The latter is an
independent producer and proves that the low byte need not remain64.
These are structural class names, not established ISA opcodes or DS ports.

Original psb stores: ELF2636a (A2), ELF263c4 (class64), function25970.
Xpsb stores: ELF89c5/8a11, function82c0. Ghidra addresses add10000 in the
preserved projects. Full expressions, raw assembly and oracle provenance are
in `isa-audit/` and the preceding A2 checkpoint.

## Integrated behavior — IMPLEMENTED_OFFLINE

`remesa_shader_host_pipeline_create_pds_structure()` derives the fragment
DATA recipe from symbolic R/T inputs, resolves the existing USE lower19 link,
and owns the CODE, CODE mask and DATA mask in existing immutable storage.
The serializer retains texture relocation sites as symbolic holes. Their
BO IDs are not mistaken for resolved GPU addresses.

`remesa_context_prepare_pds_structure_pipeline()` uses the actually bound
NIR VS/FS, existing constant green/magenta FS encoder, existing candidate VS,
owner cookies and mapping generations. Revalidation detects stale shaders,
descriptors, mappings and recipes. These are explicit offline APIs at the
existing draw-preparation boundary; default draw policy and GPU submission
have not been enabled. See `MESA_INTEGRATION_STATUS.md`.

## Evidence and actual tests

The new emitter reproduces all defined bytes in the 20 independent historical
x86 constructor cases: 588 DATA bytes and 304 calculated CODE bytes in each
of native, UBSan and i386/QEMU. Unknown slots and symbolic addresses are
excluded explicitly, never counted as matches.

The own valid Mesa MOV/END IR also runs through the original full CPU compiler,
linker and PDS constructor. The new byte emitter matches all known fields for
R off/on in native, UBSan and i386. The R-on case emits one original calculated
A2 word (4 bytes); R-off emits no A2/64. Neither contains a complete program.

The integration suite passes 1396 checks per variant. Allocation-failure
injection verifies cleanup/rollback. Other tests cover wrapping/overlapping
ranges, buffer canaries, alignment, malformed extents, immutable sources,
stale owners and rejection of unsupported instructions. Final per-command
receipts, regression results and build status are in `RESULTS.json`.

Development failures remain preserved: an incorrect RT last-R-marker oracle;
output alias error precedence; a stale-shape test using an invalid old layout;
and missing include/link inputs in an existing IR qualification runner. Its
older NIR builder expectation also assumed all FS encoding was unsupported;
it now verifies the already-supported magenta body and preserves unsupported
VS/error checks. All56 builder cases remain. Final receipts supersede failed
attempts; these changes do not invent a new shader capability.

## Remaining boundary

CONFIRMED: CPU_DESCRIPTOR_ENCODING, calculated PDS_CODE_STRUCTURE.
IMPLEMENTED_OFFLINE: typed emission, sparse serialization and shader integration.
UNKNOWN: semantic operand fields of07000345 andAF000000, physical DATA/DS/task
consumers, R attribute delivery, executable PDS semantics and first PC.
BLOCKED: a complete PDS encoder. `remesa_pds_encode()` and executable-contract
emission remain ENOTSUP; successful sparse serialization is a different API.

Four audited producers use immediate stores for07000345. Splitting that
constant into invented operand names would only disguise a byte transplant.
`PDS_07000345_ENCODING.md` records the evidence and falsifiable alternatives.

ADMISSION/gates and historical evidence are preserved. Zero SSH, MMIO, kicks,
GPU submissions, sudo changes or system Mesa installation in this round.
Historical render/readback observations are baseline evidence, not proof of
the new encoder or physical execution semantics.

## Reproduce

Run `python3 -B REPRODUCE.py --output /absolute/new/directory` from this evidence
directory. Add `--with-mesa` for the preserved-cache static/DRI build and tests.
The scripts require the existing pinned local toolchains and preserved oracles;
they do not download a DDK or install anything. `implementation.patch` is
incremental against the saved `before/` snapshot, including inherited-untracked
structure files. It intentionally does not include older dirty worktree edits.
