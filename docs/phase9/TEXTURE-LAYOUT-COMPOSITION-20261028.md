# Original texture-layout composition and selected stream CPU profile

The shared field encoder now reproduces the selected original GLES2 stream
sampler composition with explicit stride and independent minification and
magnification image filters. The original raw strided constructor is preserved.
No sampler/view gate, descriptor emitter, GPU submission, boot or installed
library changed. General userspace3D/2D and installable desktop integration are
still incomplete.

## New evidence changes the next step

The earlier raw transfer constructor and cached sampler fields must not simply
be ORed together. The final GLES2 producer masks the overlapping pitch fields
before adding the explicit stride contribution. This is now observed in the
original SGX535/rev121 library and checked through bounded CPU execution.

Evidence root:
`/home/gama/sgx535-offline/texture-layout-composition-20261028/v1/`.
`REA_LAYOUT_COMPOSITION.json` retains five analysis records. The exact library
is the same pinned `libGLESv2.so.1.5.15.3226`, SHA256
`7859b0fa0cd3cd44ed18a0c056607066c07312307f8c6d86fd8e211f9a617ed0`.
`GLES2_ANALYSIS_SNAPSHOT.json` retains the completed queries for reuse; the
native session was closed and its Java process exited.

REA followed `glTexImage2D` to level allocation/setup and then inspected the
final texture-stage producer. Relevant records:

- `ev_9983758b9c4eb66e6600ab6791bfe8b4024359d68385c8c3f1c67db67314cc9c`:
  glTexImage2D stages level contents; it is not itself final state upload.
- `ev_622e4f892467b448782e54021a49cc099cb3448b2998ffdd327bb99ae3600ca0`:
  original normal image-layout selection, Ghidra0x3b300/ELF0x2b300.
- `ev_668c47b2950b8641b84a095a0ac9f242bb714fcdae940a0f896e75d9b29629cb`:
  final stage-state construction, Ghidra0x3c630/ELF0x2c630.

Normal image-layout selection depends on power-of-two dimensions, mipmapping,
image format, cube target, context options and imported backing. That whole
path is not qualified by the selected stream replay below.

## Controlled final-state replay

`replay_composition.c` privately maps only the original RX PT_LOAD, verifies
ELF/class/machine/segment/file size, and calls the audited setter and final
stage producer with owned synthetic CPU structures. The launch checks the
binary SHA. No original constructors, GL context initialization, allocation,
GPU synchronization or submission functions execute on this selected branch.
The guest alarm is5seconds and parent timeout15seconds. `BUILD.json`,
`REPLAY.json`, stdout/strace and `VERIFY.json` bind the commands and results.

The target is **GL_TEXTURE_STREAM_IMG=0x8c0d**, not
GL_TEXTURE_EXTERNAL_OES=0x8d65. `TARGET_ENUM_BINDING.json` binds those distinct
names to the retained licensed Khronos extension header. There is no new IMG
or OES extension implementation in reMESA.

Eighty CPU cases cover all four nearest/linear min/mag combinations, numeric
format20/21, stride override off/on and five dimension/pitch tuples:
1×1/1pixel,2×2/4,640×640/640,1280×800/1280,641×479/644.
The fake streaming metadata supplies the selected image state and address;
this does not prove how a real stream allocation populates that metadata.
All changed context/texture cells and untouched memory are checked. There are
320 setter calls,80 composition calls, no ioctl/device open and no GPU work.

For these non-mipmapped clamped inputs the cached state is:

```text
cached_word0 = 0x03fe0090 | (min_linear<<10) | (mag_linear<<12)
final_word0 = cached_word0                           (no stride override)
final_word0 = (cached_word0 & 0xffe3fe00) | stride0   (stride override)
```

Here stride0 is the raw selected constructor's first word with numeric filter0.
The remaining two words carry the selected image word and four-byte-aligned
address. With explicit stride, the retained cached contribution simplifies to
0x03e20000 plus the independent filter bits. Architectural interpretation of
all retained control bits is still incomplete. Clamp bits must not be restored
over pitch bits after the mask. Neither80 correct CPU outputs nor these formulas
prove physical wrap/clamp behavior, fetch footprint or correct rendering.

## Implementation and checks

`remesa_texture_cpu_stream_sampler_encode` in the existing
`mesa/sgx535/remesa_texture_state.c/.h` reuses the raw checked constructor for
address, row, dimension, representation and span validation. It adds only the
observed selected composition contribution. Min/mag flags must be exactly0/1.
It preserves destination and written count on every error and writes12 bytes
without touching surplus capacity. No execution permission follows from it.

`codec-qualification/RESULT.json` records54 boundary/transaction/padding checks
and130 original tuples (90 retained transfer plus40 stride-composition) in each
of native, UBSan and i386. `MESA_OBJECT.json` binds compilation with actual Mesa
flags. The fresh106-file overlay registers the unchanged unit; no full new Mesa
library was linked or installed. All31 protected files remain unchanged.
Existing descriptor/Gallium sampling gates stay ENOTSUP. The prior135-case host
descriptor qualification remains applicable because all of its bound sources
are unchanged; it is not a new physical sampling result.

## Existing PDS route and remaining integration

The preserved `remesa_pds_structure` code already has a texture DATA grammar:
a state pair in logical bank0, image-address relocation and descriptor in
logical bank1, with two observed relative orders. Its typed texture-output
encoder emits the existing A2/class64 CPU forms. Those contracts will be reused;
this work does not reconstruct or replace them, resolve symbolic relocations
from untrusted values, or promote CPU index spelling to a verified GPU read.

Next bind the newly composed state to an owned texture allocation and this
existing PDS/SA delivery path. Establish its coordinate/result binding, source
footprint, code/task extent and retained lifetime before the physical textured
job. Any physical qualification must include identity, submission, completion,
retirement, readback and buffer integrity at the required sizes. Do not infer
3-word hardware consumption from a12-byte CPU write, or pad CODE to reach a
reservation. Preserve full general GL/GLES2, actual desktop/EGL/GBM/images,
multi-client isolation and recoverable installation as the required end state.

The later [constant/texture upload-image work](TEXTURE-SA-UPLOAD-20261028.md)
identifies a separate original constant-gather route. Its checked image builder
retains all execution gates; the primary PDS texture grammar is not assumed to
be interchangeable with this path.
