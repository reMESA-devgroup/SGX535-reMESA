# Physical-contract recovery: partial, with an implemented descriptor correction

**Status: PHYSICAL_CONTRACT_PARTIAL_EVIDENCE.** No architecturally qualified executable package was established. GPU and the physical PDS encoder remain ENOTSUP. The independent CPU PDS/VS/FS programs from the previous checkpoint are preserved.

## New implementation and consequence

The fragment entry of `remesa_pds_preview_record_pair()` now reports a fully determined **CPU** address/prefix word. Its old maskfcffffff incorrectly retained bits25:24 as unknown for this selected producer/fixup contract. These bits are zero. VS retains the old mask because its equivalent producer contract was not established. The encoded word value, bounds, ownership checks, exact epoch validation and failure atomicity are unchanged.

Files: `remesa_pds.c/.h`, `tests/test_shader_memory_transport.c`, `tests/test_pds_structure_pipeline.c`. The existing combined NIR/Gallium pipeline now verifies and exports the primary FS reference word with its mask alongside independently generated VS16, FS8, DATA48 and CODE8/12. No new wrapper, packet type, speculative opcode, resource manager or submission path was added. For the existing fixture B22000000/q48, the word is0c200000 and its maskffffffff. This four-byte export is explicitly a CPU reference word, **not a complete command packet**.

The current selected CPU grammar is:

| Bits | Confirmed CPU expression | Physical interpretation |
|---|---|---|
|23:0| `(u32(placement + pool_offset + BO_offset) >> 4) & 0x00ffffff` | GPU consumer/base recomposition UNKNOWN |
|25:24| zero after the whole-word fixup | Reserved/architectural role UNKNOWN |
|31:26| `(DATA_words >> 2) & 0x3f` | Physical preload count/units UNKNOWN |

For a lossless admitted preview, B andq are16-byte aligned and0<q<1024. These checks already existed. The new experiment independently corroborates why arbitrary unaligned/raw larger counts cannot be silently generalized: raw n13/14/15 discard low count bits; n256 wraps the field tozero. These are arithmetic boundary probes, **not valid shaders or hardware resource limits**.

## New independent differential evidence

**CONFIRMED_CPU — Xpsb plus public-kernel consumer.** Original Xpsb ELF77e8–7965 and relocation writer91d0–92da executed under the existing bounded CPU harness. Its synthetic pre-existing frame and BO service cuts are explicit; this is not a full original GL call. The mask is00ffffff, shift4/destination0 and background((n>>2)&3f)<<26. The exact shift/combine statements from hash-pinned GPL psb_sgx.c were compiled under UBSan and evaluated on the resulting records. Four independent changes cover n12→16, placement20000000→30000000, and a high low-address field. Whole-word results are0c000016,10000016,0c000016,0cffff16. The higher-region change does not enter bits25:24. This is source-slice execution, not running a kernel or GPU.

**REFUTED in this CPU profile:** bits25:24 retain B[29:28] or the old destination placeholder. The mask strips those address bits and the background contributes zero; the kernel operation replaces the word.

**CONFIRMED_CPU — independent psb producer and full relocation helper.** Eighteen bounded executions of serializer287ff, primary block28acb/secondary28a1c, and original helper37854 reproduce the same expression and record+18=sizebackground. Six joins use counts12/16/76 from preserved constructor-ABI oracles; those are not claimed to be complete frontend-valid shader executions. Ten raw count-boundary probes and two address-only probes are separately labeled. The full helper executes the arithmetic and record store; only BO/list/numeric-address fixture services are cut. Details, PCs, hashes and source scripts are in data-evidence/ and task-evidence/.

## Architecture evidence and limits

**CONFIRMED_ARCHITECTURAL, restricted to the cited target/embodiment:** public TI SPRUGX8C p180 §1.7.3.2 identifies SGX530 scheduling/resource/output stages; the Imagination US8046761B2 disclosed embodiment separates PDS task creation, queue eligibility and output buffering. The sources do not qualify SGX535 encodings. **INFERRED** is the relevance of those separations as SGX535 experimental confounds; AF, control18 and physical export remain **UNKNOWN**. PDF/HTML originals and hashes are preserved. No Rogue, SGX544 or undocumented DDK bitfield was imported into this driver.

## Remaining contract matrix

| Contract | CPU evidence/implementation | Architectural status / exact missing link |
|---|---|---|
|DATA stores and numeric operand indexes|Existing typed DATA/CODE; new independent count→descriptor/relocation closure|UNKNOWN: target receiver mapping from descriptor/prefix memory to physical DS registers, including read set and preload versus implicit aliasing|
|USE address link|Existing lower19 fixup, bounded bank/window arithmetic and immutable links; preserved|UNKNOWN: task-port field meanings/arity and implicit resource/dependency inputs|
|Primary PDS reference|All32 CPU bits now known and tested; VS unknown mask preserved|UNKNOWN: SGX535 consumer's base/size decoding and fetch behavior|
|07/AF|Existing parametric07 and fixed CPU trailer remain preserved|UNKNOWN: dispatch/continuation/AF retirement effects; two models match all observed CPU outputs|
|VS output and FS color|VS16/FS8 independently generated; source codecs unchanged|UNKNOWN: physical bank/export contract, finalizer effects, output-resource release and memory visibility|
|Complete submission package|CPU image bundle with symbolic links and FS reference exported reproducibly|BLOCKED: secondary/state/VDM/raster/output and dispatch/completion contracts are not all qualified; no executable package claimed|

The missing first physical PC is not used as a substitute blocker. What is absent is an independent receiver/ISA definition or attributable hardware observation for the listed physical operations.

## Why another producer replay cannot close the physical gap

The CPU producers store DATA/CODE, lengths and relocations. None of these new replays executes a PDS or USSE semantic interpreter. Bulk-loading DATA into DS and resolving equivalent operands through an implicit backing alias can yield the same CPU trace. Likewise `07 dispatch/continue; AF end` and `07 dispatch/end; AF no-effect trailer` leave all recorded producer transitions unchanged. The new descriptor experiments discriminate CPU field hypotheses; they cannot discriminate these hardware models. A qualified SGX535 decoder/specification could still resolve them offline; physical testing is not asserted to be logically inevitable.

A framebuffer result alone is also insufficient: output buffering/resource scheduling can change visibility independently of shader computation. The public architecture evidence supplies concrete controls for this distinction, not new opcode semantics.

## Validation and preservation

Fresh runs:1,018 memory-transport checks and1,689 combined NIR/Gallium checks per native, UBSan and i386/QEMU;377,455 PDS/USSE regression checks per environment. Mesa25.2.0 static/DRI rebuilt and11 suites linked against the actual archive passed, including32 NIR cases and both changed integration tests. The actual second Ninja build leaves compiled hashes stable.

Two fresh generations produce86 identical CPU artifacts, including the six FS primary word/mask pairs. Complete CODE and VS outputs still match the independently preserved original oracles; no proprietary bytes feed the encoder. The old20/20 and prior66,107 source-test receipts are baseline evidence, not falsely counted as new executions; the archive export suite was executed anew. `GENERATED_COMPLETE_RESULTS.json`, `TEST_RESULTS.json` and the per-suite logs are authoritative.

An initial export-audit harness glob accidentally included the newly added four-byte fs-primary files among eight-byte FS bodies. It failed its size assertion after successful generation. The glob was corrected, the two generations repeated and the failed receipt retained. No driver code or test coverage was changed to satisfy that audit.

Historical checkpoint hashes,192 baseline source hashes and28 gates are audited; four tracked files changed,188 unchanged. ADMISSION unchanged. The incremental patch is applied and compared in an isolated directory. No SSH, MMIO, kick, GPU submission, module/kernel/sudo change or physical experiment occurred.

## Posterior physical validation

`end-evidence/POSTERIOR_ONE_SUBMISSION.md` specifies a conditional two-region positive-signature experiment for control18, one future submission and no automatic retry. It remains **DESIGN_ONLY_NOT_ADMITTED**: the proposed concatenated counterfactual is not established as a valid/safe physical shader. It can distinguish successor-value visibility only after independent bounds/dispatch/output controls and explicit authorization; it cannot alone name END or solve AF. Timeout, hang, unchanged/prefill readback and unattributed output are INCONCLUSIVE. Preflight hash/resource/ownership mismatch is FAIL before submission.

For AF itself, the minimum missing experiment capability is an independently qualified observable successor afterAF with bounded accesses and attributable completion. Such a marker is not currently available. Appending unknown words and interpreting a hang would not solve it. No runnable hardware script or relaxed gate was created.
