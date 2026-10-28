# Destination-only native solid fill

The native fill candidate extends the qualified copy lifecycle with a typed
solid-color rectangle. Its physical qualification is recorded below when
available; offline tests alone do not approve GPU rendering. The common
userspace 3D/2D driver and installable package remain incomplete.

## Producer evidence and packet

Artifact root: `/home/gama/sgx535-offline/twod-native-fill-20261028/v1/`.
The existing original `libEMGD2d.so.1.5.15.3226` target, SHA256
`d0f7c06c94f3a5c8e122727d60dfd6a87e020849838b257b8fc8307cf057bf59`,
was called under QEMU i386 with the same private UM stub used for the copy
oracle. Destination metadata is guarded and source metadata is absent from
this fill request. ROP0xf0 and color0x7f123456 produce six payload DWORDs:

1. Destination8888 surface header and stride.
2. Destination offset in the request window.
3. BLIT with ROP0xf0f0 and USE_FILL.
4. Packed fill color.
5. Destination coordinates.
6. Rectangle dimensions.

The already established UM boundary appends FLUSH0xf0000000, giving seven
words/28 effective bytes in the existing40-byte command capacity. The unused
12bytes are zero and never submitted. No artificial CODE or command padding
is emitted. `oracle/RESULT.json` records eight original-producer calls plus a
null negative control, private dependency binding and input canaries. Each
captured body matches the independently authored encoder byte for byte.
Original register definitions corroborate USE_FILL and the color operand.
This is CPU execution of the producer, not execution by the GPU.

## Safety and implementation

`--with-native-copy --with-native-fill` selects a separate normal build with
its own default-off `remesa_twod_fill_enabled`. Mixed LAB profiles or fill
without the normal copy profile are refused. Existing ten-word legacy and
nine-word copy packing remain unchanged. The fill macro admits seven-word
native transactions with the same owned ALPHA publication/restoration.

Private ioctl8 accepts a104-byte version1 typed request: owned destination ID,
immutable surface metadata, rectangle and color. No raw VA, PFN, user packet,
code or completion receipt is admitted. Root/CAP_SYS_ADMIN, memory gate and
exact DRM file/context are required. The endpoint reconstructs the command
from actual GEM backing. The retained command union keeps the old copy size;
fill's unused union tail must be zero. One destination binding is authenticated
for live generation, pin/map/lease/holds, PTEs, permissions, byte range and PFN
uniqueness. Initialized storage is required, preserving untouched pixels.

The same retained heap session excludes simultaneous copy/fill and retains
uncertain backing after FD death or gate toggles. Completion requires the
owned2D event, empty FIFO, idle engine and clean fault state. Coherency,
publication, request-base/ALPHA restoration and retirement remain mandatory.
Only a known unissued prepared job can be cancelled; reset is never retirement.
No hashes, framebuffer comparisons, readbacks or timing diagnostics are added
to the endpoint. The external probe supplies these qualification operations.

This endpoint is not yet connected to Gallium clear callbacks. It does not
advertise general GL/GLES support or unprivileged MMU isolation.

## Offline qualification

- `ENCODER_RESULT.json`: native/UBSan/i386,20 dimension/color pairs, exact
  seven-word commands, source independence, guards, bounds, address/format
  errors and unchanged output on refusal.
- `FOCUSED_RESULT.json`: native/UBSan/i386 real GEM/preflight/lifecycle/copy/fill
  endpoint and native-MMIO models. Fill checks367 parent assertions plus seven
  joined retained/cancellation fault models; copy still passes239 parent checks.
- `NATIVE_RESULT.json`: native/UBSan/i386 MMIO tests including32 fill fault
  children, ALPHA publication/interference/restore checks and actual antiX
  object compilation/partial link with all new symbols resolved.
- `legacy-regression/RESULT.json` and `legacy-lab-regression/RESULT.json`:
  original normal-copy and LAB profiles pass with the fill macro absent.
- `PROFILE_GATES.json`: invalid profile combinations refused.
- `FULL_MODULE_BUILD.json`, `INITRD_BUILD.json`: complete experimental module
  and private boot image, original boot-script logic preserved.

The first model run stopped because its ALPHA-write assertion admitted only
nine-word packets. It now admits the independently tested seven-word fill
profile as well; register ownership/value/restore checks remain. The original
failure is preserved as `native-first-failure.log` and `.stderr`.

## Physical qualification

The external `twod_fill_probe.c` completed16 native fill ioctls at each of
1280×800,640×640 and641×479 on the physical SGX535 rev121:48 total. It allocated
and reused destination and unrelated sentinel BOs, submitted destination-only
fill requests, and checked both full final BO images. The sentinel is not a
GPU source. Source/destination/copy names in the reused harness are inherited
labels; the captured ABI is the destination-only fill ioctl8.

Boot: `700e0655-9bba-48e9-8e97-17f057b72dc6`.
Module SHA256:
`a1c54591a865a1b8315895c508e53f5f7e3afc77b1987a894a10aed1146fc9ec`.
Image SHA256:
`0687c2f38c9897160e2c02bf56e7ce7df5cf0dd7937f19e861098baab4e9b1f6`.
Client SHA256:
`0c96e9deedc9d86b21a198b4e74ea62ded06de940a6f4a8ad6d7788ac5346602`.

All cases returned exact core01130000/rev00010201, final operation16/cookie1
scoped to their session/reservation, stages31, error0/retained0/uncertain0 and
clean close. Actual hardware status drives completion and retirement. Full
readbacks checked14,041,088bytes across both BOs at the three sizes. Packed
color0x7f123456 read back as bytes56,34,12,7f in the exact requested rectangle;
all guard/padding/tail bytes and the unrelated sentinel remained unchanged.
An independent verifier replayed the expected images and checked hashes of
the final evidence outside the rendering sequence. No CPU fill substitutes
for the ioctl. No pixels of intermediate operations were read back.

| Fill rectangle | Mean ioctl wall | Min | Max |
|---|---:|---:|---:|
| 1280×800 | 25.923199 ms | 25.626234 ms | 26.174068 ms |
| 640×640 | 9.716001 ms | 9.633418 ms | 9.856212 ms |
| 641×479 | 7.338516 ms | 7.079252 ms | 8.025741 ms |

These external probe timings include submission, mapping checks, cache
maintenance, synchronization and retirement, not isolated GPU time. They
exclude final readback and presentation and do not establish sustained FPS.
DVFS/load are uncontrolled and this is a short sample. Fill and copy perform
different work; no speedup percentage is claimed between them.

`hardware/PHYSICAL_RESULTS.json`, per-size `*-INDEPENDENT_VERIFY.json`,
`*-FETCHED.json` and `artifacts-*/` preserve the records. All five controls
returned to N with no active producer or new captured failure log. Original
boot/default/empty next entry were restored before submission. A single stock
return was requested after all48 fills were retired, readbacks fetched/verified
and contexts closed. The final recovery receipt is
`hardware/FINAL_REMOTE_STATE.json`: stock boot
`46fb37c3-cbca-441c-aefa-dcefbd81891a`, exact original module, no experimental
controls/proc endpoint, original GRUB/default and empty next entry. Reset is
not retirement. No GPU job or reboot remains pending.

Native fill is qualified only within this privileged primitive and the tested
rectangle/color cases. It does not approve every color/operation or LAB tests.
Next connect it to the Linux userspace owner and Gallium clear callbacks with
writer synchronization and terminal quarantine. General3D, complete2D dispatch,
unprivileged admission, presentation and installation remain unfinished.
