# Original prefetched primary PDS construction

The primary PDS constructor for the original rev121 GLES2 fragment path has
been recovered and independently implemented as `remesa_pds_prefetch_encode`.
It prepares the input/control/texture-state path required by the selected
`mov.skipinv.end o0, pa0` texture fragment reference. It is not connected to
runtime admission or submission; physical sampling, resource delivery and full
OpenGL remain unqualified. Earlier primary and secondary constructors are
unchanged. No Mini 12, GPU, module or boot operation occurred.

Evidence root: `/home/gama/sgx535-offline/prefetched-primary-pds-20261028/`.
Authoritative replay and qualification: `v2/`; REA and Mesa object: `v1/`.
`RESULT.json`, `replay.stdout`, `QUALIFY_COMMANDS.json`, `MESA_OBJECT.json`,
`REA_PRIMARY_PATH.json` and `REA_PRIMARY_SNAPSHOT.json` preserve results.
The REA session was closed with its snapshot saved.

## Trace from real drawing

The pinned original GLES2 library is the same artifact used by the
[USP finalizer oracle](USP-FINALIZER-ORACLE-20261028.md), SHA256
`7859b0fa0cd3cd44ed18a0c056607066c07312307f8c6d86fd8e211f9a617ed0`.
Ghidra addresses are ELF addresses plus0x10000. REA followed:

- `glDrawArrays`0x1af20: shader/state update0x47b60, then draw path0x1a750.
- Draw packet0x486d0 selects secondary constants0x465d0 separately from
  primary fragment input preparation0x45b00.
- Primary adapter0x45b00:
  `ev_2a1f3e7c7461f87404f9760d80de280b23725635659a7cb3d25356f3d5271138`.
- Pure primary writer0x27cd0/ELF0x17cd0:
  `ev_7ef1c52a8829f4ec0189b8867768441129cc446582cda8859f2a9fdfcaf6519a`.
- Its interleaved DATA allocator0x276e0, DATA extent helper0x27750 and
  operand-field writer0x27790 were checked through assembly as well as
  pseudocode. Their register arguments are not ordinary C stack parameters.
  The top-level primary writer has three ordinary stack arguments.

The native adapter receives the finalizer's input and sample records. It
combines sample-record OR/keep values with each texture's existing three-word
state: `patched[j] = (state[j] & keep[j]) | or[j]`. Two u16 selectors at the
start of each28-byte sample record select the native state triple using
`selector1 + 4*selector0`. Their wider meanings are not inferred here.
In the selected texture reference both selectors are0. Original format20 uses
OR words `[0,0x0c000000,0]`, format21 `[0,0x0d000000,0]`, with keep words
`[0xffffffff,0xe0ffffff,0xffffffff]`. These records are not instructions or
allocations; their pointers and backing texture lifetime still need binding.

The primary writer consumes task words at host offsets8/12/16, input count
at20, input controls at24 and sample indices at0x218. Each sampled input refers
to one already patched three-word state. This differs from the two-image
secondary constant DMA route. No secondary PDS was substituted for primary.

## Independent implementation

`mesa/sgx535/remesa_pds_prefetch.h/.c` accepts bounded typed CPU values:
three task words, at most16 input records, at most16 state triples. These
limits are implementation policy, not a claim about hardware capacities.
The function validates ranges, counts, sample indices, reserved fields,
capacity and all input/output aliases before publishing. It authenticates
neither GPU addresses nor task resource allocations; its caller must supply
validated retained bindings before any future upload can be admitted.

The writer's allocator chooses between two interleaved CPU banks by pairs.
Task words initially occupy physical DATA cells0,1,8. A nonsampled control with
low nibble15 remains eligible for merging until a sampled input consumes it;
a later nonsampled control with another nibble does not cancel eligibility.
A merge replaces mask0xbff in that earlier control and removes a redundant
control allocation/instruction. Sample state triples still occupy three
additional logical cells. The implementation preserves these original rules,
including mixed and nonadjacent eligible cases.

CODE is calculated from the original helper's independent selector/cursor
fields: task selector5, control selector2, texture triple selector4, followed
by the observed AF trailer. No raw CODE input, embedded program template or
padding instruction is accepted. These are CPU field observations, not a
new PDS semantic decoder or proof of physical source-port behavior.

Original entry alignment and DATA rounding are16 bytes in this constructor.
That is separate from the TA descriptor's capacity/alignment contract and
from GPU fetch bounds. Unknown DATA holes are initialized to zero with mask0;
known writes and CODE have mask0xff. Reserved capacity, DATA extent, CODE size
and total effective size are returned separately. Tail bytes remain untouched.

Examples observed in the original constructor:

| Input records | DATA bytes | CODE bytes | Effective bytes |
| --- | ---: | ---: | ---: |
| No inputs |48 |8 |56 |
| One sampled input |48 |16 |64 |
| Two sampled inputs |64 |24 |88 |
| Sixteen sampled inputs |304 |136 |440 |
| Eligible nonsampled input then sampled input |48 |16 |64 |

## Qualification

`replay_primary.c` maps only the original first RX segment after exact ELF and
SHA checks and calls the pure writer. No original DSO constructor, GL/EGL,
GPU allocator or shader runs. Five-second guest alarm and30-second parent
bound preserve terminal executions; traces contain no GPU device, ioctl or
network calls. Backing guards and all bytes outside the effective output stay
intact, including entry-alignment gaps.

There are102 logical recipes (counts0..16 and six input patterns), each at
four original host alignment offsets:408 original replays. Patterns cover
all sampled, all eligible nonsampled, both mixed orders and eligible/ineligible
nonsampled records plus an eligible R/ineligible R/T sequence. Native, UBSan and i386 each compare408 outputs to the
original, checking every known word and every untouched original DATA hole.
The independent host output is also exercised at four byte alignments.
Native/UBSan pass290960 checks each, i386290876; the difference is the size of
the output layout structure in rejection-preservation tests. Failure paths
preserve bytes, masks and layout. All31 protected files remain unchanged.

The initial v1 qualification covered340 cases. v2 preserves and extends it
with explicit nonadjacent merge cases, without changing the implementation.

The fresh Mesa overlay stages110 files and the new unit compiles with the
existing real Mesa i386 flags. This is not a fully linked/installed new Mesa
library. No normal rendering diagnostics, hash, wait or gate was added.

## Next requirement

Bind the finalizer metadata to independently constructed coordinate/input
controls and patched state triples. Authenticate complete texture, vertex,
constant and executable allocation ranges, stage/task resources and ownership;
retain them through fences and retirement. Then integrate the primary program
with the existing secondary constant route and shader, establish private-MMU
execution exclusion/restore, and qualify a real textured draw at required sizes.
CPU equivalence alone does not enable sampling or establish an FPS gain.
Desktop presentation, full general GL/GLES2, multi-client isolation and a
recoverable installable driver remain part of the unfinished objective.
