# Native color clears through Gallium

The native solid-fill primitive is now connected to `clear_texture`,
`clear_render_target` and color framebuffer `clear`. Each uses the persistent
resource owner and the normal Linux fill transport. Physical qualification is
recorded below once complete. Complete3D/2D and an installable driver remain
unfinished; these callbacks do not grant general GL/GLES admission.

## Connection and safety

Enable the backend before resources and the screen callback before children:

```c
remesa_persistent_set_native_fill(&device, remesa_linux_memory_native_fill);
screen = remesa_screen_create(&device);
remesa_screen_enable_native_fill(screen, remesa_persistent_fill_region);
```

Check each return value. Context/resource access is externally serialized.
The opt-in leaves public device/config layouts and drawing capabilities intact.
Large bound BGRA color storage is admitted solely for native fill with a
resource-only backend, under the same resource limits; its draw gate remains
closed. Default CPU storage operations remain explicit when native is absent.

Raw texel clear preserves packed bytes. Surface/framebuffer color clears reuse
the existing color packer and rectangle/scissor validation. The destination's
previous writer is waited before a new content epoch is reserved. The owner
validates actual ordinary-storage membership, pin/map/lease/registry state,
logical byte span, dimensions/pitch and the qualified address window, holding
the BO until dispatch returns. Success requires authenticated synchronous
GPU completion, visibility and retirement before publishing the epoch.
Known unissued failures cancel the reservation; uncertainty quarantines the
resource and retains the backend reference. No native error falls back to CPU.

The normal fill transport validates its owned destination, issues one104-byte
ioctl and checks immutable request fields, exact SGX535/rev121 response and a
new monotonic operation. The same operation tracker covers copy and fill.
Malformed or uncertain responses poison destination backing. No INFO, buffer
map/readback/hash, allocation or clock is added to this rendering path.
Necessary bounds/permissions/coherency/fences/retirement/error checks remain.
The unchanged kernel retains root/CAP_SYS_ADMIN, default-off gates and exact
DRM file/context isolation. No unprivileged admission is claimed.

Depth/stencil and unsupported fill cases fail explicitly. A combined native
color/depth clear is refused before either attachment is written. Only color
fill is qualified by the existing primitive. The original gears execution and
its independent depth path remain unchanged when this opt-in is absent.

## Offline evidence

Root: `/home/gama/sgx535-offline/gallium-native-fill-20261028/v1/`.

`owner-v2/result.json` passes native/UBSan/i386 owner and all three Gallium clear
callbacks at the three required sizes, correct packed raw/red colors, no CPU
maps/fallback, busy lease refusal, unsupported dimensions, known-unissued
cancellation/retry and retained uncertain ownership. Its injected GPU produces
no pixels. `writers/result.json` checks cross-context writer waits/timeouts,
no fallback and malformed callback terminal failure. These are CPU tests.

`TRANSPORT_RESULT.json` tests interposed protocol errors in three architectures:
exact receipt, lease/ID/alignment rejection, one ioctl/no retry, malformed and
uncertain poisoning, known-unissued cancellation and recorder exclusion.
`private-mesa-v2/RESULT.json` builds the eight changed objects with actual pinned
Mesa25.2 Meson arguments in a private archive. Original cache objects/archive
remain unchanged. Nine `regressions/RESULT.json` tests pass against that archive:
fill writers/transport, Gallium copy, copy writers/transport, legacy CPU
copy/clear, original memory transport, scene captures and gears geometry.

`PROBE_BUILD.json` binds the external i386 qualification client and safe
non-device tests. `staged-overlay/integration.json` includes the fill UAPI in
the103-file Mesa source overlay. `PREPHYSICAL_GATE.json` verifies candidate
sources, binaries, immutable image and31 protected historical artifacts.

Initial harness failures remain preserved: an outdated framebuffer initializer,
a non-existent old-cache twod source expectation and a missing log parent
before test invocation. None required weakening a driver gate.

## Physical qualification

The actual i386 client completed144 native fills:16 calls of each of the three
APIs at each of1280×800,640×640 and641×479. Each size reused two owned GEM
resources/mappings across all48 calls. One was an unchanged sentinel; it was
not a fill source. Final readbacks were taken after each16-call API sequence,
so all three distinct colors were checked physically at all three sizes.

Boot: `25a4cddd-493a-491e-bba0-842a84100d33`.
Client SHA256:
`f0b72a5ba470badf0e7c63148a11ffe2f68d0ad464981fe70dc4c5f9eb7690b4`.
The unchanged qualified fill module/image are bound in the
[native fill report](TWOD-NATIVE-FILL-20261028.md).

Every call passed SGX535core01130000/rev00010201, completion/visibility/
retirement checks. Each session finished at operation48/cookie1 (scoped to
its context/reservation), stages31, error0/retained0/uncertain0/cleanup0.
Each size observed48 actual fill ioctls, zero CPU clears and zero memory
ioctls, leases or new allocations inside the native callback sequences.
Readback/diagnostic counters and clocks live only in the external probe.

The42,113,880bytes captured across the nine sequence results and two buffers
were replayed by the independent verifier: exact raw0x7f123456, red0xffff0000
and green0xff00ff00 rectangles, unchanged sentinel and intact logical guard
columns/rows. This does not inspect GEM page padding or every intermediate
image. Inherited `copy_*` fields/filenames in the harness describe fill calls,
not copy rendering or source consumption.

| Resolution | Raw texel clear | Surface clear | Scissored framebuffer clear |
|---|---:|---:|---:|
| 1280x800 | 27.726732 ms | 27.191547 ms | 29.414202 ms |
| 640x640 | 10.556287 ms | 10.725443 ms | 9.862846 ms |
| 641x479 | 8.289868 ms | 7.241258 ms | 7.235797 ms |

These are external callback wall times including owner/transport/cache/sync/
retirement, excluding qualification readback and presentation. They are not
isolated GPU time or sustained desktop FPS. Short samples and uncontrolled
DVFS/load do not support a speedup percentage against earlier primitive runs.

Evidence: `hardware/PHYSICAL_RESULTS.json`, per-size independent verification,
fetch manifests and raw `artifacts-*/` files. All five controls returned to N,
producers0, clean context closes. Original default/empty next boot restored
before GPU work. A single return to stock was requested only after all144
fills retired and all nine readbacks were independently verified. Final stock
state is bound in `hardware/FINAL_REMOTE_STATE.json`: stock boot
`baf997a8-a60c-4a2c-b516-c9c019ec2cd3`, exact original module, no observer or
experimental controls/proc endpoint, original GRUB/default and empty next entry.
Reset is not retirement. No GPU job or reboot remains pending.

### User-visible white flash remains unresolved

The user reported one white flash during the attempt. It is preserved in
`USER_DISPLAY_OBSERVATIONS.jsonl` and `DISPLAY_FLICKER_REVIEW.json`. Correct
private buffer readbacks and retirement do not prove display coexistence.
The captured1280 kernel-log delta has only an unrelated missing-TOMOYO-init
message; the640 and641 deltas are empty. Absence of errors is not a cause.

Source review shows explicit native writes to the SGX FIFO, event ACK,2D
request base and ALPHA, with owned restoration required. No display-plane
register is directly written by those adapter functions. The memory service
excludes display GTT overlap and uses `gma_power_begin(dev,false)` to hold
already-on power; close uses `pm_runtime_put`. This does not rule out indirect
shared-state or power effects. No display/CRTC state was captured during the
flash, so neither Xorg overwrite nor a power transition is established.

Before desktop coexistence qualification, the next minimum is a separate LAB
observer timestamping session open/close and API sequences while reading
connector/CRTC/plane/DPMS and runtime power state before/during/after. Do not
change shared registers or remove exclusion to test the hypothesis. This
component's pass covers native color-clear pixels/lifetime, not flicker-free
presentation or stable everyday desktop operation.

## Remaining work

Other2D operations, general driver construction, unprivileged MMU admission,
presentation and packaging remain unfinished. General3D still needs complete
event PDS, attachment/DPM initialization, raster control and typed native
submission followed by physical desktop GL/GLES qualification. Video remains
later work. This component does not establish desktop FPS or API conformance.
