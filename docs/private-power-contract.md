# Power-transition counterexamples and the S3 qualification boundary

The Mini 12 read-only observation at
`/home/gama/sgx535-offline/mini12-readonly-power-20261029/v1/RESULT.json`
reports kernel `5.10.240-antix.1-486-smp`, PCI `8086:8108` revision `06`,
`freeze mem disk` and `s2idle [deep]`. No suspend request, register access,
GPU submission, module installation or boot change was made. This advertises
deep suspend; it does not prove SGX revision, successful resume or GPU reset.
On ACPI platforms, deep suspend-to-RAM corresponds to S3, as described in the
[kernel sleep-state documentation](https://docs.kernel.org/admin-guide/pm/sleep-states.html).

CPU tests compile the actual retained power entry/helper bodies and schedule
these cases with pthreads and modeled PCI/MMIO/backends:

1. Suspend sees a zero display count. Before hardware shutdown, another
   non-forcing power claim succeeds. Power-down still occurs with that claim live.
2. Resume sets `display_power` before restoring GTT/registers. Another claim
   succeeds during the incomplete restoration.
3. Forced begin interprets boolean PCI-resume success as zero. Successful PCI
   resume is rejected; failed PCI enable is accepted, followed by IRQ setup/MMIO
   and a published power reference. PCI enable also runs under the claim spinlock.
4. Public resume ignores PCI-resume failure and still restores display/MMIO,
   publishes active state and returns success.

The control with a claim acquired before the suspend count check correctly
returns EBUSY. These results are **counterexamples, not a driver safety pass**.
They have not been reproduced as physical crashes or physical S3 transitions.
The target's normal runtime-PM enable call is inside `#if 0`; this limits claims
about normal runtime behavior but does not qualify the system-sleep callbacks.

## Incremental correction already applied

The private lid worker now queues on `system_freezable_wq`. The target's default
system workqueue is not freezable; its dedicated freezable queue has the
WQ_FREEZABLE flag. That selection lets system suspend freeze this worker before
device suspension. Its existing stop/join, DRM-entry and power-reference behavior
remains. No rendering-loop cost or normal-profile code changes are introduced.

This is one worker's system-sleep integration. The general PM admission/error
counterexamples remain open, including other raw-MMIO owners and the forced
resume/lock-order contract. Runtime-PM and physical S3 remain unqualified.
An existing virtual MMIO mapping does not authorize access with device memory
space disabled: [PCI PM documentation](https://docs.kernel.org/power/pci.html).
S3 is not accepted as an independently proven SGX reset/retirement mechanism.

## Offline evidence

Root: `/home/gama/sgx535-offline/private-power-contract-20261029/`.

- `v1/counterexample/RESULT.json`: actual power bodies, native/UBSan, scheduled
  suspend/resume races and PCI-failure controls. `driver_safety_pass=false`.
- `v1/i486-counterexample/RESULT.json`: static i486/QEMU counterexamples confirmed.
- `v2/lid-test/RESULT.json`, `v2/i486-lid-test/RESULT.json`: actual lid work with
  freezable queue selection, existing transition/denial/cancellation regressions,
  native/UBSan and i486/QEMU. Workqueue/PM/MMIO are modeled, not kernel execution.
- `v2/full-module/RESULT.json`: complete i386 Kbuild/modpost/link in 101.316 seconds,
  one job and 768 MiB address-space ceiling per process; original kernel/cache
  keys and observer symbols unchanged. Uninstalled/unqualified.
- `v2/full-module/MODULE_VERIFICATION.json`: previous read-only ELF guard checks.
- `v2/FREEZABLE_ELF.json`: new module imports system_freezable_wq; previous module
  does not. Both binaries unchanged. Import presence alone does not prove freezing.
- `v2/SOURCE_IDENTITY.json`: only changed staged runtime input is the private
  lid fragment; protected files unchanged. Normal-profile components exclude it.

Unqualified module SHA-256:
`33ea3f620c4fdb237dad3083d1b6680808d2203a5b70b7156f12f1b2def4e082`.

Next correction must atomically close admission against the suspend decision,
publish availability only after restoration, and propagate PCI failures. It must
also preserve legitimate PM-internal callbacks and respect IRQ/process context,
KMS/power lock order, raw-MMIO owners and balanced references. Enabling runtime
PM or replacing the retained force path wholesale without that review is not a
qualification shortcut. Physical testing stays blocked until these contracts
and the retained GPU stop/collection policy are ready.
