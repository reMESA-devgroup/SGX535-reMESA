# Serialize native 2D against the console FIFO producer

`remesa_twod_reservation.h/.inc` supplies a device-local kernel reservation
under the existing `dev_priv->lock_2d`. The pinned staging tool also patches
the original `psbfb_2d_submit` to check that reservation after taking the same
lock and before its first wait or command write. The include alone is not an
integrated producer guard.

Reserve requires a powered, idle engine with an empty FIFO. It records the
current BIF_TWOD_REQ_BASE and retains an exclusive caller-owned token; it does
not change a register. Release refuses a foreign token/device, lost power,
busy engine or a request base not yet restored. Failures preserve ownership.
The native adapter must separately prove completion, visibility and retirement
before release. Idle and a matching base alone cannot authorize retirement.
No spinlock is retained across the split-phase job calls.

The device owner pointer is appended to `drm_psb_private`, preserving prior
field offsets. A candidate module must rebuild its native translation units;
this is not a binary patch for an already loaded module.

The old console caller previously ignored submission errors. A guarded EBUSY
is known to occur before any command write, so the caller now uses its existing
CPU console-copy helper for that case. An EIO/uncertain partial submission is
never replayed as a CPU copy. This fallback serves only the legacy framebuffer
console; it does not implement a native reMESA GPU copy or establish hardware
acceleration. The native 2D job path has no CPU rendering fallback.

The original encoder, direction helper and wait body remain unchanged. When
there is no reservation, legacy behavior is preserved. During a reservation
the legacy wait cannot run, so its timeout cannot reach `psb_spank` and reset
the active engine. The other retained `psb_spank` call is in device startup,
before runtime registration; native callers must retain device/module/power
lifetime. This reservation does not independently prove global producer or
reset exclusion for an arbitrary kernel version.

## Reproducible staging and verification

```sh
python3 tools/remesa/stage_twod_reservation.py \
  --native-source /home/gama/sgx535-offline/gears-depth-less-single-clear-20261026T160400Z/module \
  --output /tmp/remesa-twod-reservation
```

The output directory must not exist. The tool requires the exact reviewed
accel_2d.c and psb_drv.h hashes, checks unique integration anchors, copies native
sources into the new directory and records output hashes. It neither edits the
input nor installs/builds a module, registers an ioctl or issues GPU work.
Refusal to overwrite and refusal of a mutated source before output creation
are tested.

Evidence: `/home/gama/sgx535-offline/twod-reservation-20261027/`.
`RUN.py` extracts the actual staged legacy wait, submit, direction, copy and
console caller functions without changing their bodies. The explicit CPU
register/locking model executes those functions and the actual reservation
include. Native, UBSan and i386 each pass 424 checks, including 1280x800,
640x640 and 641x479 console requests; original packets before/after a reservation;
no FIFO reads/writes/reset during reservation; powered/idle/base/token failures;
independent devices; successful release/reuse; and no CPU replay on legacy EIO.
Only the original signed loop-counter warning is locally suppressed around the
verbatim extracted functions; new code is tested with `-Wall -Wextra -Werror`.
The first fixture compilation lacked the slave-port macro from psb_drv.h;
the runner now extracts that original definition rather than hardcoding it.

Private complete accel_2d.c and psb_drv.c translation units compile using the
retained actual antiX i686 commands, including the new struct size and native
Linux spinlock APIs. Original input hashes remain unchanged. Model counters
and successful object builds are not evidence of physical FIFO behavior.

No executor, IRQ capture hook or GPU submission is installed by this increment.
Next: shared pre-ACK IRQ ownership, stale-event boundary, actual core/revision
and MMU-root authentication, FIFO emission, native coherency and request-base
restoration. Then paired physical readback/guards/retirement before Gallium
dispatch. Keep the qualified 3D completion mask and working GLXGears unchanged.
