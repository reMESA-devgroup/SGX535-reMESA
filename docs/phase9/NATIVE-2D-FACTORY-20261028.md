# Owned native 2D backend construction

`remesa_loader_native_2d_screen()` explicitly constructs a resource-only
Gallium backend with both native copy and color fill. It owns a CLOEXEC DRM
FD duplicate and a per-file GEM session until the last screen child is
released. This is an internal privileged construction path, separate from
Mesa's general DRI/OpenGL constructor. Complete3D/2D and an installable driver
remain unfinished.

## Lifetime contract

The factory checks read/write access, duplicates the caller's FD, rechecks
that duplicate's access mode and probes PCI identity through that descriptor.
It opens the typed memory session and configures the existing qualified
32MiB private allocation profile, resource-only persistent owner, native copy
and fill callbacks. It does not enable kernel gates, replace default drivers
or grant GL/GLES drawing capability. The caller may close its original FD;
the duplicate belongs to the backend. An ordinary `screen->destroy()` starts
shutdown, while contexts, resources and fences retain the backend as before.

`remesa_persistent_take_provider()` transfers provider lifetime before any
resources exist, without changing public device/config layouts. Persistent
close first requires all jobs, scene captures, buffers and shaders to be gone.
Only then does it release the provider, close the GEM session and close the
owned FD once. Failed or malformed provider release is cached, retaining the
owner and preventing repetition of an uncertain CLOSE. Failed owners refuse
new allocations. Without this opt-in, borrowed-provider teardown is unchanged.

Known constructor failures clean up. An ambiguous OPEN retains its duplicate
and returns an opaque failure owner, even when the underlying memory OPEN
could not return a valid session object. Submission uncertainty retains its
buffers/provider through the existing quarantine path. No forced cleanup or
retry is offered for these ambiguous outcomes. Recovery/reset remains separate
and cannot substitute for GPU retirement. Linux `close()` is not retried after
an error because the numeric descriptor may already have been released.

The implementation lives in a new `remesa_native_2d_loader.c` unit. Protected
`remesa_loader.c` remains byte-identical. A stack device copy during constructor
rollback avoids touching a containing owner after the release callback frees
it. Screen shutdown remains the owner of successful factory resources.

## Offline evidence

Root: `/home/gama/sgx535-offline/native-2d-factory-20261028/v1/`.

- `factory-v3/result.json`: native/UBSan/i386 actual constructor/owner/Gallium,
  CLOEXEC duplicate, independent caller FD, combined monotonic copy/fill,
  deferred destruction, closed-screen rejection, constructor rollback and
  ambiguous OPEN/CLOSE/submission retention. Syscalls are interposed; GPU0.
- `lifetime-v2/result.json`: provider transfer before resources, deferred close,
  exactly-once release, cached failure/malformed callback and unchanged borrowed
  teardown across three architectures. These CPU models do not prove rendering.
- `private-mesa-v4/RESULT.json` and `NEW_UNIT_BINDING.json`: eight existing Mesa
  objects rebuilt plus one new loader unit with the pinned Mesa25.2 compile
  model, actual archive member and source hashes checked. Original cached
  archive/objects and protected loader remain unchanged. The generic copied
  build receipt's historical eight-object wording is clarified by the explicit
  new-unit inventory.
- `regressions-v2/RESULT.json`:12 actual-archive regressions, including old
  diagnostic loader ownership, provider lifetime, color fills, copy, writer/
  transport errors, legacy CPU storage, memory transport, scenes and gears.
- `PROBE_FINAL_BUILD.json`, `PREPHYSICAL_GATE.json` and
  `staged-overlay-v2/integration.json`: external i386 client, current bindings,
  all31 protected files and104-file source overlay registering the new unit.

The first fixture omitted its mock GPU-map/sync opt-in and correctly refused
allocation. The protected-file gate caught an initial append to the old loader
before any test boot; that source was restored and the new unit introduced.
The moved unit initially lacked its screen declarations, caught at compilation.
All failed artifacts remain preserved. No runtime safety gate was relaxed.

## Physical qualification

The actual i386 client used this constructor on the Mini12 SGX535 rev121 at
1280×800,640×640 and641×479. Each session closed the original caller FD after
construction, then executed one native copy and three16-fill API sequences
(raw texture, surface red and scissored framebuffer green). Readbacks after
copy and each fill sequence checked56,151,840logical bytes, exact requested
pixels, untouched sentinel and guards. This does not inspect GEM page padding
or every intermediate frame.

Boot: `6d744725-2c53-48df-a23e-c72ac419f93d`.
Client SHA256:
`3464d4c362c4b88c0c717b9479980a3d44d0fc5870c2e20beda717cfbb7a1b73`.
The unchanged fill module/image are bound in
[TWOD-NATIVE-FILL-20261028.md](TWOD-NATIVE-FILL-20261028.md).

All147 GPU operations (3copies+144fills) passed SGX535core01130000/rev00010201,
authenticated completion/visibility/retirement, final operation49 per session,
stages31 and no retained/uncertain/error outcome. Each session used one
monotonic tracker across copy and fill. There were no CPU clears/copies or
memory ioctls, leases or allocations inside those native sequences.
The original FD was closed before use, screen destruction began before child
cleanup, the owned FD remained alive while children existed, and final resource
release performed exactly one MEM_CLOSE and closed the duplicate. All three
contexts closed successfully after their actual GPU retirements.

| Rectangle | One copy wall time | Raw clear mean | Surface clear mean | Framebuffer clear mean |
|---|---:|---:|---:|---:|
| 1280x800 | 34.751347 ms | 28.123985 ms | 28.418763 ms | 27.859003 ms |
| 640x640 | 13.517920 ms | 11.333376 ms | 10.009404 ms | 10.356340 ms |
| 641x479 | 9.591443 ms | 7.395109 ms | 8.381216 ms | 7.369757 ms |

These short external wall timings include checks/cache/sync/retirement and
exclude readback/presentation. The separate display observer also ran; these
are not isolated GPU time, controlled optimization comparisons or desktop FPS.

Evidence is in `hardware-v2/PHYSICAL_RESULTS.json`, per-size fetch/independent
verification and raw artifacts. The first `hardware/` attempt kept the copy
gate disabled because its controller came from the earlier fill-only test.
Its first copy was refused EOPNOTSUPP before prepare/start, zero GPU words,
virgin source state, followed by successful session/FD close. The probe's
conservative pre-dispatch uncertain flag did not represent an uncertain DMA
outcome in this case; `KNOWN_UNISSUED_GATE_REFUSAL.json` binds the gate/source/
cleanup evidence. That failed record was preserved. Corrected attempts used
new paths on the same proven-virgin boot and enabled both required gates;
no admission gate was removed, no ambiguous GPU attempt was retried.
The independent verifier's first summary incorrectly printed3images while
its checks already verified4; `1280x800-FIRST_REPORT_COUNT.json` is retained
and the corrected report counts all4images. No pixels or gates changed.

`DISPLAY_POWER_REVIEW.json` records41 external cached-sysfs samples at200ms
around session/API/close timestamps. Sampled connector/DPMS/framebuffer/runtime
power state did not change. This does not capture scanout content or a short
flash and does not explain the earlier user-reported white flash. No correlated
flash observation was recorded for this attempt. Desktop coexistence remains
unqualified; no Xorg configuration/module was replaced during these tests.

All five controls returned to N with no active producer. Original GRUB/default
and empty next entry were restored before submission. A single stock return
occurred only after all147 retirements, independently verified readbacks and
context/FD closes. `hardware-v2/FINAL_REMOTE_STATE.json` verifies stock boot
`5acedcdc-692f-4bb9-b3cd-63cebd71dbd3`, original module, observer/control/proc
endpoints absent and no pending next boot. Reset was not used for retirement.

## Remaining work

Full general3D shader/texture/state/native draw contracts, the remaining2D
operations, unprivileged MMU admission, presentation and installation remain
unfinished. The DRI/OpenGL gate stays closed. Minecraft remains an application
target through the general driver, not an application-specific substitute.
