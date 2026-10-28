# Original Series5 USSE oracle for SGX535 rev121

The official SDK 3.5 compiler's hidden USSE decoder now runs offline with its
own original SGX535/rev121 descriptor. This independently names instructions
emitted by the existing encoders; it does not qualify a new GPU program,
resource binding, register allocation, API feature or submission path.

The complete evidence is retained at:

`/home/gama/sgx535-offline/hidden-usse-disassembler-20261027/`

Start with `SDK35_REPORT.md`, `SDK35_RETAINED_COMPARISON.json` and
`SDK35_GENERATED_AUDIT.json`. The compiler artifact has SHA256
`9349ed4d7643d81cb638b41eb41b69507cc2f9f15bae14c8f2116e79f70e7bd9`.
It was extracted using ranges from the official SDK archive, with the original
page length/CRC32 checks, without executing the installer. No proprietary
executable or source is incorporated into the driver or this repository.

## Bounded controlled execution

`sdk35_decoder_preload.c` calls the original inner decoder at ELF VA
`0x0829921b`, using the original descriptor at `0x082d2a88`:
`[4, 121, 0x40128f80, 0, 0x082d2800]`. It checks those five words, input length,
output termination and output canaries. Execution uses QEMU i386 and the private
cross-toolchain loader/libc/libm; it replaces compiler main and calls no GPU,
GL or EGL interface. The Python harness limits execution to 15 seconds.

The external SGX540 example's wrapper address `0x082994cb` also exists in this
build. Its loop consumes eight bytes per iteration: count is the number of
64-bit instructions, not DWORDs. Our harness calls the inner decoder once per
instruction rather than copying the example's file-size/4 count.

The older EMGD descriptor has a different layout and flags. Neither it nor the
external fabricated SGX540 context is substituted into this SDK decoder.

## Existing generated encoders checked

`audit_generated.c` calls the actual unchanged `remesa_usse.c` and
`remesa_usse_arithmetic.c` implementations. It produces 17 cases comprising
122 instructions: ADD/MUL, MAX, single-parameter ADD/MUL, parameter MAD,
first-term control, both MVP uniform layouts, both complete generated gears
body layouts, DOT3 and reciprocal square root. All encodings are identical
between native, UBSan and i386 builds. QEMU runs and UBSan complete without
stderr or errors. `GENERATED_AUDIT_BUILD_CHECKS.json` records these results;
the oracle audit records the exact source hashes and per-case bytes/text.

Observed examples:

| Existing request | Original decoder output |
| --- | --- |
| ADD, class2/base0 + class2/base4 | `fmad.skipinv.repeat4 o0, pa0, c52, pa4` |
| MUL, class2/base0 * class2/base4 | `fmad.skipinv.repeat4 o0, pa0, pa4, c48` |
| MAX, class2/base0 and class2/base4 | `fmax.skipinv.repeat4 o0, pa0, pa4` |
| Selected ADD with class3/base1 | `fmad.skipinv.repeat4 o0, pa0, c52, sa1` |
| Selected parameter MAD | `fmad.skipinv.repeat4 o0, pa0, sa1, sa5` |
| First-term component control | `smlsi #1, #0, #1, #0, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0` |
| Selected DOT3 | `fdp.skipinv.repeat3 o0, pa0, pa0` |
| Selected reciprocal square root | `frsq.skipinv o0, pa0.abs` |

For the two generated MVP bodies, the decoder names the four coefficient bases
`sa17`, `sa21`, `sa25`, `sa29`; product/accumulator destinations are `r0`, `r4`,
and the final position is `o0`. In the complete generated gears bodies it names
normal input `pa4` for the attribute case and `sa33` for the state case,
material `sa5`, scalar alpha `sa4`, and final color output `o4`/`o7`.
These are independent ISA spellings for the sampled bytes. They do not prove
that a particular DMA, task or allocation delivers the required contents.

The 12 retained-instruction comparison also identifies the selected numerical
three-source tail records as `emitvtx` or `emitst`, event transfer records as
`emitpix1`/`emitpix2`, and the event end as `ptoff.end`. Those exact forms must
not be generalized to every family31 instruction or all numerical classes.

## Limits and next use

The oracle emits individual instruction text. This audit does not execute MOE
state, calculate shader results, prove rounding/constant values, establish
whole-program equivalence or replace paired physical desktop-GL/GLES2 tests.
The decoder return integer is recorded uninterpreted, as its original wrapper
ignores it. A printed mnemonic alone is not a conformance or GPU admission gate.

Use this pinned offline oracle for future selected instruction-field and
compiler comparisons. Keep it outside FAST and all normal draw loops. No
encoder, shader, rendering gate, GLXGears profile or boot configuration changed.
The outstanding general event CODE84 program uses **PDS**, a different ISA;
this USSE decoder does not resolve that blocker. General 3D/2D and installable
driver completion remain unproved.

The later [texture-sampling field observations](TEXTURE-SMP-ORACLE-20261028.md)
use the same original SGX535/rev121 context. They do not resolve texture-state
descriptors or qualify a GPU sampling program.
