# Private console synchronization and PM ownership

The private psbfb_sync now takes a non-forcing power claim before its first
lock/register access and releases it after the original poll loop unlocks.
Offline admission returns EAGAIN without reading registers; missing framebuffer
metadata returns ENODEV. Success, FIFO timeout and blitter timeout all balance
the reference. The existing private fbops DRM-entry wrapper remains responsible
for joining planned device teardown.

The native poll conditions, timeout extent and return values for admitted calls
are unchanged. This is a guard on the console synchronization path, not a new
rendering-loop diagnostic. Five non-private profiles and the qualified native
checkpoint are unchanged. Native console submit/copy, source masks, GPU command
encoding and retirement gates are not changed by this correction.

## Confirmed ownership distinction

The retained GEM memory open path acquires a power reference before device
exclusion and holds it for the context lifetime. Its close refuses live jobs
outside unattached PREPARED state, leased/quarantined backing and outstanding
VMAs before releasing that reference. Software file/device-loss handlers do
not release it. The PM display-count check therefore remains relevant to
retained GEM ownership; a borrowed owner's boolean alone is not the reference.

Native console submission has a different contract. psbfb_2d_submit returns
after putting commands in the FIFO, and psbfb_copyarea_accel then releases its
power reference. The producer-end callback invalidates source history; it does
not wait for execution. The target Poulsbo power_down callback returns zero
without a GPU drain. Thus no CPU reference remaining does not establish
retirement of previously queued console commands.

The console fixture preserves a counterexample to this broader PM contract
in both old and corrected modes: FIFO writes queue delayed work, submit returns,
the modeled caller releases its reference, and the PM count check admits closure
while work remains queued. No physical device was put in that condition.
This remains unresolved by the psbfb_sync access guard.

The target fbmem.c also contains generic CPU read/write and pixmap paths that
discard the fb_sync return value. Reporting EAGAIN prevents this callback's
offline MMIO but does not make those callers safe against pending GPU writes.
Do not claim general CPU/GPU framebuffer ownership from this guard.

## Evidence

Root: `/home/gama/sgx535-offline/private-console-power-20261029/v1/`.

- `counterexample/RESULT.json`: actual native sync/submit bodies, native and
  UBSan, demonstrate offline MMIO and modeled power closure during a paused sync.
- `guards/RESULT.json`: actual corrected bodies pass native and UBSan controls:
  offline denial, active sync blocking closure, idle success, FIFO/blitter busy
  timeout, completion during polling and missing metadata. No leaked reference.
- `i486-counterexample/RESULT.json`, `i486-guards/RESULT.json`: static i486/QEMU
  counterparts. Compiler sign-compare suppression applies to retained native
  submission code; actual extracted bodies are not rewritten to satisfy tests.
- Every mode explicitly records the remaining asynchronous-console PM
  counterexample. driver_safety_pass is false and GPU operations are zero.
- `CALL_CHAIN_REVIEW.json` and `*-actual.c.txt`: source-bound GEM open/close,
  console caller, PM IRQ mask and Poulsbo power-down observations, plus the
  target fbmem.c hash for the ignored return-value limitation.
- `SOURCE_REVIEW.json`, `NORMAL_PROFILES.json`: only private accel_2d.c changes;
  submit/copy caller bodies and all five non-private generated maps unchanged.

PCI/PM admission, locking, time and FIFO/MMIO are reduced dependencies. The
scheduled pthread test is not a real kernel PM execution. Delayed queued work
is a model of the asynchronous contract, not a GPU retirement observation.

## Remaining qualification work

Console submission needs an attributed completion/retirement and durable
power/ownership interval across queued work, with failure retention rather than
assumed success. An idle status sample or source invalidation cannot replace
that contract. Existing GEM context references must stay held until their
established close gates permit release. Registered PM IRQ masks, complete
source quiescence, partial-failure recovery and physical S3 remain unqualified.
No module installation, physical MMIO, GPU operation, reboot, suspend or push
is performed for this milestone. N/N, quarantine and boot recovery are preserved.

## Build result

Full i386 Kbuild/modpost/link passed in 106.371 seconds with one job and a
768 MiB per-process address-space ceiling. Original build inputs/symbols are
unchanged; existing warnings are retained in build.log. SOURCE_IDENTITY.json
binds 215 staged/build inputs, the current generator, four source-bound fixture
results, reviewed call-chain sources and 31 protected inputs. Read-only ELF
verification passed for the existing guards and six disabled controls; this
does not establish runtime ordering or physical safety.

Uninstalled, physically unqualified module SHA-256:
`4b9dc69ed87f3a8b5103c930251920bf5cdc53f93aeb3493a4ba0d9fc81a7967`. GPU operations: zero.

Follow-up: `private-console-timeouts.md` records removal of tick-dependent waits
and automatic FIFO-timeout reset in the private candidate. The asynchronous
power/ownership counterexample remains open.
