# Private asynchronous MMIO power claims

The private-MMU candidate now holds a non-forcing power claim across the
complete existing IRQ handler and the platform brightness callback. This closes
two demonstrated gaps in the admission work in `private-power-admission.md`.
It does not qualify suspend/resume, physical removal, GPU stop or retirement.
The native checkpoint and the five non-private generated profiles are unchanged.

## Behavior and ownership

An admitted IRQ holds the claim until after the original read/capture/ACK
sequence. A power transition must reject that active claim with EBUSY. An IRQ
denied admission returns IRQ_NONE before reading or acknowledging registers;
it does not fabricate a completion. Source masks, source routing, fixed backend
and the original capture-before-ACK order are unchanged. The PM owner exception
does not apply in interrupt context, including when current is the PM task.

ASLE holds an outer DRM entry and power claim around the generic backlight
dispatch and publishes cblv success only after an admitted call. Denial reports
ASLE_BACKLIGHT_FAILED without changing cblv. The platform callback also takes a
power claim, returning EAGAIN when unavailable. It deliberately does not take a
nested DRM entry: a previously admitted ASLE operation must be able to finish
its callback after unplug starts, while unplug joins the outer entry. The
generic backlight helper returns void and cannot propagate a nested denial.

For planned detach, admitted sysfs callbacks rely on the target backlight
framework's ops_lock/unregister join; internal ASLE/lid producers have their
separate work joins. IRQ teardown relies on free_irq joining the handler before
mapping/chip destruction. These contracts assume the hardware remains accessible
until the planned teardown joins finish. They do not establish safety after
surprise physical device loss or prohibit every MMIO access after DRM unplug.

## Offline evidence

Evidence root:
`/home/gama/sgx535-offline/private-async-power-20261029/v5/`.

- `counterexample-final` and `i486-counterexample-final`: actual old handler and
  ASLE/generic/platform/LVDS dispatch bodies reproduce access after modeled PM
  closure, offline IRQ capture/ACK, and false offline brightness success.
- `guards-final`: native optimized and UBSan controls pass, including active
  claims blocking closure, offline denial, admitted ASLE spanning unplug, modeled
  sysfs unregister joining callbacks, and the interrupt PM-owner exclusion.
- `i486-guards-final`: static i486 controls pass under QEMU.
- `admission-regression`, `asle-regression-final`, `irq-regression`: existing
  admission, ASLE cancellation and actual fixed capture/begin/end handoff
  regressions pass. The handoff fixture passes native, UBSan and static i486,
  retaining the original fault/retirement gates.
- `TARGET_CONTRACTS.json`, `PRIVATE.diff`, `SOURCE_REVIEW.json`,
  `NORMAL_PROFILES.json`, `INPUT_REJECTION.json`, `SOURCE_IDENTITY.json`:
  target callback joins, bounded source changes, five unchanged normal profiles,
  changed-input rejection, 212 identical staged/build inputs and 31 unchanged
  protected files. Failed earlier fixtures/logs remain in the evidence tree.

The full i386 Kbuild/modpost/link passed in 93.340 seconds. Read-only final ELF
verification passed, including existing guard symbols and six disabled controls.
Module: `full-module/module/gma500_gfx.ko`, SHA-256
`50b2fee332c14aeb1cb6dd6e8d6e37337271cf828b13be324428b2b911471527`.
This module is uninstalled and physically unqualified. Tests extract actual
function bodies but model PM, DRM, backlight, IRQ and hardware dependencies;
passing them does not prove the kernel/hardware lifecycle. GPU operations: zero.

## Remaining blockers

The retained PM IRQ-uninstall path still preserves SGX/MSVDX/TOPAZ mask bits;
these changes do not establish source quiescence or GPU retirement. Internal
PCI-suspend and GTT-restore errors still require propagation and bounded failure
tests. Partial-failure recovery and a physically verified S3 cycle remain open.
Next experiments must preserve the existing memory quarantine, boot recovery
and N/N controls; no physical sleep or module installation was performed here.

Follow-up: `private-pci-errors.md` records the subsequent PCI helper error
propagation. GTT failure handling and hardware qualification remain separate.
