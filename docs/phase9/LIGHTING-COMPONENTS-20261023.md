# Original lighting controls and arithmetic bodies

**IMPLEMENTED_OFFLINE / CONFIRMED_CPU.** Full lighting, physical depth and
hardware glxgears remain unqualified. Zero GPU submissions, MMIO or SSH here.

The existing typed USSE arithmetic module now independently emits:

- Selected B5 CPU controls: four signed8-bit operands or four2-bit component
  selectors per operand; corresponding mode bits, flags0x200, trailing offsets0.
- Restricted DOT3 body: class2 aligned sources, class1/index0 destination,
  kindB/major2/sub0 and repeat3 from flags31.
- Restricted RSQ body: class2 source indices0..3, class1/index0 destination,
  kind5/major1/sub1, scalar mask1 and numeric modifier2 from flags201.

These are calculated fields, not historical byte arrays. Unknown formats,
indices, controls, aliases and insufficient capacity fail transactionally.
The bodies are8 bytes each and EXCLUDE surrounding controls/MOVE/export/task
resources. The full NIR/Gallium lighting path remains ENOTSUP. No capabilities,
admission controls, physical state or existing execution gates are promoted.

## Independent evidence

Original psb_dri.so SHA256
74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8.
The bounded CPU harness/services/software target are inherited unchanged from
mvp-arithmetic-recovery-20261023T031000Z. Its software target is not proof of
SGX535 rev121 hardware behavior. Public Mesa IR is authored here; the original
frontend/compiler supplies independent comparison outputs. Generated original
CODE is an oracle only and is never copied into the implementation.

Eight valid DP3/RSQ variants vary one source swizzle or scalar component.
Eleven B5 entry/exit pairs were captured. Left DP3 xyzw→yxzw changes two fields
from numeric1 to selectors0x481, packed0xa1, and mode bits1/2. Right-source change
changes another packed field and mode bit0. All four RSQ component selections
retain the same B5; only the arithmetic source index changes. Thus B5 cannot be
identified simply as the direct source swizzle from this observation.
One additional valid DP3 reverses inputs, independently confirming its source
packing. Six arithmetic body oracles cover DOT3 both source orders and RSQ x/y/z/w.

Ghidra confirms the exact B5 writer G0x261c79 (ELF251c79), evidence
`ev_3e96aaf059bb92f5eb1f172037625230ced80f3eec3fd9f8e9d39b1c1e4a1f96`.
Selected dossier is REA_B5_PRODUCER.json; full retained query snapshot is
REA_SNAPSHOT.json. A first query accidentally used an ELF address as a Ghidra
address and resolved a different function; it is not used as B5 evidence.

The producer converts each class15 3-bit selector to2 bits, checks class8 values
against[-128,127], packs four bytes at24/16/8/0, and mode flags at3/2/1/0.
The final flags expression reduces, for admitted flags bit3=0, to major31,
bit25 and bit20. The alternate flags arm and nonzero trailing offsets are excluded.
Ghidra repeat-control, source-helper, modifier-helper and MOVE dossiers are also
preserved. Register-sensitive modifier semantics use assembly, not the malformed
recovered register prototype: ELF241a79 returns the low two EAX bits.

Additional original-CPU traces follow values backward:
`config → setter1f2d1c → IR operands(stride0x34) → transfer23d64c → assembler → B5`.
At1f2d6d, config.mode==1 selects class15 and mode1; the other arm selects class8
and mode0. This independently supports deriving the encoded mode from the
admitted operand format, rather than merely correlating output bytes.
Records in B5_*PRODUCERS.json/B5_OPERAND_ORIGINS.json include exact addresses.
Their provenance is explicitly the last observed x86 store: synthetic service
copies and sub-dword stores are not a complete dynamic SSA history.

## Validation executed

Native, native UBSan, i386/QEMU and rebuilt real Mesa archive:
-321 checks per environment against11 original B5 writer outputs.
-128 checks per environment against6 original DOT3/RSQ body outputs.
Actual-archive regressions: coefficient178, MAX54 and old ADD/MUL68 checks.
Static driver archive and DRI module rebuilt successfully. Exact commands,
outputs, stdin hashes and source/archive identities are retained. No full-Mesa
UBSan, physical i386 test, unexecuted NIR rerun or GPU success is implied.

## Remaining useful work

The primitive CODE fields are now encodable, but the valid complete programs
also contain B5 and MOVE records. Do not append a sampled prefix/tail as a
program template. Close how the original scheduler derives the initial and
restored B5 configurations, then compose the demonstrated typed operations.
The trace reaches config writes1f5ad7/1f5aea/1f5afd/1f5b10 and their local
origins, including1f577e. GHIDRA_HANDOFF.md gives the exact next instruction.
Matrix broadcast/uniform transport, VS COLOR0 export, FS varying input and
physical qualification remain separate. The prior owned264-byte depth control
implementation is preserved; its larger native job is still not admitted.
