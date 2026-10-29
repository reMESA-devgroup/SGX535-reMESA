# Keep the OpRegion owner until notifier callbacks have joined

Native OpRegion finalization clears `system_opregion` before calling
`unregister_acpi_notifier`. The video callback checks that pointer, then reads
it again to find the ACPI mailbox. A callback already in the chain can pass its
first check before finalization clears the pointer. That source-level schedule
leaves its later lookup without an owner. The mailbox mapping is not freed until
later; the prematurely cleared global pointer is the distinct problem here.

On the inspected antiX 5.10 target, ACPI uses a blocking notifier chain. Dispatch
holds its read semaphore through the callback; unregister takes the write
semaphore and removes the notifier. Thus unregister joins in-flight readers
before returning. The private stager now clears `system_opregion` after that
join. Work cancellation, mapping cleanup and the earlier ASLE/backlight join
retain their ordering. No new lock, per-frame work or public ABI is introduced.
Only private staged `opregion.c` changes. Native source remains untouched and
subject to its existing input pin; five normal generated source maps are unchanged.

## Evidence and limits

Root: `/home/gama/sgx535-offline/private-acpi-notifier-20261029/v1/`.

- `notifier-test/RESULT.json`: actual previous/current callback, initialization
  and finalization bodies, native and UBSan. Pthreads hold a notifier reader
  while finalization enters unregister. The old ordering clears its owner;
  the new ordering keeps it until the reader exits. Normal notification,
  absent header/ACPI mailbox, repeat finalization and late-dispatch controls pass.
- `i486-test/RESULT.json`: static i486/QEMU versions of both fixtures pass.
- `TARGET_CONTRACTS.json`: source review and input hashes for the target ACPI
  wrappers and blocking notifier reader/writer contract. The core's boot-time
  single-thread special case is unchanged. This is not execution of kernel locks.
- `NORMAL_PROFILES.json`, `OPREGION.diff`: five normal generated source maps
  unchanged against `f90290f`; the private runtime-source change is confined to
  the owner-clear ordering in `opregion.c`.
- `SOURCE_IDENTITY.json`: 210 staged/final build inputs identical and all
  31 protected files unchanged.
- `full-module/RESULT.json`: complete i386 Kbuild/modpost/link in 83.736 seconds,
  one build job, 768 MiB process address-space ceiling; source kernel/cache keys
  and observer symbols unchanged. Module remains uninstalled and unqualified.
- `full-module/MODULE_VERIFICATION.json`: read-only ELF, earlier guard functions
  and six disabled BSS controls passed. These symbol checks do not prove callback
  ordering or live N/N state; actual-source model scope is described below.

Unqualified module SHA-256:
`63dd3e803cf28157df505bda05c040a9cbb8b2a652bca53bae9008359e0ac1d1`.

The callback's global pointer reads have a scheduling seam implemented with a
shim macro. A second lookup that would return NULL is recorded using a safe
fallback mailbox, avoiding an actual invalid dereference. This exercises the
source's two-lookup schedule; it does not prove that the baseline compiler emits
two machine loads, that the hardware exhibited a crash, or that every compiler
would preserve that exact schedule. The correction removes the conflicting
owner write from the callback interval regardless of load coalescing.

Kernel notifier/DRM/workqueue/mapping APIs are modeled; no physical GPU operation,
installation, parameter, boot or Mini 12 change occurred. This addresses one
owner-lifetime contract and does not qualify complete driver teardown or GPU
stop/collection. KMS/PM/legacy review and physical driver qualification remain.

`TARGET_CONTRACTS_INITIAL_REVIEW.json` preserves a failed review-script assertion:
its substring search matched the notifier function declaration instead of the
call in the body. The corrected review matches the `ret` assignment. No driver
or hardware failure occurred in that review attempt.
