# PDS + USSE: complete CPU images, physical contract still blocked

Status: **PDS_ENCODER_PARTIAL_EVIDENCE**. Complete CPU-constructor CODE and a complete minimum VS CPU allocation can now be generated independently. A physically qualified executable PDS/VS pipeline is **not** established. GPU remains ENOTSUP.

## Implementation

`remesa_pds_structure.c/.h`: new explicit `CODE_CPU_COMPLETE` request policy and typed `CPU_TRAILER_AF`. The existing parametric PAIR_SINGLE prefix, A2/class64, allocator, symbolic DATA references, USE lower19 relocation, little-endian storage and validation are reused. The complete policy resolves the initial slot and emits the final CPU trailer; every CODE bit is known. Policy zero preserves previous masked structural behavior. The new field is appended after the historical848-byte prefix (current C struct852bytes); historical fixtures are copied into a zero-initialized struct by a private adapter, never reinterpreted as852-byte records.

The AF CPU rule is specific: the primary psb25970 constructor unconditionally stores AF000000 at ELF263e4 after its generated body and before commit. It has no explicit CPU encoding operands; both typed DS indexes must be NO_CELL. Encoding rejects nonfinal/multiple trailers, AF-only sequences, wrong prefixes and unknown classes before publishing output. This is a fixed contractual word, not a historical shader template or a claimed HALT opcode. Unknown DATA cells/fixups retain their evidence masks.

`remesa_usse.c/.h`: semantic POSITION IR with the explicit numeric mapping input[c]=(class2,c), output[c]=(class1,c) now generates16bytes, using the existing MOV and three-source codecs. The first record uses source(2,0), destination(1,0), controls12=3,21=1,23=1. The second uses sources(0,0),(2,0),(2,0), extensions0,1,1; W0control21=1, W1control12_13=1 and finalizercontrol18=1. These arguments were observed through the actual valid original frontend, setter, linker and builder, independently of the resulting hashes. Other numeric mappings retain the prior32-byte candidate. No physical bank/export/END names are assigned to unknown fields.

`test_pds_cpu_trailer.c` is new; the existing export-contract and structural-pipeline tests were extended. The existing host shader pipeline accepts the opt-in complete CPU PDS image with16-byte VS and8-byte constant FS, immutable storage and symbolic relocation. No default Gallium execution path is enabled; no resource/framebuffer/fence subsystem was rewritten. Generic VS `remesa_usse_encode()` still lacks an explicit qualified mapping; physical `remesa_pds_encode()` still returns ENOTSUP.

## Independent new evidence

**CONFIRMED CPU:** two authored public Mesa IR constant-green shaders, differing only in the recovered R feature, execute original33490→370d8→25970→original384ab commit under bounded x86 emulation. DATA48bytes; CODE8/12bytes; committed totals56/60bytes. The additional observation is the CPU read set: zero x86 DATA loads of CODE/AF after its store. The commit computes length=end−start at384d1–384d6; it does not decode AF. Fixture services and bounded stops are documented in the replay, not represented as full GL operation.

**CONFIRMED CPU:** valid POSITION=input0 and POSITION=input16 compile and pass actual setter2e7f2→link2e72c→USEbuilder4252d. The linker computes auxiliary count zero. Actual30a09 arguments are (0,8,8,0,1,0,0,1), followed by30fb7 finalization. Both commit16bytes before the replay stops at4265e, before PDS allocation4268d. Own semantic IR/API-generated16bytes match both complete original allocations. The paired-output40-byte case is deliberately not generalized.

**CONFIRMED CPU:** three linked FS cases traverse original264b7; at26654 it sets final word1bit18 before commit26665. This establishes a constructor control rule, not architectural termination. Existing8-byte constant green/magenta/yellow encoding remains reused.

**IMPLEMENTED_OFFLINE:** complete CODE compares against20 preserved original x86 oracles in native, UBSan and i386/QEMU, including the AF and initial words. Six combined own-IR images (three colors × R off/on) generate VS16, FS8, DATA48, CODE8/12;74 exported artifacts are identical over two separate generations. Independent new green Roff/on full CODE oracles and two complete VS allocations match. Historical588DATA/304previously-knownCODE bytes still match; those historical counts are not relabeled as fully known DATA.

## Final validation

- CPU trailer:1,014 checks and20 whole-CODE oracle cases per native/UBSan/i386 variant; additional actual-Mesa-archive i386/QEMU run also1,014 and20/20.
- VS export contract:66,107 checks per native/UBSan/i386 and actual archive.
- Combined structural/NIR/Gallium pipeline:1,641 checks per native/UBSan/i386 and actual archive.
- USSE/PDS regression:377,455 checks per native/UBSan/i386 on the final frozen source.
- Mesa25.2.0 static archive and ELF32 DRI built;10 actual-archive suites, including32 actual NIR-builder cases, passed. Actual second Ninja execution left compiled hashes unchanged.
- Final protection and patch-application receipts are in INTEGRITY.json and PATCH_CHECK.json. Six existing source files changed, one test added; the other185 original sources and28 gates are unchanged. ADMISSION unchanged.

Authoritative final receipts: combined-complete-vs/, usse-pds-regression-final/, mesa-build-complete/, integration-evidence/archive-test/, usse-evidence/TEST_RESULTS.json and GENERATED_COMPLETE_RESULTS.json. Earlier8-byte-VS intermediate tests/builds and one failed archive test are preserved chronologically and are not the final qualification. The earlier USSE note describes the intermediate8-byte body; next-vs/WHOLE_VS_CPU_CONTRACT.md supersedes it for the final16-byte program. The failed archive check exposed an outdated header/test comment; it was corrected before final frozen validation without removing coverage.

## Exact remaining boundary

**UNKNOWN:** physical DATA→DS preload/fetch, task-port interpretation of07, meaning of AF, physical input/output banks and VS export/termination. Physical encoder/submission contracts remain blocked. Complete CPU CODE does not establish that arbitrary initialized but unknown DATA holes are physically unused. DATA mask zero remains zero.

The new original-commit/read-set experiment is already the next CPU experiment that was executed. It demonstrates why another literal-producer replay cannot close AF: modelA (07 dispatch/continue, AF end) and modelB (07 dispatch/end, AF no-effect trailer) produce the same observed x86 transitions, metadata and existing constant readback. No CPU decoder consumes AF on the audited path. The uncertainty does not arise from first-PC; that investigation was not reopened.

The bounded independent SGX544 supplied-blob lead found a USE terminate allocator and an import of PVRUniFlexCompileToHw, but its libusc.so implementation is absent from that supplied set. It supplies no qualified SGX535 AF decoder or new encoder rule; compatibility was not assumed.

Next highest-value evidence: a provenance-qualified SGX535 CPU disassembler/interpreter or semantic IR writer mapping both07 continuation and AF to named semantic operations. Merely finding another invariant AF store, or a correctly rendered constant shader, does not distinguish the two models. For incremental CPU VS coverage, the concrete next existing producer is psb ELF3fd16/40125 on the valid paired-output40-byte case; it can extend auxiliary handling but cannot prove PDS termination.

No SSH, MMIO, GPU submission, kick, module/kernel/sudo change or installation occurred. No full OpenGL claim is made.
