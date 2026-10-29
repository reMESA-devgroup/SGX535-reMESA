# Private GEM preparation borrows the actual GPU reservation

The GEM/private-MMU loan now requires an already-held producer reservation
for its actual device. It claims that reservation with the stable loan token
before private page-table preparation. Reservation release and native2D IRQ
issue reject the token while the MMU loan remains. Private-owner release must
succeed before that guard can be unclaimed and the job detached.
No root publication, new ioctl, GPU execution or installation was enabled.

Evidence: `/home/gama/sgx535-offline/mmu-loan-guard-20261028/v1/`.
`RESULT.json` covers the GEM loan tests; `GUARD_RESULT.json` covers actual
reservation/IRQ/native-adapter source models and six real kernel objects plus
partial link. `COMMANDS.json` and `GUARD_COMMANDS.json` bind commands/logs.
The directory was started under the previous date; it is intentionally retained.

The changes are in `remesa_twod_reservation.h/.inc`, `remesa_twod_irq.h/.inc`
and `remesa_gem_private.h/.inc`, with tests in the corresponding test files.
This extends [GEM preparation](GEM-PRIVATE-PREPARATION-20261028.md); that earlier
qualification's unguarded call signature is superseded by the guarded API.

## Ownership and failure behavior

`remesa_twod_mmu_guard_acquire` takes lock_2d followed by the existing shared
IRQ lock. It checks the expected device, exact held reservation, global IRQ
ledger ownership, unissued state, no pending/cookie/retired state and2D idle.
A nonnull kernel token is stored as `mmu_owner`; it is not a userspace cookie,
GPU address or proof of completion. Another borrower is refused. Release
requires that same token, same reservation and unchanged unissued ledger.
It performs no MMIO writes, ACK, reset or event masking.

The reservation's shared IRQ owner already excludes the fixed3D admission
paths; its existing device owner blocks native/console2D production. The new
field also blocks direct2D IRQ issue and dropping the reservation ledger.
Consequently a caller cannot release the producer reservation while still
borrowing it for private-MMU preparation. Other ownership and power contracts
remain in force; this does not exclude default-MMU writers across a root switch.

GEM preparation authenticates the job/backings first, then attaches the loan,
claims the guard, and calls the actual private owner constructor. Ordinary
preparation failure unclaims the guard before rolling back attachment. A retained
exceptional owner keeps guard, attachment and GEM holds. If guard cleanup itself
fails, those holds remain rather than being discarded. Release destroys only
an unbound private owner, then unclaims the guard, then detaches. Failure at
either stage retains the loan. The original job still requires explicit
cancellation or an eventual qualified executor; no retirement is synthesized.

The guard belongs to exclusive caller-owned kernel storage, which must remain
at a stable address through successful loan release. The caller may release
the reservation normally after the guard is unclaimed. A new guard cannot be
acquired from an already-issued2D job. No normal loop hash, readback, full-buffer
comparison or diagnostic checkpoint was introduced. The added common2D branch
is an ownership check, not a diagnostic gate.

## Qualification

Native/UBSan/i386 each pass333 GEM loan checks against the retained GEM service
source with modeled Linux/GEM, MMU and guard callbacks. Coverage adds guard
acquisition failure, MMU preparation after guard acquisition, retained-owner
failure, release refusal and guard-release failure after successful private
owner cleanup. The real shared guard is tested separately, not inferred from
these callbacks.

Actual reservation/legacy-producer CPU tests each pass641 checks. IRQ models
pass980 parent checks plus62 joined fault models; native adapter models pass
3306 parent checks plus21 joined MMIO fault models, in each compiler variant.
Inherited reservation tests also run inside those latter suites; their counts
must not be summed as independent unique coverage. Guard tests exercise wrong
device/token, duplicate claim, blocked IRQ issue/drop, blocked reservation
release and already-issued job refusal. Original legacy console behavior and
normal native2D success/failure paths remain exercised.

With normal copy/vendor/fill macros enabled, actual antiX units `accel_2d`,
`psb_drv`, `gma500_fixed_backend`, `psb_irq`, `mmu`, `remesa_gem_memory` compile
and partially link; all new driver symbols resolve. This covers the new guard
and private API in the real kernel types. It is not a fully linked/installed
module or a physical qualification. The previous frozen checkpoints and all31
protected files remain unchanged. Earlier physical2D evidence stays historical
and is not used to approve this newly compiled candidate.

## Remaining runtime requirement

The MMU loan now has a concrete producer reservation instead of only a caller
promise. Default-MMU writer exclusion must still span root publication through
restoration; the source MMU writers use driver->sem. Actual3D IRQ ownership
must be transferred under the existing shared lock without opening a console
producer gap; the preparation-only guard currently refuses all issuance.
Owned completion, visibility and retirement must precede checked root restore
and release. No idle snapshot replaces that lifecycle. Physical identity,
submission, completion, retirement, readback and integrity checks remain
required before enabling a textured draw or claiming isolation/FPS gains.
General GL/GLES2, desktop presentation and installability remain incomplete.
