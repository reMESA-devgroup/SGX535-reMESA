# Bounded PDS generator comparison: SGX544 is not an SGX535 oracle

The original OMAP SGX544 services binary supplies named PDS generators and a
small arithmetic helper, but no SGX535 PDS byte-to-text decoder is established
by this investigation. No vendor program or new instruction words are added
to the driver. The SGX535 event CODE84 requirement remains open.

Target:
`/home/gama/sgx535-gfx/references/omap5-sgx-ddk-um-linux/targetfs/lib/libsrv_um.so.1.9.6.0`.
SHA-256: `3edc7b7b64c87aa604beecaaf62fc0f8186e9fc054b22079bb398bafaa226be1`.
Only the shipped binary, symbols and debug type information were inspected;
the source paths in its assertion strings were not opened. Excluded
confidential/PSP2 sources were not used.

## Event program observations

REA/Ghidra evidence
`ev_dcf67bc5643ba1cb844056a42143fd157fd4354822ae19bd02b0467334d43317`
identifies the complete `PDSGeneratePixelEventProgram` at ELF159f0,
Ghidra259f0. The original function:

- checks16-byte buffer alignment;
- copies100 bytes from its original static event table;
- sets `ui32DataSize` to48;
- replaces selected DATA words using three named USE task-control groups;
- publishes the aligned DATA pointer and returns buffer+100.

`PDSGetPixeventProgSize()` independently returns100 in evidence
`ev_1854825c978c2eaa0e79d0f57446e93f842c40656a85a9b6487c4988b8116a19`.
The committed copy extent and reported DATA extent therefore leave52 bytes
after DATA. They do not measure a GPU fetch width or establish allocation
capacity; a symbol's padding/alignment gap is not such proof either.

Our retained SGX535 producer instead reserves400 and commits148 bytes,
with DATA64 and CODE84. Its descriptor field0c=16 must not be reinterpreted
using the OMAP profile's DATA size or task-control layout. Matching alignment
alone does not establish compatible instruction forms, ports or descriptors.

## MOVS arithmetic observations

Evidence `ev_7bf020f9356b9b5213819656dbb952730ec60100e96ee9a16c393fd209b0991b`
identifies `PDSEncodeSimpleMOVS` at ELF Thumb entry1591b, Ghidra2591a.
The complete nontruncated value flow has100 operations. Its debug signature
has four unsigned arguments: destination, DS constant index and two extra
fields. The original arithmetic groups the index by four and uses quotient
contributions shifted12 and18, with different paths for destinations2,4 and
other values. These are CPU encoding observations in this binary, not a
claim about which fields/values are legal hardware operands.

`REPLAY_OMAP_MOVS.py` runs the original Thumb function in the existing private
Unicorn installation. It maps only the original ELF segments and private
guest stack, permits execution only inside the recovered function and its
return sentinel, bounds each call to512 instructions and checks outer stack
canaries. No ABI function is replaced; no external calls, device access,
native vendor-library loading or GPU operation occurs.

All1024 sampled calls match an independently written numerical model:
destinations0/2/4/5, indices0..63 and both extra fields0/1. Every write stays
on the guest stack. This validates those sampled CPU results; it neither
declares those whole ranges hardware-valid nor supplies a general assembler.
It does not justify substituting this helper for the SGX535 pair/selector
codecs or using it to invent the still-unknown CF/C7/F7 event fields.

Evidence root:
`/home/gama/sgx535-offline/event-gles2-fields-20261031/v1`.
See `OMAP_REA_COMPLETE.json`, `OMAP_REA_SNAPSHOT.json`,
`OMAP_SYMBOL_LEAD.json` and `OMAP_MOVS_CPU_RESULT.json`. This comparison narrows
the usable oracle boundary; it grants no SGX535 execution, GL/GLES, desktop,
resolution, performance or installable-driver approval.

## Reanalyzed compiler CF candidate

The additional narrow scan of the existing original EMGD GLES2 and GLSL
compiler binaries found nine selected-class sites in GLES2 and one CF site
in the compiler. These are locator hits, not a complete generator inventory.
The compiler target SHA-256 is
`3aeb3a45b89b6e292875f84acd427930ce2e6170653970082d22e8123584a0dd`.

The first REA attempt was interrupted during analysis; its process and exec
handle were later absent and its evidence ledger had zero function records.
That empty snapshot is preserved. After the user authorized repeating the
analysis, the same target/procedure was analyzed successfully.

Evidence `ev_8e02f34cb645980a1adb100684d94308be756ed6e325ae463a88a5254f6c145b`
contains the whole function at Ghidraeb4a0, including the candidate store at
Ghidraeb9ae (ELFdb9ae):1610 body bytes across five ranges,456 nontruncated
value-flow operations. It writes8 at offset34 andCF000000 at offset38 of
the operand storage referenced by a compiler record's114 field. Other
records use4f000000,4f800000 and47800000 in the same value position.
All record offsets and hexadecimal values in this description are base16.

Two focused callees establish the surrounding record behavior:

- `ev_9cf3a891c875fe1df406b17d00b3b5ca957743a52b25395a0b6f1befd9681559`:
  Ghidraf40c0 sets the record's operation kind and allocates/initializes its
  operand array in0x34-byte (52-byte) strides;474 nontruncated operations.
- `ev_183e29670b7986dc100b94d7ae803d3ac176e8720ff97443c9dacaca019d812e`:
  Ghidra53f90 links records through180/184 and updates list head/tail/count;
  54 nontruncated operations.

This is compiler intermediate-record construction, not a direct PDS CODE
writer. ReinterpretingCF000000 as IEEE float gives−2^31, while4f000000 and
4f800000 give2^31 and2^32. That numerical pattern supports an integer-conversion
constant interpretation, but the exact operation-kind names and later compiler
lowering remain unestablished. No PDS bit-field meaning follows from this hit.
This finding excludes that direct-emitter lead; it does not prove that no PDS
code or decoder exists elsewhere in these stripped binaries.

Full records and the completed provider snapshot are retained as
`COMPILER_REANALYSIS_EVIDENCE.json` and `COMPILER_REANALYSIS_SNAPSHOT.json`.
No compiler main, GPU or GL/EGL entry point was executed. This result does not
change an encoder, geometry, shader or native execution gate.
