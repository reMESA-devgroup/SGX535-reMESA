# Private console waits without tick-dependent timeout or reset recovery

Private console FIFO-room and fb_sync waits now use elapsed fast-clock time,
with the existing approximately one-second budget, instead of requiring jiffies
to advance while interrupts may be disabled. FIFO timeout returns EIO without
calling psb_spank. Sync still reports EBUSY for an engine that remains busy.
Timeouts do not claim completion or release GPU-owned buffers.

The original FIFO-room helper can be called under lock_2d with local interrupts
disabled. Its timeout depends on jiffies, and its loop has no scheduler yield.
fb_sync also polls under that lock with interrupts disabled and uses jiffies.
If ticks do not advance there, neither timeout condition forces an exit. The
tests demonstrate that conditional failure; they do not claim a physical hang
was reproduced on the Mini 12 or assume its online CPU/tick configuration.

The old FIFO timeout calls psb_spank, whose retained body asserts reset for
BIF, DPM, TA, USE, ISP, TSP and 2D, then clears BIF fault state. This does not
establish retirement of other issued work. The private timeout now preserves
that hardware state and returns the existing error. The original initialization
reset function and initialization call are unchanged. Native packet construction,
FIFO submission, copy caller, IRQ masks and all five non-private profiles remain
unchanged. No CPU rendering replaces GPU execution in this correction.

The target kernel exports ktime_get_mono_fast_ns and documents its NMI-safe
access. It can step backwards across a timekeeper update. Unsigned elapsed-time
subtraction conservatively produces timeout if a sample is earlier than the
start. This may shorten a failed wait; it must not turn failure into completion.
This is not a guarantee under a broken/non-advancing hardware clocksource.

## Evidence

Root: `/home/gama/sgx535-offline/private-console-timeouts-20261029/`.

- `v2/counterexample/RESULT.json`: actual retained FIFO wait, submit and sync
  bodies, native/UBSan. Tick-driven timeout invokes the modeled reset dependency;
  frozen-jiffies FIFO/sync children fail to return before their one-second host
  alarm. The alarm terminates CPU test children, not a driver or GPU job.
- `v2/guards/RESULT.json`, `v2/i486-guards/RESULT.json`: corrected native/UBSan
  and static i486/QEMU controls. Frozen-jiffies waits return the expected error
  while preserving queued state and without reset. Backwards-clock controls
  return failure. Earlier offline-MMIO, PM admission, timeout-reference balance,
  missing-metadata and successful-sync controls remain.
- The asynchronous-console PM counterexample remains explicitly present in
  both modes. These tests do not prove durable ownership after submission.
- `v2/CLOCK_RESET_CONTRACTS.json`, `psb_spank-actual.c.txt`: target kernel clock
  API/provenance and actual reset body. The reset dependency in the fixture
  records reset/queued-state effects; it does not execute physical reset MMIO.
- `v2/PRIVATE.diff`, `SOURCE_REVIEW.json`, `NORMAL_PROFILES.json`: bounded private
  wait changes, untouched packet/reset/submit/copy bodies and five normal profiles.

Initial `v1/guards` and `v1/i486-guards` compilation failures are retained. The
fixture MMIO macros originally omitted the dev_priv argument used by real kernel
macros, leaving an apparently unused parameter after reset removal. The corrected
fixture consumes/asserts that argument without changing extracted driver bodies.
Exact v1 fixture/runner snapshots are in `v1/SOURCE_SNAPSHOTS`.

## Remaining work

Console power and backing must still remain owned across asynchronous execution
until attributed completion/visibility/retirement. The native 2D executor's
existing launch contract expects authenticated mappings and retained job storage;
it cannot simply borrow a callback's stack context or a framebuffer raw pointer.
Generic fbdev callers that ignore sync errors also need an ownership contract.
Source quiescence, failed-operation recovery and physical S3 remain unqualified.
No hardware test, installation, reboot, suspend or push is part of this change.
All tests report GPU operations zero and driver_safety_pass false.

## Build result

Complete i386 Kbuild/modpost/link passed in 91.898 seconds with one job and a
768 MiB per-process address-space ceiling. Original build inputs/symbols are
unchanged; existing warnings remain in build.log. Read-only ELF verification
passed for the existing guards and six disabled controls. CLOCK_SYMBOLS.txt
records the clock import, not runtime clock qualification. SOURCE_IDENTITY.json
binds 215 staged/build inputs, current generated sources, final fixtures, clock
and reset source contracts and all 31 protected inputs.

Uninstalled, physically unqualified module SHA-256:
`0ea9417a05d9bb9bc60ed46cb401e3d7a7159b781f0a703b632fffa853653ba1`. GPU operations: zero.
