# Render target and GLX progress

Parent: `x11-drawable-layout-20261021T211132Z`, manifest SHA256
`ec8ef8f426e8abb585a159a148609e3e90eef258d5ee8b754f27659ea031ebbe`.
This checkpoint does not qualify a larger GPU draw, depth, GLX context or gears.

## Implemented offline

- `remesa_render_target.{h,c}` encodes the selected linear BGRA8 CPU descriptor,
  background extent, distinct floor/ceil raster/termination extents and scene
  allocation metadata. Pitch is constrained to multiples of32. Logical width
  and pitch are separate: width33/pitch64 uses63 in the event extent,32 in the
  background width field, and different raster/termination tile rounding.
- Scene layout reproduces Xpsb's original small-dimension function, including
  the explicitly selected context+64 branch and its255 clamp. Cookie15 is not
  generated: the original function leaves it unwritten.
- Typed depth compare/write contribution and Z raster-prefix state encode
  only demonstrated CPU fields. UNDEFINED/CLEAR/DEFINED, cpp2/4, padded backing,
  address alignment, five split-mask relocation contributions and failure
  rollback are explicit. They do not initialize Z, establish tiled addressing
  or grant Gallium depth capability. The subsequent conditional reg480 OR
  remains outside this prefix; this is deliberately not a complete packet.
- `opengl_lab_drawable.{h,c}` connects real state-tracker default front/back
  attachments to existing resource ownership. Publication acknowledgement
  precedes swap; errors preserve ordering. Resize uses the existing resource
  allocator and retains outstanding old resource references. Still32x32, no Z.
- `remesa_xcb_present_oriented()` publishes a resource with explicit origin
  and without rotating attachments. The frontend owns the one swap; using the
  separately swap-owning presentation API as its callback would swap twice.

## Independent evidence

`COMPARE_ORIGINALS.py` compares C-emitted records with original x86 execution:
four target-prefix cases, twelve complete scene cases and eighteen depth-control
cases plus eighteen constructor/setter/raster cases, in native, UBSan, i386/QEMU
and actual Mesa archive variants. Target word1 is an unresolved original symbolic
relocation and is excluded. Unwritten scene cookie15 is excluded. Original shader
program bytes are not an implementation or exported oracle for this work.

Depth lifecycle replays separately establish that full lazy clear stores rawfloat
metadata, not a CPU-filled depth image. `surface+24=2` is DEFINED, not pending
clear. The tempting store atELF2efea is in a command allocation and is not this
surface field. Finalization can subsequently OR004000d0 through the saved reg480
pointer. These are CONFIRMED_CPU, not hardware bit-meaning proofs.

The actual Mesa default-framebuffer test creates a real context and performs two
CPU clear/readback/acknowledged-swap/front-readback frames. No GPU pixels or GLX
success can be inferred from that test. Its own results state this explicitly.

## Why1280x800 is not an increased constant

One BGRA8 plane requires4,096,000 bytes; independent front/back/Z backing alone
requires12,288,000 bytes before scene/heaps and page constraints. Existing retained
COLOR at40001000 cannot grow in place: role6 starts40002000. Additionally the
original scene producer requires119,584 bytes (30pages) at1280x800, while the
retained scene role is8192bytes. It requests16initial clear pages. These offsets,
table generation, initialization and references must move together. CPU descriptor
agreement does not qualify larger event/export behavior or bounds of observers.

## GLX

The original upstream Mesa25.2 DRI GLX path is DRI3, with handle sharing contracts
still absent in reMESA. General DRI admission remains ENOTSUP. A matching private
GLX stack compiled successfully, using authenticated isolated i386 dependencies
from `glx-dependencies-20261022T023107Z`. Configure and978 build steps completed
with exit0. A bounded i386 loader probe resolves seven real GLX/GL entry points
from this exact library, with the matching libgallium, without opening X/DRM or
creating a context. Nothing is installed on Mini12 or the host system.

The linker's RWX warning is retained: upstream `src/mapi/entry_x86_tls.h:55`
declares its `wtext` section as `awx`. ELF inspection locates that section in
the RWE segment. No warning suppression, permissions override or system change
was introduced. A compiled library is not proof of runtime GLX admission.

### Reproduced default-window draw blocker

The actual Mesa-context probe now compiles/links its own position VS and green
FS and calls glDrawArrays into a fresh default BACK attachment. GL error is0;
the driver returns ENOTSUP before any shader preparation or submission. The
observed raster state has half_pixel_center1, bottom_edge_rule1 and vertex
clamp1. The restricted physical FBO profile explicitly rejects bottom_edge_rule1.
Mesa `st_atom_rasterizer.c:275–281` sets this field from Y_0_TOP orientation and
the clip origin. It is not an arbitrary dirty state to delete.

The original failed attempt and assertion logs remain in
`frontend-default-draw-20261022T024700Z/`. The final test deliberately asserts
the rejection and zero dispatch; its PASS means correct rejection, not a draw.
The actual archive probe and its separately instrumented frontend/probe UBSan
variant pass. Semantic-only VS status reports0 encoded bytes without a numeric
mapping; FS reports8. No16-byte VS emission or GPU retirement is claimed by this
rejection test. The earlier physically qualified VS evidence is unaffected.

GLXgears additionally requires depth, transform/lighting/normal/material shaders,
variable geometry, preserved framebuffer and repeated submissions. The current
physical milestone remains a restricted original-OpenGL triangle, not gears.

## Execution boundary

No SSH, X connection, MMIO, GPU submission, module operation, installation or
boot/configuration change occurs in this checkpoint. The previous authenticated
X11 readback publication and physical triangle remain preserved evidence.
The28 gates and ADMISSION remain unchanged. New physical experiments need their
concrete packet, memory bounds, recovery and explicit owner authorization.
