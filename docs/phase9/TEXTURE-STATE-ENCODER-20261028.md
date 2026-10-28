# Texture state field encoder and original GLES2 sampler observations

The shared userspace source now contains a checked constructor for the selected
SGX535/rev121 strided CPU state-builder profile. It is registered in the isolated
Mesa overlay. It does not enable texture sampling, change the existing Gallium
sampler/view admission, or connect the desktop to the GPU. The full2D/3D and
installable-driver goal remains incomplete.

## Implemented boundary

`mesa/sgx535/remesa_texture_state.c` and `.h` implement
`remesa_texture_cpu_strided_state_encode`. Inputs identify numeric format20/21
and filter0/1/2 of the original builder, with the selected numeric layout3.
The implementation derives the three words from fields; no vendor code,
executable template, proprietary shader or binary dependency enters the driver.

The constructor rejects null inputs, unsupported IDs, zero dimensions/address,
misalignment, rows shorter than width, fields exceeding their representations,
and a full-row span outside the32-bit address space. It checks before writing,
leaves all outputs unchanged on failure, writes exactly12 bytes on success and
leaves surplus capacity untouched. The4096 field representation ceiling is not
an advertised GL limit or physical layout approval. Owned allocation budgets,
mapping validation, visibility, GPU fetch footprint and retirement remain
separate mandatory backend contracts.

The existing semantic descriptor emitter still returns ENOTSUP, and nonempty
sampler bindings still block drawing. No FAST diagnostic, hash, checkpoint,
readback, wait or log was added. Existing physical copy/fill and gears code is
unchanged. This component supplies field construction needed by the future
common textured renderer; it is not a replacement for that renderer.

## Qualification

Evidence root:
`/home/gama/sgx535-offline/texture-sampler-semantics-20261028/v1/`.

- `codec-qualification/RESULT.json`:32 boundary/transaction/padding checks in
  native, UBSan and i386 variants, plus90 selected original strided-builder
  tuples matched in each variant. The vectors originate in the separately
  retained [original state-builder replay](TEXTURE-STATE-ORACLE-20261028.md).
- `codec-qualification/MESA_OBJECT.json`:new unit compiled with actual cached
  Mesa build flags. `staged-overlay/integration.json` binds106 staged files,
  including the new unit and Meson registration. No full new Mesa library was
  linked or installed by this qualification.
- `descriptor-regression/result.json`:135 existing host descriptor/gate cases
  pass in native, UBSan and i386. These retain ENOTSUP for hardware emission.
  `INVOCATION_CORRECTION.json` preserves an initial unsupported test-runner
  argument; the correct dedicated descriptor runner then passed.
- All31 protected files remain unchanged. No Mini12 or GPU operation occurred.

The pure unit test can also be run locally with:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Imesa/sgx535 \
  mesa/sgx535/remesa_texture_state.c \
  mesa/sgx535/tests/test_texture_state.c -o /tmp/remesa-texture-state-test
/tmp/remesa-texture-state-test
```

This command checks CPU field construction, not hardware rendering.

## Original sampler setter

REA followed the original EMGD GLES2 `glTexParameteri` to the internal setter
at Ghidra0x34460 (ELF/file0x24460). Binary SHA256:
`7859b0fa0cd3cd44ed18a0c056607066c07312307f8c6d86fd8e211f9a617ed0`.
Its own build string confirms SGX535/rev121. `GLES_TARGET_BINDING.json` records
that string, the retained Khronos enum-header hash and the recovered ABI.
`REA_GLES2_SETTER.json` retains evidence IDs
`ev_c3622cc79682f8e23916c40dc898e082038c8025d8efa474bb464e11ac44f5de`
and `ev_8e64a908a21bd7b3680842816922210d140c6a798ef8118516fa66a7f4525ecd`.

A bounded CPU-only replay calls the audited setter using its regparm(2) ABI,
EAX=context and EDX=GL_TEXTURE_2D, with pname/value on the stack. It maps the
original RX segment, executes no vendor constructors or GL context setup and
uses only owned synthetic CPU structures. Thirty-six valid combinations make
144 setter calls. All changed words, context dirtiness and untouched memory
are checked. `replay_sampler.c`, `SAMPLER_BUILD.json`, `SAMPLER_REPLAY.json`,
`SAMPLER_VERIFY.json` and raw stdout/strace retain reproduction evidence.
There are no ioctls or device opens, and the original binary is unchanged.

Observed per-texture cached fields:

| GL request | Original cached value |
| --- | --- |
| MIN_FILTER NEAREST | 0x001e0000 |
| MIN_FILTER LINEAR | 0x001e0400 |
| MAG_FILTER NEAREST | 0 |
| MAG_FILTER LINEAR | 0x00001000 |
| WRAP_S CLAMP_TO_EDGE / MIRRORED_REPEAT / REPEAT | 0x80 /0x40 /0, masked field |
| WRAP_T CLAMP_TO_EDGE / MIRRORED_REPEAT / REPEAT | 0x10 /0x08 /0, masked field |

Both image filters linear contribute0x1400, which matches one contribution in
the transfer-builder table. This does not by itself prove that cached sampler
fields can simply be ORed into every image layout. Strided pitch encoding
reuses bits occupied by wrap and minification fields in the ordinary cached
state. The final state-composition path and actual strided clamp behavior still
need to be established; the field codec does not invent that association.

## Public source boundary and next step

Pinned public sources were obtained from Maemo's `sgx-ddk-um` commit
`6fb7fa24d8aa3d7d97b835b42e464e95bbdf0588` and `xf86-video-pvrsgx` commit
`3d085e97b37446a3783a14fe04bb2fac14ea8dde`. The headers are dual MIT/GPLv2;
Nokia's Xorg sources carry an MIT-style notice. Fetch manifests bind URLs/hashes.
The Xorg file maps symbolic filters/repeat modes, but its composite section is
disabled and the fetched header does not define the newer API it references.
`PUBLIC_API_LIMITS.json` records why these names were not joined to EMGD IDs.
This source is context, not a proven accelerated desktop implementation to copy.

Next establish final image-layout/sampler composition and PDS/SA delivery using
the existing validated upload mechanisms, then qualify an owned textured draw
with physical identity, submission, completion, retirement, readback and buffer
integrity. Do not infer a GPU3-word fetch from a12-byte CPU write or manufacture
CODE padding. Keep normal desktop/EGL/GBM/image ownership and multi-client
isolation as required integration work, not optional follow-up after a demo.
