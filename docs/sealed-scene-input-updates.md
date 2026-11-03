# Updating sealed scene data after retirement

The captured frontend/general-session cold/warm path is physically qualified
with frozen inputs at 1280×800 and 640×640. Changing a matrix between its jobs
is still refused in three places: capture holds, Linux `private_active` upload
exclusion, and kernel READ seals. Reusing an idle pipe fence does not authorize
writing through those exclusions.

`remesa_private_session_patch_scene_input` supplies the kernel memory
transaction needed for a future typed update. It is an internal helper with
no ioctl or current general-session caller. Ordinary memory WRITE, write
LEASE and SYNC remain denied; every READ seal stays active throughout.

The caller must retain the outer DRM/source entry and supply a stable policy
derived from initial authenticated program/descriptor admission. Its pure
validator must identify the exact input and data window and validate all
replacement bits. This is not permission to patch arbitrary READ inputs or
CODE. User-controlled policy pointers, functions or raw userspace data are
not accepted by this kernel contract.

The helper copies at most 512 bytes into local staging, then locks endpoint
and memory in the existing order. It requires the same source/backing owner,
an acknowledged DPM retirement operation, valid pool identities, the complete
sealed READ set, and no live jobs, CPU leases, holds or user mappings. Stale epochs,
device/file loss, quarantine and policy rejection refuse the transaction.
The write is DWORD aligned, within the allocation and contained in one page.

Only after validation does it reuse the backing's retained CPU mapping or
temporarily map the affected page. It writes the copied bytes, flushes that
one page and publishes a DMA write barrier. It does not allocate GEM objects,
clear pools, read back frames, compare entire inputs or hash buffers. A refused
policy or failed temporary mapping changes no content. The current READ loan
permissions already forbid GPU writes to the input, so this operation needs
no GPU-to-CPU transfer of device-written data.

## Verification and integration still required

Evidence is under
`/home/gama/sgx535-offline/scene-input-epochs-20261103/v1`.
The actual memory-service/seal/transaction code is tested with injected
GEM/MMU/cache APIs in native, UBSan and static i486/QEMU variants. Prior hardware
retirement and typed policy admission are explicit model assumptions. These
tests are not new physical GPU or shader qualification.

The test policy authorizes one 36-word data window using the existing retained
finite/bounded float rule. Tests cover stale epochs, incorrect owner/ID,
forbidden prefix and partial writes, bounds/alignment, nonfinite values,
jobs/aliases/leases/quarantine, mapping failure, unchanged bytes outside the
window, retained mappings and exactly one affected page's cache publication.
Existing READ-seal, DPM-seal and eleven-role input tests remain separate.

Before making this callable by the frontend, admission must bind the real
parameter DATA window from the existing encoder metadata; the test policy is
not a replacement for that admission. Linux transport and frontend capture
state must participate in the same acknowledged update, including uncertainty
handling. The general LAB catalogue still authenticates its complete original
inputs and must not be bypassed to accept changed bytes. A changing-input
physical experiment needs its own presealed expectations before execution.
The existing PDS/USSE/TA encoders and qualified program bytes need no rewrite.
