# Held visible capture lifecycle (CPU integration)

The existing split sequence API already supports retained readback. No new
production wrapper, ioctl, executor admission, or job state is needed. The
concrete integration is `kernel/remesa/gma500/tests/test_depth_visible_lifecycle.c`.
It compiles the original GEM service and sequence implementation, calls the
actual read-only visible endpoint, and accumulates all 1,000 chunks of a
4,096,000-byte synthetic 1280x800 image while the job remains VISIBLE and held.
Only after capture succeeds does the caller acknowledge and retire the job.

`remesa_retained_submit_once()` is unsuitable for this capture interval: its
`remesa_sequence_execute()` call performs issue, wait, publish, acknowledge,
optional detach, and retire synchronously before returning. Mode 12 proves
that successful execute leaves readback returning ENOENT. A future isolated
controller should instead use prepare, attach its qualified session, issue,
wait, publish, read all chunks, acknowledge, detach, and retire. It must retain
its endpoint, session, source epoch, original observer records, and same-task
isolation throughout. These tests do not qualify observer policy for a future
cross-syscall split; actual source/observer admission must independently permit
that interval.

Thirteen modes cover successful retained capture, prepared cancellation,
validation rejection, uncertain start, poll timeout, completion-ticket mismatch,
publication failure, readback PTE corruption, transient readback vmap failure,
interrupted reader/close refusal, acknowledgement failure, attached alias
retirement refusal, and synchronous execute retirement. Every mode also checks
prepare allocation rollback and the absent default executor. Faults after possible
issuance retain backing; transient readback ENOMEM leaves VISIBLE unchanged and
allows the same reader to resume. The synthetic attached flag in mode 11 models
only the GEM lifetime check, not real borrower/session construction. PTE failure
in readback updates authoritative GEM state; the next sequence transition observes
the quarantine. No completion, visibility, or rendered-pixel claim is made.

Checkpoint: `/home/gama/sgx535-offline/depth-visible-lifecycle-20261023`.
Reproduce with `python3 /home/gama/sgx535-offline/depth-visible-lifecycle-20261023/run_tests.py`.
Native, UBSan, and static i386/QEMU run the same 13 modes. Imported original
sources and all 31 protected admission/gate hashes are checked before and after.
The output BGRA files are explicitly CPU_SYNTHETIC fixtures.

The remaining native 1280x800 blocker is the qualified image/executor ingress,
not the lifecycle or chunk-size mechanism. The original native session create
requires ten roles and a 264-byte frozen command-pairs image. The borrowed owner
requires each exact `sgx535_frozen_get_requirement()` size and uses frozen
session/fire-wire planners. `remesa_retained_triangle_images()` compares those
full FIRE3 images and commands byte for byte. The owner executor's visible method
calls fixed `sgx535_gma500_owner_capture_color()` and stores only 4096 color bytes
in its readback and provenance response. The native triangle request is exactly
32x32. Enlarging capture storage or accepting different role counts cannot make
this a depth-capable 1280x800 executor.

The smallest justified next hook is a separately qualified owner/profile binding
at retained session construction, before `remesa_borrowed_owner_init()` locks the
fixed descriptor requirements. That binding must supply exact generated depth
image roles/sizes and command/owner validation plus the qualified event/background
program/image and publication path. Existing exact32 validation must remain the
original branch. It is premature to expose a generic callback wrapper: the new
native depth profile and its maintenance/evidence contract are still missing.
No implementation of that hook or weakening of the fixed profile is included.
