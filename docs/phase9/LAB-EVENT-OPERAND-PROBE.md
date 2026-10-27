# LAB event operand hypothesis, not an instruction encoder

The general event CODE84 remains unresolved. This experiment prepares bounded
diagnostic edits of the pinned historical event oracle; it does not embed an
event program into the Mesa driver, approve a new instruction form, change a
runtime gate or submit hardware.

The original finalization audit under
`/home/gama/sgx535-offline/general-userspace-driver-20261027T040000Z/event-finalization-audit/`
executes original CODE stores, real commit and metadata publication. Six cases
use two private-RAM bases and capacities148/160/400. Every case has21 CODE
writes, zero CODE writes after construction,148 effective bytes and descriptor
field0c=16. Only400 is the original constructor's requested capacity;148/160
are supplemental allocator-state cuts, not qualified allocation policies.
DATA64 is outside the executed construction slice. Neither this result nor
the descriptor establishes GPU fetch width. The pinned retained relocation
journal contributes seven event fixups at DATA offsets0/8/16, none in CODE.
Unobserved callers or later service passes remain outside that proof.

The candidate hypothesis is deliberately narrow:

| Original DATA word | CODE ordinal | Proposed selector bits | Copy destination |
| --- | --- | --- | --- |
| 12 | 11 | 12:10 | DATA15 |
| 13 | 14 | 12:10 | DATA15 |
| 14 | 17 | 12:10 | DATA15 |

Those are numerical bit positions, not established PDS operands, banks or
instruction semantics. DATA15 was unwritten by the CPU producer and zero in
the captured oracle; that does **not** prove the GPU never reads it.

`tools/remesa/lab_event_operand_probe.py` accepts only the exact148-byte event
whose SHA256 is8ec9cecfde60a0312c5ae83ba7ffae34d45fd26bbaad7628e8afee5867a15061.
For each source it emits four controls: copy-only, select-copy, copy with the
original zeroed, and select-copy with the original zeroed. The copy is always
made into DATA15. CODE remains84 bytes; only the selected three-bit hypothesis
may change, and no instructions or alignment padding are added. Reference
bytes are supplied externally as a LAB oracle, never included in product CODE.
An existing output directory is refused rather than overwritten.

```sh
python3 -B tools/remesa/tests/test_lab_event_operand_probe.py
python3 -B tools/remesa/lab_event_operand_probe.py \
  --reference-event "$EVENT_REFERENCE" --output "$NEW_PROBE_DIRECTORY"
```

The twelve generated files and manifest at
`/home/gama/sgx535-offline/general-userspace-driver-20261027T040000Z/event-operand-probes/`
are not launchable profiles. They contain no module, boot image, admission
receipt or launcher. The four unit tests cover all12 controls, all148 one-byte
reference corruptions, invalid sizes/requests, immutable source, exact DATA/CODE
changes, CLI output and refusal to overwrite evidence. These tests prove only
mutator confinement and manifest behavior, not the field hypothesis or GPU
memory safety. Mesa/kernel code and the FAST path are unchanged by this tool.

A physical experiment still requires a strict separate LAB profile, independent
address/PFN/permission confinement, necessary synchronization/fences, failure
handling and recoverable N/N boot with rollback. It must preserve identity,
submission, completion, retirement, readback and input/buffer integrity checks.
The required1280x800/640x640 pair and occasional641x479 remain applicable;
desktop GL and GLES results must be reported separately when exercised.
Raw LAB operations cannot count as passes for either API; both API rendering
qualifications remain outstanding.
No failed retirement, fault or integrity error may be waived for this hypothesis.

Interpretation must distinguish controls. If copying into DATA15 alone changes
the result, that cell cannot be treated as dead. Matching select-copy alone
does not prove a field: the changed instruction might not be observed on that
path. A changed original-zero control plus restoration by select-copy-zero,
with matching copy-only/select-copy controls, would support the candidate for
that observed branch/scene only. It would not establish a generic ISA encoder.
COLOR and defined Z bits must use the retained LAB contracts, including VOID
upper Z bits; no CPU rendering may replace the physical operation.

The next executable step is to establish the complete LAB profile and memory
confinement for those controls. Generating files or matching original bytes
does not satisfy it. Full event construction, native job admission, general
3D/2D APIs and an installable physically qualified driver remain pending.
