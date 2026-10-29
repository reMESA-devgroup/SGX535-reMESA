# Observe lid transitions in process context and join before detach

The retained native lid timer starts even when OpRegion setup did not publish
a lid-state pointer. Its callback reads that pointer unconditionally. Its close
poll also waits while PP_ON is clear: it can spin indefinitely once the panel
is off, or finish prematurely while the panel is still on. Either spin can
prevent synchronous timer teardown. The callback may invoke the existing LVDS
I2C brightness path from timer context, where sleeping work does not belong.

The private profile replaces this one timer implementation with delayed work.
Existing `psb_lid_timer_init`/`takedown` interfaces remain, and native source and
five normal profiles remain unchanged. New private fields are appended to the
driver structure; previous field offsets are retained.

- Initialization requires both a published OpRegion header and lid-state pointer.
- The worker holds DRM entry and the existing non-forcing power reference during
  hardware access. Loss denies access; unavailable power defers observation.
- Each execution samples panel status once. A transition stays pending until
  PP_ON matches the requested lid state and the sequence bits are clear. There
  is no new guessed timeout, forced completion or busy wait. Stuck hardware
  remains pending between observations at the existing PSB_LID_DELAY interval.
- The worker issues the target command once per changed request. It darkens the
  closing panel before clearing its target and lights an open panel after the
  status confirms completion. A reversed lid request changes the pending target.
  Unchanged settled state does not rewrite registers or brightness.
- Queue admission and the stopping flag share the lid lock. Teardown sets that
  flag, drops the lock and cancels/joins work. It now runs before backlight
  unregister; the later chip-teardown call can repeat it. No lock spans the work
  join or brightness operation. No work is queued after the stop decision.

This removes the explicit timer register loops, not every possible wait in the
existing brightness/I2C/power implementation. Kernel workqueue, power-management
and I2C behavior still require target qualification. No rendering path, submission
format, shader, geometry, frame hash/readback or checkpoint behavior changes.
There is no new FPS or physical acceleration evidence.

## Offline evidence

Root: `/home/gama/sgx535-offline/private-lid-work-20261029/`.

- `v4/lid-test/RESULT.json`: actual native timer bodies and actual new work
  fragment, native/UBSan. Native NULL read and steady-register spin are safely
  intercepted with longjmp; premature close is recorded with PP_ON still set.
  New cases cover absent mapping, unchanged settled state, pending/completed
  open/close, reversal, repeated observations without spinning, power/loss denial,
  duplicate init, idempotent stop and pthread joining of a blocked brightness call.
- `v4/i486-test/RESULT.json`: final static i486/QEMU fixture passes. Intermediate
  v3 tests are retained before adding the settled-state/no-rewrite control.
- `v1/unload-test/RESULT.json`, `v4/asle-regression/RESULT.json`: actual unload/
  probe and previous ASLE/backlight controls, native/UBSan. The private lid stop
  is required before backlight/chip teardown; resource retention remains intact.
- `v3/NORMAL_PROFILES.json`, `v3/LID.diff`: five normal generated source maps
  unchanged against `8ff958a`. Private changes are psb_drv.c, psb_drv.h, psb_lid.c
  and the new remesa_private_lid.inc component.
- `v3/SOURCE_IDENTITY.json`: staged source identity and protected input checks.
- `v3/full-module/RESULT.json`: complete i386 Kbuild/modpost/link in 86.590 seconds,
  one build job, 768 MiB process address-space ceiling, source kernel/cache keys
  and observer symbols unchanged. Uninstalled and physically unqualified.
- `v3/full-module/MODULE_VERIFICATION.json`: read-only ELF/previous guards/six
  disabled BSS controls and new lid lifecycle symbols. `v3/VERIFIER_NEGATIVE.json`
  rejects the previous module when a process-context lid worker is required.
  ELF symbols do not prove work ordering, power safety or live N/N state.

Unqualified module SHA-256:
`fd7702f49c2fd7f5fbee816de2f4140b1ec8666ab4a81f6829ca057afad5e274`.

## Fixture history and qualification limits

`v1/lid-test` retains the original failed lock-ownership assertion, fixture and
diagnostic logs: optimized host behavior differed when logging was added.
`v2/lid-test` retains a second failed compound assertion after replacing the TLS
ownership flag with mutex-protected pthread owner tracking. Generated host
assembly/GIMPLE/preprocessed source and diagnostic variants are preserved there.
The final fixture keeps owner tracking and checks the same ownership, pointer
and delay predicates in separate assertions; none was removed. Native, UBSan and
independent static i486/QEMU tests pass; `v4/HOST_LOCK_REPEATS.json` records three
additional final host runs. The cause of the earlier optimization-sensitive host
fixture behavior has not been independently established. Do not attribute it to
kernel execution or claim a compiler defect from these observations.

DRM/power/workqueue/MMIO/I2C dependencies are modeled. Tests do not execute kernel
workqueues or actual panel hardware, and underlying backend wait bounds have not
been fully qualified. No Mini 12/SSH/install/boot/parameter change occurred.
Physical private-MMU qualification, complete owner/PM lifetime review, retained
GPU stop/collection and complete GL/GLES/desktop acceleration remain unfinished.
Reboot-only quarantine and the existing recovery/rollback requirements remain.
