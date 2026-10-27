# Owned pre-ACK 2D event capture

The native 2D reservation now shares the existing fixed IRQ lock with the
qualified 3D backend. `remesa_twod_irq.h/.inc` adds an independent 2D owner and
event ledger in the native backend translation unit; it does not alter the 3D
benign mask, lifecycle7 or event interpretation.

The staging tool inserts capture into
`sgx535_gma500_fixed_irq_capture_locked`, which the unchanged pinned
`psb_irq_handler` invokes under that same lock before `psb_sgx_interrupt`
acknowledges status. Both admission checks in the 3D begin path also reject a
live 2D IRQ owner. Conversely 2D reserve rejects an existing 3D IRQ owner.

Reservation publication and removal take `lock_2d` then the shared IRQ lock.
The interrupt handler takes only the IRQ lock. It cannot dereference a token
after successful release because removal is serialized with capture. No path
introduced here acquires the producer lock while holding the IRQ lock. The
global IRQ reservation permits one native owner across devices, matching the
existing fixed backend's singleton ownership; producer pointers remain local
to each device.

The kernel adapter must establish a real idle/stale-event-ACK boundary under
the same IRQ lock before calling `remesa_twod_irq_issue_locked` and writing the
first FIFO word. This helper records possible issue and one nonzero cookie; it
does not establish that hardware boundary itself. It refuses rearming a live
operation. Capture before issue or from another device cannot enter that
operation's ledger. Capture after issue retains every status bit except the
aggregate master bit31; faults and unexpected events are never filtered away.

Snapshot requires the exact owner and cookie and leaves output unchanged on
failure. `remesa_twod_irq_mark_retired_locked` requires exactly bit27 and no
status2 event. It is only a ledger marker: the native adapter MUST first prove
device visibility, DMA quiescence and state restoration. The helper cannot
turn a CPU snapshot into hardware retirement. Drop refuses an issued operation
without that marker, or one with later unexpected events. Producer release
still checks power, FIFO/busy state and restored request base before dropping
IRQ ownership. There is no clear-ledger, force-drop, rearm-on-timeout or reset
recovery API.

All new IRQ routines are kernel-only and require the existing lock (asserted
with native lockdep). They allocate nothing, issue no MMIO, acknowledge no IRQ,
read no framebuffer, compute no hash and emit no per-frame diagnostics.
No userspace completion injection or executor registration is installed.

## Evidence

`/home/gama/sgx535-offline/twod-irq-20261027/` contains RUN.py, RESULT.json,
CLOSEOUT.json, staged sources/manifest, exact extracted native functions,
native/UBSan/i386 logs, private kernel objects and the partial native link.

Each variant runs the updated reservation/actual legacy producer suite (510
checks), then the actual IRQ ledger and actual staged capture wrapper (838
parent checks including that regression, plus 62 joined status-fault child
cases). Tests cover old events before possible issue, wrong cookies, refused
rearm/drop, foreign-device events, master-bit handling, every other status1 and
every status2 bit, and a fault arriving after the retirement marker. Unknown
events retain ownership and prevent release. The staged wrapper preserves the
old 3D behavior of excluding bit27 and master while retaining other bits.
The explicit page/register/lock models are CPU evidence, not Linux IRQ timing
or physical completion proof. The final parent fault fixture deliberately
retains its CPU token until process exit; no GPU operation was issued and this
is not represented as hardware retirement.

Complete accel_2d.c, psb_drv.c, gma500_fixed_backend.c and psb_irq.c compile with
their retained real antiX i686 commands. A relocatable link of the native
producer/backend/handler resolves all new 2D and shared IRQ symbols. It is not
a linked/installable kernel module. Original input hashes remain unchanged.

Staging now pins five native inputs: accel_2d.c, psb_drv.h,
gma500_fixed_backend.c, psb_reg.h and psb_irq.c. Existing-output and changed-input
refusals remain tested. Historical producer-only evidence at
`/home/gama/sgx535-offline/twod-reservation-20261027/` remains intact; its earlier
independent-device behavior is superseded by shared singleton IRQ ownership.

Next required implementation is the actual native adapter: core/revision and
selected-MMU-root authentication, physical stale-event boundary, request-base
write, ten-word FIFO emission, poll/IRQ association, device cache visibility,
and atomic quiet/restoration/IRQ release on retirement. Keep the reservation
on any uncertainty. Then qualify paired 1280x800/640x640 and occasional641x479
copies with GPU identity, submission, completion, retirement, readback and
guards before Gallium dispatch. No new GPU job, FPS, API capability or physical
2D acceleration is claimed by this increment.
