# SGX535 rev121 texture-sampling decoder observations

The original SDK35 USSE decoder was called with its original SGX535/rev121
context, not the compiler CLI's default SGX540/rev130 target. No GPU calls,
production shader changes, driver feature enablement or Mini12 boot occurred.

Evidence: `/home/gama/sgx535-offline/desktop-texture-contract-20261028/v1/`.
The pinned executable, wrapper and context are described in
[USSE vendor oracle](USSE-VENDOR-ORACLE-20261027.md).

## Observations

The 32 opcode-family probes identify family28 (`high=0xe0000000`) as `smp1d`.
Sixty single-bit probes locate fields; 112 combination probes exercise unequal
register numbers, bank combinations, all16 channel masks, all4 DRC selections,
and dimension/LOD combinations. Raw bytes, exact commands, source hashes,
stdout/stderr and parsed results are retained in `SGX535_OPCODE_FAMILIES.json`,
`SGX535_SMP_FIELDS.json` and `SGX535_SMP_COMBINATIONS.json`.
`probe_smp_combinations.py` reproduces the last corpus in a fresh artifact folder
with the same pinned executable/shim. It refuses to overwrite the input corpus.

Selected textual observations (word0 is low, word1 is high):

| Field | Observed decoder effect |
| --- | --- |
| word0 bits21..27 | destination register number |
| word0 bits14..20 | second operand register number |
| word0 bits7..13 | third operand register number |
| word1 bits10..11 | 0:1D, 1:2D, 2:3D; 3 prints `invalid_smp_lod_mode` |
| word1 bits8..9 | ordinary, bias, replace, grad spellings |
| word1 bits12..15 | channel mask; x alone is printed without suffix |
| word1 bits0..1 | `drc0` through `drc3` |
| word1 bit2 plus bit18 | second operand banks r/pa/o/sa |
| word0 bits30..31, word1 bit17 clear | third operand banks r/o/pa/sa |

For example, one controlled record prints `smp2d r7, pa11, sa19, drc0`.
This establishes that the decoder accepts an `sa` spelling for that operand;
it does not establish how many state words the GPU reads there.
The combined-field consistency check covers all112 records and preserves all31
protected files. Output canaries and termination checks passed in the shim.
The checks concern decoder text; their expected strings are not independent
proof of hardware semantics.

## Remaining contract

The sampler/view metadata emitter still returns ENOTSUP for the hardware
texture descriptor. We have not established its word count, address/format/
stride/dimension fields, alignment, fetch footprint, PDS delivery or retirement
requirements. The USSE instruction decoder does not decode that state or PDS.
The decoder even returns integer0 for the dimension3 cases that print invalid;
return0 must not become an instruction-validity or GPU-admission gate.

Next minimum work is to establish the SGX535 texture-state layout and its upload
contract from permitted independent material, then qualify bounds/ownership and
a real textured draw before connecting shader lowering. Keep synthetic probes
outside production CODE and preserve general DRI admission until these contracts
are implemented. This is progress toward the shared textured desktop renderer,
not an accelerated desktop or physical texture-sampling result.
