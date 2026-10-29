# Original rev121 USP finalizer and prefetched fragment reference

The original GLES2 USP parser/finalizer now turns the retained constant and
texture2D primary objects into selected final USE bodies in an isolated CPU
replay. The texture reference uses a prefetched primary input: its entire USE
body is `mov.skipinv o0, pa0`. It does not contain an explicit SMP/WDF pair.
The existing independent reMESA MOVE encoder reproduces that body exactly,
including the native full-mask/no-alpha termination variant. No runtime shader
admission, GPU module, normal render loop or Mini 12 state was changed.

Evidence root: `/home/gama/sgx535-offline/usp-texture-finalizer-20261028/`.
Authoritative run: `qualification-v2/RESULT.json`, `COMMANDS.json`, four case
subdirectories and `decode.stdout`. Compiler regression: `compiler-regression/RESULT.json`.
REA: `v1/REA_FINALIZER_COMPLETE.json` and `v1/REA_FINALIZER_SNAPSHOT.json`.
The session is closed; reuse its snapshot rather than repeat the investigation.

## Original artifact and observed contract

Original library:
`/home/gama/sgx535-offline/phase9-full-legacy-driver-20261016/package-members/libGLESv2.so.1.5.15.3226`,
SHA256 `7859b0fa0cd3cd44ed18a0c056607066c07312307f8c6d86fd8e211f9a617ed0`.
Ghidra addresses below are rebased by0x10000 relative to ELF addresses used by
the CPU harness. The original library is privately copied, never patched.

REA established these APIs and actual native consumers:

- 0x4be80 creates a0x1d4-byte context with allocation/free/print callbacks
  (`ev_2c8eafe0a5efdbaa020537b9a16b9941fe0f5d5a5ee8ace15f7e4ec394b8d42a`).
- 0x4bef0 invokes the USP parser;0x4bf50 sets five format DWORDs per sampler,
  plus two controls. These five words are neither the compiler's40-byte format
  record nor the GPU's12-byte texture state.
- 0x4bfe0 sets context+0x1cc;0x4c000 sets context+0x1d0. Names/complete semantics
  remain unknown. Native fragment adapter0x44240 supplies1 and0 for flags0,
  full color mask, no alpha test. No checks were bypassed.
- 0x4c020 finalizes (`ev_deab0cbcd6d404ffafd0761d42d87987b864e120ee637bbdd4ebd646066ed6b1`);
  output builder0x50c10 returns a single owned allocation with a0xa0-byte
  header, CODE count u16 at0x0c and CODE pointer at0x10. Count*8 is effective
  CODE length, not the allocation size. Core/revision at4/8 are4/121.
-Actual native consumer0x44240 copies exactly count*8, consumes input records
  through u16 count0x40 / pointer0x44 and sample records through count0x90 /
  pointer0x94 (`ev_af35af98a00bfff497396d316cde00e48038a92b610d86933eb54cda8f8c8d74`).
  Both record types occupy28 bytes; some padding is unwritten. Whole-record
  byte identity is deliberately not asserted.

The bounded numeric scan `v1/FORMAT_CANDIDATES.json` found three candidates;
it alone did not establish their identities. The retained native glTexImage2D
analysis independently selects0x60e20 for GL_RGBA/GL_UNSIGNED_BYTE and0x60e60
for GL_BGRA/GL_UNSIGNED_BYTE. Their numeric pixel IDs at+0x20 are21 and20;
patcher input starts at+0x0c. The third candidate is an RGB path and was not
used as RGBA. No broad texture-format capability claim follows from two cases.

The original GLES2 initializer has one constructor (0x4bdb0), which uses
pthread_once to create a TLS key via0x4bd80; its Java-registration pointer is0.
It is CPU-only. Unlike the separate compiler DSO, its constructor list is NOT
empty. RTLD_NOW loads it against independently authored memory services and35
aborting device/EGL traps. No original services or EGL implementation is loaded.

## Replay and independent comparison

`tools/remesa/qualify_usp_finalizer.py` pins the original library, SDK decoder,
decoder shim and two previously qualified USP inputs. It builds only own CPU
harness/dependency sources, uses a read-only input ending immediately before a
PROT_NONE page, guarded bounded allocations, validated owned output ranges,
process timeouts and disabled core dumps. Logs preserve exact commands and
system calls. All four cases exit normally with live allocations0, intact
guards, no GPU device, ioctl, socket or connect operations.

Selected output, same for numeric formats20/21:

| Source | Original HW allocation | Effective USE | Input records | Sample records |
| --- | ---: | ---: | ---: | ---: |
| Constant primary USP218 bytes |228 bytes |8 bytes, `mov.skipinv o0, sa9` |1 |0 |
| Texture primary USP246 bytes |240 bytes |8 bytes, `mov.skipinv o0, pa0` |1 |1 |

For the native plain fragment path, HW flag2 requires a real terminating NOP;
otherwise native code sets bit18 on the final instruction. These selected
outputs have flag2 clear, so both terminated bodies remain8 bytes and decode
as `mov.skipinv.end`. The harness rejects the unqualified flag2 branch; no
padding instructions are manufactured. Eight raw/terminated instructions were
checked by the original SGX535/rev121 SDK decoder, including exact byte binding.
Its return0/1 is END-related, not a general validity boolean.

The qualifier also compiles the existing `remesa_usse_cpu_move_encode` and
compares its guarded8-byte outputs directly to original texture outputs. No
new opcode or production API was needed. The changed shared allocation helper
was checked by rerunning all three original GLSL compiler cases successfully.

The v1 compile attempt failed warnings-as-errors due misleading indentation;
its source/logs remain intact. v2 exploratory replay passed; qualification-v1
passed the initial tool; qualification-v2 additionally proves the existing
independent driver encoder matches. No failed result was overwritten.

## Next integration requirement

For this profile the sample is delivered through the native fragment input
path before USE executes. The native adapter marks the sampled input and uses
its sample record when preparing PDS/interpolation state. This redirects the
next task toward that PDS contract and resource delivery; inserting explicit
SMP/WDF into this8-byte body would change the compiler-selected pipeline.
The independent explicit SMP/WDF encoder remains valid within its earlier
CPU qualification scope, for programs that require it.

Still required: independently construct the selected PDS sampling/interpolation
configuration, connect coordinates and retained texture/state/constant resources,
verify output/task capacity and execution closure, integrate safe MMU ownership
and retirement, then qualify a physical textured draw at the required sizes.
Raw USP objects and these CPU references are not admitted GPU submissions.
No physical result, FPS gain, general shader compiler, complete OpenGL/GLES2,
accelerated desktop or installable driver completion is claimed.
