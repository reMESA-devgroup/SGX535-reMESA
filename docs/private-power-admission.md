# Close power admission across private PM transitions

The retained native power implementation checks its display reference count
without the claim spinlock. An ordinary caller can claim power after the zero
check and keep that claim across PCI power-down. Resume exposes display power
before GTT and display restoration finish. Its forced claim path also treats a
boolean PCI-resume success as zero, invokes PCI enable under a spinlock, and can
accept a failed resume. Public resume ignores the failed PCI enable and proceeds
to MMIO. The actual-body baseline witnesses remain preserved separately.

The private profile now replaces those power entry bodies. The native checkpoint
and five normal generated profiles remain unchanged. New private state is
appended to the device structure, preserving existing field offsets.

- Suspend closes claim admission under the same spinlock that tests the count.
  An existing claim returns EBUSY before lifecycle invalidation; a later claim
  fails throughout the transition. Zero references do not establish GPU stop.
- The PM task may nest claims during save/restore only while its hardware access
  is usable. IRQ context is explicitly excluded even when current identifies
  the interrupted PM task. This preserves the native connector callbacks that
  call power_begin from inside modeset save/restore.
- Resume publishes ordinary admission after PCI, power-up, GTT, register restore
  and IRQ setup. PCI D0/enable errors return before the following MMIO. Callback
  errors propagate and leave admission closed; recovery from partial callback
  failures has not been qualified. No rollback sequence is invented here.
- Active forced claims still succeed. Offline forced claims fail without a
  partial PCI wake or synchronous KMS restore. The retained psb_drv.c has runtime
  PM enable under #if 0. This profile does not provide runtime-PM forced wake.
  A full restore may acquire modeset locks already held by a force caller;
  the system PM callback must finish before ordinary claims become available.
- Claim bookkeeping uses get_noresume/put_noidle. It balances runtime usage
  without scheduling an unsupported runtime resume from inside the claim lock.

Only private power.c and appended psb_drv.h fields change in generated existing
sources, plus remesa_private_power.inc. The stager pins the retained power input.
No submission, shader, frame observer, checkpoint or installed module changes.

## Offline evidence and remaining blockers

Root: `/home/gama/sgx535-offline/private-power-admission-20261029/v3/`.

`admission-final/RESULT.json` and `i486-admission-final/RESULT.json` compile the actual private
fragment with modeled PCI/PM/MMIO/task identity dependencies. Native, UBSan and
static i486/QEMU runs cover scheduled competing claims during suspend and resume,
active/offline force behavior, PCI D0/enable failures, nested PM callbacks and
borrowed-current IRQ rejection, existing-claim EBUSY, and callback error closure.
These are dependency models, not execution of kernel PM or interrupt machinery.
The retained old-body counterexamples are under
`private-power-contract-20261029/v1/{counterexample,i486-counterexample}/`.
`v1` admission evidence preserves the initial save-error admission policy before
v2 changed it to fail closed after IRQ disable. The v2 target build rejected
power.o because the MMIO macro requires the local identifier dev_priv; v3 uses
that identifier. The failed build/log and prior fixture outputs remain preserved.

`NORMAL_PROFILES.json` compares five generated maps with baseline 07d4aeb;
`SOURCE_REVIEW.json` records private changed files and 31 unchanged protected
inputs. `INPUT_REJECTION.json` rejects altered power input; the initial diff
helper omission and corrected native fallback are recorded in
`SOURCE_REVIEW_INITIAL_FAILURE.txt`. The target module build and ELF review are recorded under `full-module`.
The complete i386 Kbuild/modpost/link passed in 99.888 seconds with one job
and a 768 MiB per-process address-space ceiling. Original kernel/cache inputs
and observer symbols remain unchanged. Read-only ELF and previous guard/six
disabled-control checks passed; symbol presence does not prove PM ordering.
`SOURCE_IDENTITY.json` compares all staged/final source inputs and protected
hashes. The module is uninstalled and physically unqualified.

Module SHA-256:
`b9ac7c4fb942eb498e682c9a50a49757ca7db93ab446a04eb7d277479a1f3e82`.

The direct PM call to psb_irq_uninstall retains SGX/MSVDX/TOPAZ mask bits when
irq_enabled is set and does not join a running IRQ handler. That handler contains
raw VDC/SGX MMIO outside power_begin and outside its display power check. Native
ASLE/backlight paths can also reach the LVDS I2C/PWM brightness implementation
without an outer power claim. This admission correction does not guard those
accesses. IRQ producer/retirement semantics and brightness ownership require
bounded source/callback tests before changing their masks or wrappers.

The retained void PCI-suspend helper still ignores PCI power-state errors;
psb_gtt_restore internally ignores psb_gtt_init failure and returns zero. Those
internal errors cannot be qualified merely by checking their outer return value.
GPU stop/retirement, complete PM/MMIO lifetime, partial-failure recovery, physical
S3, removal and full GL/GLES/desktop acceleration remain unfinished. Existing
reboot-only quarantine and recovery requirements remain. GPU operations: zero.
