# Join ASLE brightness work before backlight destruction

The private profile previously inherited this unload order from the retained
native driver: uninstall IRQ, unregister backlight, tear down chip, then finish
OpRegion. OpRegion finalization cancels its ASLE work only at that final step.
An already queued or running brightness worker can therefore use
`dev_priv->backlight_device` after unregistering the backlight. The worker reads
`bd->props.max_brightness` and calls `gma_backlight_set`; native backlight exit
does not clear that owner pointer or give this worker an independent reference.

The private stager now adds `cancel_work_sync(&opregion.asle_work)` immediately
after IRQ uninstall and before backlight exit. The only inspected caller of the
ASLE scheduling entry is the native IRQ handler. On this target,
`drm_irq_uninstall` calls `free_irq`, which joins executing IRQ handlers. There
is therefore no remaining IRQ producer between the join and backlight teardown.
Canceling the queued work, or waiting for running work, prevents its stale use.
No endpoint, MMU, power or backlight lock is held across the work join.

OpRegion memory stays mapped until original finalization after chip teardown.
Moving all of finalization earlier would break the lid timer's use of the
OpRegion lid-state mapping. The later native work cancellation remains harmless.
The early join is conditional on CONFIG_ACPI and a published OpRegion header:
the inspected setup initializes work before publishing that header. Missing or
failed setup skips it. The private stager pins the exact native OpRegion and
backlight source used for these assumptions; the IRQ source was already pinned.

This modifies private unload only. Five normal generated source maps remain
identical, and there is no submission/frame-loop instrumentation, readback,
hash, allocation or shader change. It neither establishes GPU stop/retirement
nor qualifies module removal, installation or recovery. Reboot-only quarantine
and the outstanding KMS/PM/legacy-owner review remain in effect.

## Offline evidence

Root: `/home/gama/sgx535-offline/private-asle-teardown-20261029/`.

- `v4/asle-test/RESULT.json`: actual previous/current unload and original
  ASLE set/worker bodies. Baseline permits a queued brightness callback after
  modeled backlight unregister; fixed unload cancels it. A second case uses
  pthreads to hold a running brightness callback and verifies that unload waits
  before unregistering its backlight. Native and UBSan builds/runs passed.
- `v4/i486-test/RESULT.json`: the same baseline/fixed fixtures statically compile
  for i486 and pass under userspace QEMU. This is not kernel IRQ/workqueue testing.
- `v2/unload-test/RESULT.json`: actual unload/probe regression, native/UBSan,
  retained endpoint/GEM and normal cleanup/rebind controls. The combined ASLE
  fixture also reruns those controls, including absent OpRegion/backlight.
  Final v4 adds controls with IRQ already uninstalled and with an initialized
  OpRegion but no backlight. Intermediate v3 results remain preserved.
- `v3/SOURCE_REVIEW.json`, `v3/UNLOAD.diff`: only staged runtime source change
  against `4fa4ae2` is the early join in `psb_drv.c`; 31 protected files unchanged.
- `v3/NORMAL_PROFILES.json`: five normal generated source maps unchanged.
- `v3/INPUT_REJECTION.json`: private staging rejects altered OpRegion/backlight
  source inputs instead of silently inheriting unreviewed lifetime assumptions.
- `v3/full-module/RESULT.json`: complete i386 Kbuild/modpost/link passed in
  102.308 seconds with one build job and 768 MiB per-process address-space limits;
  original kernel/cache keys and symbols unchanged. Uninstalled/unqualified.
- `v3/full-module/MODULE_VERIFICATION.json`: read-only ELF and earlier entry,
  GEM, fbdev and disabled-control symbol checks passed. Symbol presence does not
  prove the new work-cancellation order; the actual-source model tests cover it.
- `v3/SOURCE_IDENTITY.json`: all staged build inputs still match their manifest;
  protected files remain unchanged.

Unqualified module SHA-256:
`0c0fef5252046a83e9cd364f8fec2b85301488231df5f9c9534e81b49d91a3ac`.

The workqueue, backlight, DRM, IRQ and hardware dependencies are modeled.
The stale-object witness keeps its fixture storage allocated and records use
after logical unregister; it does not deliberately access actually freed memory.
Target-source inspection supports the producer/reference contract separately.
Zero GPU operations were performed; no Mini 12, boot or installed module changed.

Historical fixture failures are preserved: `v1/unload-test` contains a compiler
misleading-indentation rejection in a mock; `v2/asle-test` contains a missing
explicit return in the reused main after renaming it into a normal function.
Original fixture copies and logs remain in those directories. These were host
fixture build issues, not kernel or hardware failures.
