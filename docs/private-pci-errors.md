# Private PCI helper error propagation

The private candidate now propagates PCI save, configuration-read, D3hot and
configuration-write errors from its PM helpers. This changes only the private
generated module; it does not alter the qualified native checkpoint or the five
non-private profiles, and it adds no rendering-loop diagnostics.

The original suspend helper returned void. It ignored pci_save_state, all four
configuration reads and pci_set_power_state errors, then marked the device
suspended. The original resume helper also ignored configuration writes. The
new helper commits its saved BSM/VBT/MSI values only after all reads succeed.
Failures before disable stop without disabling the device. After disable, the
suspended flag records that PCI restore/enable is required even if D3hot fails;
otherwise a later resume could skip enabling the disabled device. The outer
power-suspend callback returns the helper error and leaves admission closed.

Resume checks each explicit BSM/VBT/MSI configuration write, translates PCI BIOS
status with the target kernel's pcibios_err_to_errno, and returns before later
writes or device enable on failure. The existing D0 and enable failure checks
remain. No rollback or successful GPU recovery is inferred from these changes.

## Tests and limitations

Evidence root:
`/home/gama/sgx535-offline/private-pci-errors-20261029/v1/`.

`counterexample/RESULT.json` extracts the actual retained native PCI helpers and
demonstrates ignored save/read/D3/write/D0 failures in optimized native and UBSan
fixtures. `pci-helpers/RESULT.json` exercises the corrected helpers: save failure,
each of four read/write failure positions, D0/D3 failure, enable failure,
successful flow, already-suspended behavior and restore/enable after failed D3.
`i486-pci-helpers/RESULT.json` repeats controls as static i486 under QEMU.
`admission/RESULT.json` and `i486-admission/RESULT.json` retain the scheduled
admission/PM-owner/error regressions and check outer propagation. PCI APIs and
power/MMIO dependencies are modeled; these are not kernel PM or hardware tests.

The target header defines pci_save_state as int and pci_restore_state as void.
Explicit configuration statuses can be checked; this does not invent an error
return for the latter void API. The separate GMCH configuration-word operation
and the GTT initializer's internal error/teardown contract remain to be handled.
In particular, retained psb_gtt_restore ignores psb_gtt_init failure and continues
insertion, even though the initializer's error path calls psb_gtt_takedown and
clears gtt_map. That is the next bounded failure experiment, not a path approved
by these PCI helper tests.

Physical S3, source quiescence, GPU stop/retirement, partial-failure recovery and
surprise physical loss remain unqualified. No installation, suspend, reboot,
physical GPU operation or push is part of this change. Existing isolation,
quarantine and boot recovery requirements remain in effect.

## Build result

The complete i386 Kbuild/modpost/link passed in 156.956 seconds with one job
and a 768 MiB per-process address-space ceiling. Existing build warnings remain
in the preserved build log. Read-only ELF guard checks passed; all six controls
remain disabled in the image. Original build inputs/symbols are unchanged.
SOURCE_IDENTITY.json binds 214 staged/build inputs and all six fixture results;
31 protected inputs are unchanged. Five normal profiles compare identical.

Uninstalled, physically unqualified module SHA-256:
`f4af4a9a5ffa796cf0baedec2ed6330c76ba5fe929039028dadd11bb08d0b09c`. GPU operations: zero.

Follow-up: `private-gtt-resume.md` records the subsequent checked GTT reuse
path and GMCH configuration-word error propagation. These offline corrections
do not qualify physical S3, source quiescence or complete failure recovery.
