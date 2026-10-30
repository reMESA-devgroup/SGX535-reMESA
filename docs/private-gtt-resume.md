# Private GTT resume failure handling

The private candidate stops restoration on initialization or range-insertion
failure and retains the existing mapping/resource owners. Previously,
psb_gtt_restore ignored psb_gtt_init's return value and continued inserting
pages after its error path had called psb_gtt_takedown and cleared gtt_map.
The retained initializer also rejected changed geometry only when both the
table size and stolen size changed, accepting either change alone.

Cold initialization keeps the original function body under a private static
name. The public wrapper selects a checked reuse path for resume. No native
checkpoint or non-private profile is changed. The PTE encoder, entry lookup,
insertion body and cold-init teardown remain unchanged. This is not a new GTT
format or a change to successful command/shader encoding.

## Resume contract

Resume requires established GTT/stolen mappings, scratch page and resource
root. It checks PCI configuration-read statuses, current table physical base,
stolen base, table BAR/page count, aperture base/page count and resource extent
against the existing graph. A change in any of them is rejected. Stolen length
must fit the table entry capacity and aperture; scratch/page PFNs must fit the
existing 32-bit PTE encoder. The table's mapped page count and its PTE entry
capacity are calculated separately. The successful stolen and scratch writes
retain the original initializer's exact extent; this change does not silently
expand that extent.

All ranges with backing pages are checked before any table-entry writes:
positive page count, valid roll, aligned resource extent, page-count agreement,
aperture and table bounds, non-NULL pages and encodable PFNs. The GTT mutex
covers validation and restoration. A failed initializer prevents range replay;
a failed insertion is returned instead of being ignored. The outer PM callback
already keeps admission closed on these failures. It now also propagates the
GMCH configuration-word write status before continuing to GTT restoration.

The resume helper does not allocate, unmap or replace mappings, mutate the
resource graph or overwrite the original saved gmch_ctrl/pge_ctl values. An
error does not establish recovery: partial hardware programming can already
have occurred, and GPU stop/retirement and physical PM remain unqualified.

## Evidence

Root: `/home/gama/sgx535-offline/private-gtt-resume-20261029/`.

- `v1/counterexample`, `v2/counterexample`: actual retained init/restore/
  teardown/PTE/entry/insert bodies demonstrate continued insertion after failed
  initialization, accepted single-size changes and ignored configuration-write
  failures. Missing-map insertion is recorded by a dependency wrapper instead
  of deliberately causing a host invalid access. Valid insertion delegates the
  unchanged actual insertion body.
- `v2/guards`: optimized native and UBSan controls pass. `v2/i486-guards`:
  static i486 controls pass under QEMU. Cases include missing mappings/root,
  config read/write failures, table/stolen/base/aperture changes, high table
  address, range extent/roll/page-count/table-capacity errors, invalid pages/PFNs,
  insertion failure, empty tree and successful stolen/scratch/rotated replay.
- `v2/SUCCESS_TABLE_COMPARISON.json`: two valid layouts produce byte-identical
  32,768-byte combined table outputs across the old native/UBSan model and the
  corrected native/UBSan/i486 models. These are CPU model outputs, not physical
  readbacks or proof of PCI/MMIO timing on the Mini 12.
- `v1/admission`, `v2/i486-admission`: previous admission, PM-owner and error
  regressions pass, including denial after the outer GMCH write fails.
- `v2/SOURCE_REVIEW.json`, `v2/NORMAL_PROFILES.json`: retained cold initializer
  body and PTE/entry/insert/teardown bodies unchanged; all five non-private
  generated profiles unchanged.

The initial v1 fixture outputs and exact fixture/runner snapshots are preserved
under `v1/SOURCE_SNAPSHOTS`. The v2 fixture adds scratch-write, empty-tree and
additional bounds controls. Test dependencies model PCI, resources, MMIO,
mapping ownership and locking; these are not real kernel PM executions.

## Qualification boundary

This candidate is not installed or physically qualified. No Mini 12 suspend,
reboot, register access or GPU submission is performed by the offline tests.
Cold-init failure cleanup still uses the retained implementation and is outside
this resume correction. Source quiescence, GPU stop/retirement, complete
partial-failure recovery, physical S3 and surprise physical removal remain open.
The existing quarantine, N/N and boot-recovery requirements still apply. A
failed or changed resume graph must not be treated as an approved rebind.

## Build result

Complete i386 Kbuild/modpost/link passed in 98.592 seconds, one job and a
768 MiB per-process address-space ceiling. Existing build warnings are retained
in the build log. Read-only ELF verification passed, including existing guard
symbols and six disabled controls. Original build inputs and symbols are unchanged.
`v2/SOURCE_IDENTITY.json` binds 215 staged/build inputs, the current generator
and fragments, final fixtures and all 31 protected inputs.

Uninstalled, physically unqualified module SHA-256:
`c4cdf4111dcab06eb83de766b4df072468c6097dc3871e9e2232e4f489d0d418`. GPU operations: zero.
