# Decoder-derived scalar SMP2D body — 2026-10-28

The driver now contains `remesa_usse_sample_2d_body_encode`, a small standalone
backend encoder registered in the staged Mesa sources. It builds two effective
64-bit instructions from independently described fields: ordinary SMP2D with
xyzw mask, r destination, pa coordinate operand and sa sampler operand, followed
by WDF on the exact same requested DRC. It has no predicate/repeat/LOD override,
export, END, trailing CODE filler or complete-shader admission.

The 7-bit indices are field representation, not proof of allocated register
windows or GPU read/write spans. In particular the field value127 in a CPU test
must never be treated as a safe xyzw allocation. The names pa/sa/r are original
decoder spellings. This encoder does not establish interpolation, normalized
coordinates, sampler-state fetch size, format, resource ownership, cache
visibility or task capacity. WDF is GPU instruction text, not a CPU fence or
retirement receipt. Existing normal shader/descriptor gates remain unchanged.

## Evidence and construction

The existing SMP observations are preserved in TEXTURE-SMP-ORACLE-20261028.md.
New REA evidence from the complete decoder catalog is reused without reopening
Ghidra: SMP parser0x0829228a (`ev_a33cca1817df0dd7860b763f0c4c57cd98c705a6295b54e205f82e4d8f0c39a3`),
operand helpers0x08289d26/0x0828a1cc, WDF parser0x0829863e
(`ev_2cd9917b42dc456f6d98f996a5918433884a75f47396c3e2a952f52329ce1344`).
No vendor executable/decompiled source is copied into the production unit.

SMP word0 is bank3 in bits31:30, destination bits27:21, coordinate bits20:14
and sampler bits13:7. Word1 is major28, channel mask15 in bits15:12,
dimension1 in bits11:10 and coordinate bank1 in bit2, plus DRC bits1:0.
The WDF fields are major31, other-selector2 in bits21:20, suboperation1 in
bits26:24 and the same DRC bits1:0; its word0 is zero. These observations qualify
byte-to-text construction on the original scalar535/rev121 decoder only.

## Verification

Evidence directory `/home/gama/sgx535-offline/usse-sample-body-20261028/v1/`.
Native, UBSan and i386 each PASS 52545 checks, including 2916 tuples over unequal
and boundary register indices and all four DRCs. Actual emitted bytes from all
three variants are identical. The original decoder checks all 5832 instructions
against exact SMP2D/WDF text, with canaries and termination intact. Its return
boolean is not used as an acceptance gate. Invalid fields/reserved input,
insufficient capacity, aliases and wrapping host spans preserve both outputs;
excess capacity is untouched. This distinguishes effective16 bytes from a larger
reserved output window. No copied instruction template is emitted.

`MESA_OBJECT.json` records compilation of the freshly staged source with the
existing Mesa build flags. The fresh overlay contains108 files. This is an
updated-unit compile, not a complete newly linked or installed Mesa library.
`RESULT.json` and `ORACLE_RECORDS.json` retain all commands/cases and source
hashes. All31 protected files are unchanged. No GPU, boot or install operation.

## Next integration requirement

Connect the sampler register operand to the owned committed second constant
source image and its indirect texture/state dependencies, preserving the distinct
first image. Establish task register resources, pixel-stage coordinate delivery,
result consumption/export and termination, then combine with whole-interval
private-root exclusion/restore and owned retirement. Only an exact physical
textured draw with identity, submission, completion, retirement, readback and
guards can qualify this for execution. Generic NIR texture/ALU/control flow,
GL/GLES2 capability and installable full desktop acceleration remain incomplete.
