# Serialize private source authority with fixed-action MMIO

The private fixed backend formerly called `remesa_route_source_write()`, released
the source lock, and then issued the register write. A concurrent source poison,
IRQ, lifecycle or producer invalidation could take effect in that gap. Final
retirement could still reject the operation, but the check did not exclude a
later CPU MMIO write.

The new private path calls `remesa_private_source_write()`. It validates the
existing source authority, the exact bound backend/task, private-loan presence,
active/power/register state, timeout, and aligned offset within the SGX register
mapping. It issues one trusted fixed-action register write before releasing
`source_lock`, which all source invalidators also acquire. It never invokes a
sleeping callback or acquires the IRQ lock while holding the source lock.

The established IRQ path takes fixed IRQ lock then source lock. This new path
takes only source lock, so it adds no source-to-IRQ lock inversion. Existing
fixed actions are called outside the IRQ critical section: TA_FIRE and TA_LOAD
set their flags and release that lock before invoking the actions. There is no
new generic ioctl or userspace-selected register-write interface; fixed action
plans remain authenticated by their existing callers.

This orders **CPU write issue** against software source invalidation. It does not
prove completion of a posted bus write, establish device retirement, or replace
the unresolved PCI removal/resource-lifetime protocol. It adds no register
readback, diagnostic hash, checkpoint or polling wait per write. Default and
legacy fixed-write dispatch remain unchanged in non-private stages.

Verification under `/home/gama/sgx535-offline/private-source-mmio-20261029/v1/`:

- `linearization/RESULT.json`: native/UBSan with exact extracted authority,
  poison and old fixed-write functions, actual source-valid implementation and
  the new source/MMIO fragment. A two-thread schedule reproduces the old gap;
  the new path orders recorded CPU MMIO before invalidation. Pre-poisoned source,
  wrong task, power/mapping failure, elapsed deadline and invalid offsets produce
  no recorded write. Owner structs and MMIO are modeled; pthread locking is real.
- `linearization-i486/RESULT.json`: the same reduced pthread test statically
  compiles for i486 and passes under QEMU. No SGX hardware execution.
- `full-module/RESULT.json`: complete i386 Kbuild/modpost/link passes in
  93.397 seconds, one compiler at a time, 768 MiB process address-space ceiling.
  Kernel/cache inputs remain unchanged. The module is not installed.
- `full-module/MODULE_VERIFICATION.json`: read-only ELF check requires the new
  source/MMIO symbol and confirms all six experimental controls are zero BSS.
  Symbol presence is not proof of runtime behavior or live N/N settings.

Module SHA-256:
`6e3941b649d07ca42d8df39e782ea9761eb38b93453438e45234dd940b8bbe12`.

The reproducible CPU regression is
`tools/remesa/test_private_source_mmio.py --baseline-module OLD_STAGED_MODULE --output NEW_DIRECTORY`.
Use a pre-correction module source for the baseline counterexample. No device is
opened. The preserved model tests do not authorize physical private-context use:
PCI detach and general shared-backend synchronization remain unfinished.
