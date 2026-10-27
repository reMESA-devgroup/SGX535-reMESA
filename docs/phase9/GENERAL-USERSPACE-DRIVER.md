# General userspace driver: desktop OpenGL and GLES 2.0

The active target is hardware-accelerated desktop OpenGL and OpenGL ES 2.0
through a common Gallium backend, with an installable driver. X11 integration
comes later. The initial physical target remains antiX i686 on the Dell Mini
12, PCI 8086:8108, SGX535 rev121. No OpenGL version/conformance or general
acceleration is currently claimed.

## Reuse the physical results

The restricted gears path establishes real GPU execution, vertex processing,
lighting, depth, retirement and a read-only GEM-FD presentation contract. Its
fixed geometry/shader/profile admission is a regression fixture, not the
normal submission API for arbitrary applications. Preserve the physical
artifacts and previously qualified encoders; do not replace GPU transforms
with the legacy CPU transform/clip path.

The latest constant-normal upper-clamp proof used the exact 336-byte shader
already present in the gears profile and produced full COLOR 51/255/51 with
matching Z24 over three retired operations. These local records remain at:

`/home/gama/sgx535-offline/gears-state-upper-qualification-20261027T025000Z/PHYSICAL_RESULTS.json`

That component result does not qualify a general shader compiler or a complete
OpenGL implementation. The bounded 600-frame result published earlier also
remains valid; subsequent window-resize refusals are not general window support.

## Concrete missing contracts

| Layer | Current evidence | Required next behavior |
| --- | --- | --- |
| Shader IO and compilation | Typed encoders and restricted NIR profiles; old semantic pipeline links position + constant FS only | Common desktop/GLES linkage, then broader NIR operations and IO/resource allocation |
| Geometry and resources | Raw pre-VS snapshots and owned dynamic scene composition exist | Application-supplied VB/IB, dimensions and state reach the GPU through owned allocations |
| Physical submission | FAST fixes eleven BO roles and a twelve-draw scene; only bounded parameter updates are mutable | Typed dynamic scene submission with range, permission, PFN, isolation and lifetime validation |
| Completion | Physical receipts, retirement, cache ownership and read leases are demonstrated | Reusable BO/fence ownership for application draws, including failure/quarantine |
| Textures and render state | CPU contracts do not establish physical sampling or arbitrary blend/raster behavior | Physically qualify sampler/texture, blend, clipping and preservation features before advertising them |
| API entry | The DRI screen deliberately rejects general OpenGL admission | Both desktop GL and GLES 2.0 use the same qualified Gallium backend, initially offscreen |
| Installation | Mesa loader/EGL overlays exist; current qualified runtime is private | Reproducible i686 build and isolated package with an explicit matching DRM dependency and uninstall/rollback |
| 2D and X11 | X11 presentation currently copies completed GPU pixels | GPU 2D drawing/blits and later window-system integration; copying pixels to X11 is not desktop acceleration |

## First common implementation increment

`remesa_shader_linkage_prepare()` owns validated VS/FS semantic snapshots and
checks POSITION/COLOR0 producer-consumer compatibility. It distinguishes
constant, smooth and flat fragment input; an absent COLOR0 producer rejects a
varying FS. Revalidation compares complete canonical programs and derived IO,
not a digest. The old pipeline and all execution guards retain their behavior.

This interface does not yet support arbitrary GLSL varyings, NIR instructions,
shader BO allocation or physical draw submission. In particular, accepting a
semantic link is not GPU execution permission. Flat raster/provoking state
still needs its separate draw contract. The next integration must carry this
common linkage into program/resource preparation instead of selecting a
historical shader or the gears-specific callback.

With the project's pinned Mesa cache already prepared, the focused check is:

```sh
python3 mesa/sgx535/tests/qualify_shader_ir.py \
  --test linkage --cache "$MESA_CACHE" --output /tmp/remesa-linkage
```

It exercises native, UBSan and i386 builds plus actual Mesa NIR builder
producer/consumer graphs. It creates no GL/EGL context and performs no GPU
submission; it is an offline prerequisite, not the physical driver result.

## Installation and qualification direction

Keep shader compilation, scene construction and resource policy in userspace.
The matching minimal DRM/GEM/MMU, memory isolation and synchronization support
remain kernel dependencies; this work cannot turn the stock unextended gma500
kernel into a general SGX driver through a library name alone.

Build the Mesa DRI/EGL driver in a separate prefix first. Do not replace the
system libGL, alter the default boot, install the old private gears launcher or
advertise capabilities before their physical qualification. Exercise desktop
OpenGL and GLES contexts independently through the same backend; a successful
CPU fixture, context creation, software fallback or X11 image is not physical
GPU validation. Initial offscreen tests must prove submit, retirement, expected
readback and released backing on the SGX535 before packaging is promoted.
