# Original SGX535/rev121 GLSL compiler CPU oracle — 2026-10-28

The original EMGD GLSL compiler can now compile authored constant and texture2D
fragment shaders offline for its explicit core4/revision121 target, with both
primary and alternate USP outputs. This is a reference oracle for implementing
reMESA; it is not a proprietary compiler dependency in the normal driver.
No GL/EGL, GPU, X11, kernel module or Mini 12 operation was performed.

## Artifact and observed ABI

Compiler: `/home/gama/sgx535-offline/phase9-full-legacy-driver-20261016/package-members/libemgdglslcompiler.so.1.5.15.3226`,
SHA256 `3aeb3a45b89b6e292875f84acd427930ce2e6170653970082d22e8123584a0dd`.
The ordinary SDK CLI's default540 target was not used. The backend helper at
Ghidra0x24340 explicitly writes core4 and revision0x79 into the parameters.
GLSLCompileToUniflex includes hardware compilation through0x58df0: its name alone
had not established whether it stopped at IR. These findings are from REA.

GLES2 glCompileShader and its compiler initializer supplied the actual static
parameter profile, replacing the exploratory zeroed/synthetic metadata:
fragment class5, parameter words at offsets18=9,1c=119,20=1,30=2,34=5;
request flags2, fragment alternate selector1, source option7=0x7fffffff.
The real driver obtains offset40 dynamically from client information >>2. The
oracle supplies an explicit finite64 budget, not a captured native value or a
claim about GPU task capacity. The alternate compile divides that budget by4.

The native compiler-format table has8 slots of40 bytes: four raw format1 tags,
selector0x60a, then0,4,0xffffffff,0. Its last DWORD is not written by the observed
constructor. The oracle keeps this320-byte CPU input read-only with a PROT_NONE
page immediately following it. This is NOT the hardware texture-state descriptor
or its fetch size. The separate existing GPU state constructor emits12 bytes.

The compiler's original CRT initializer was inspected: the constructor sentinel
is0xffffffff and Java registration pointer0. Its dependency is replaced by an
independently written library exporting ONLY alloc/calloc/realloc/free/locale.
No real services/GPU library is loaded. Bounds, allocation guards, caller-storage
guards and complete release are checked. No original executable code or
pseudocode was imported into reMESA production sources.

## Preserved failures and resolution

`/home/gama/sgx535-offline/glsl-texture-compiler-20261028/v1/` preserves all attempts.
The first loader attempt lacked realloc; its failure trace is retained. IR-only
constant and texture compilation then succeeded with no live allocations.
An absent compiler-format table caused a CPU null read at ELF0xc93cd (Ghidra
0xd93cd), instruction `[EDX+0x10]`, mapped through state offset0x288 to the
second hardware-argument pointer. Supplying bounded metadata removed the fault.
Zero resource budgets then failed the original regalloc range check. An explicit
16 budget with incomplete output/profile data exposed further original regalloc
errors, including `uOutIdx > 0` and a primary-attribute type assertion.

An observation-only longjmp wrapper recorded error8 and exact caller locations;
it always delegated the SAME jump/code to libc. No original check, option or
instruction was patched to suppress a failure. Static GLES2 defaults fixed the
parameter contract. A16 budget produced the texture primary object but failed
the alternate path, whose budget had become4. The selected64 budget and complete
native static defaults produce both objects successfully. The budget remains
an explicit compiler-input policy, not proof of a physical resource allocation.

## Reproducible qualification

`tools/remesa/qualify_glsl_compiler.py` builds the independently authored sources
in `tools/remesa/compiler_oracle/` in a NEW private directory. The original binary
is hash-pinned and privately copied, not modified or installed. All executions
are bounded, disable core dumps and record exact commands/stdout/syscall traces.
The final run does NOT use the diagnostic longjmp interposer.

Results: `/home/gama/sgx535-offline/glsl-texture-compiler-20261028/qualification-v1/RESULT.json`.
Three cases pass: constant, texture with unwritten-word probe0, and the same
texture with probe0xdeadbeef. Both texture outputs are byte-identical across those
probes and match the earlier observed runs. All cases report success, core4/rev121,
intact guards and live allocations0 after shutdown. Traces show no GPU device,
ioctl, socket or connect operations. All31 protected files remain unchanged.

Sizes are **USP allocations**, not effective USSE CODE: constant primary218 and
alternate206 bytes; texture primary246 and alternate246 bytes. The qualifier
checks only magic0x55535020, version31, declared body length, first descriptor
and core/revision prefix. The original USP constructor confirms this is an
object parser. Its decompiler reports unresolved switch recovery/max-restart
warnings; its entire block grammar is NOT qualified. Do not decode every8 bytes
of a USP object as an instruction or submit it as a USE program.

Example using the retained local toolchain (choose a fresh output directory):

```sh
python3 /home/gama/sgx535-worktrees/driver-resume-20261018/tools/remesa/qualify_glsl_compiler.py \
  --compiler /home/gama/sgx535-offline/phase9-full-legacy-driver-20261016/package-members/libemgdglslcompiler.so.1.5.15.3226 \
  --cc /home/gama/sgx535-offline/phase9-real-mesa-20261008/wrappers/cc \
  --qemu /home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386 \
  --runtime /home/gama/sgx535-offline/gcc14-i686-qualification-20260930/root/usr/i686-linux-gnu/lib \
  --output /home/gama/sgx535-offline/glsl-texture-compiler-20261028/new-qualification
```

## Next driver requirement

Recover the USP sample/result-unpack/output patch contract (or independently
establish the same typed final program), distinguish metadata from actual CODE,
and compare the original sampler/packing sequence with our existing SMP2D+WDF
encoder. Then bind the complete second-source/first-image/texture dependency
closure, coordinate and register resources, PDS, output/termination and private
MMU lifetime. Only a real textured draw with submission/completion/retirement,
readback/guards on SGX535/rev121 can approve execution. General NIR arithmetic,
texture/control-flow lowering, GL/GLES2 state/capabilities, accelerated desktop
and installation remain incomplete. No shader gate or capability was enabled.

Evidence: `v1/REA_COMPILER_FULL.json`, `REA_COMPILER_SNAPSHOT.json`,
`REA_GLES_NATIVE_PROFILE.json`, `REA_GLES_NATIVE_SNAPSHOT.json`; both REA sessions
closed after saving snapshots. Reuse them rather than repeating auto-analysis.
Key IDs: compiler backend `ev_ca4a9e7936a881e46b0d1171cb6211f43a680c194da8208e0a35ebd6a2c8c6b5`;
GLES native profile `ev_328d1aa6abfa5dfe62f6bbc5495d21d285b860f9d4498a820887472f823882cb`;
initializer `ev_f6562f2ecfd08ea3fe544ee351be0f0059742a102347c66d5d6fa5885888cf11`;
USP reader `ev_9cdab48acffd0fd660a9347f319e1b7464da85c7f2a5b7fd3b05dc1803303eda`.
