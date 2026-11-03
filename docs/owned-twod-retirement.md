# Native 2D retirement record for ordinary drawing

The physical ordinary OpenGL attempt at
`/home/gama/sgx535-offline/normal-matrix-qualification-20261103/v1/`
established a concrete integration gap: `glClear` completed its native 2D
fill, but the first-owner source guard refused the following `glDrawArrays`.
The old producer interval intentionally invalidates that lineage. Its end
does not establish retirement or restore source authorization.

The next owned transition needs the actual fill's native and GEM retirement
facts. Previously the native context and GEM job were cleared before the
fill endpoint returned. This small change makes those facts available inside
the existing kernel call chain, before its owner is freed:

- `remesa_gem_twod_job.h` adds a kernel-only retirement record and an optional
  stable output pointer in the existing launch. It identifies the device,
  default PD, full GEM ticket, native cookie, destination span, packet length,
  core/revision, request-base restoration and owned IRQ status.
- `remesa_twod_native.inc` fills that record only after its existing hardware,
  visibility, IRQ retirement and reservation-release checks succeed. It
  consumes the already checked ledger under the same IRQ lock. There are no
  additional MMIO reads, writes, acknowledgments, resets, flushes or waits.
- `remesa_gem_twod_job.inc` supplies stable job-owned output storage, checks
  identity against the exact held job and marks GEM retirement only after
  releasing its executor, backing holds and views. Malformed native output
  quarantines the job and cannot retain a completed GEM marker. Adapters
  without a native record produce no inferred receipt. A later issue replaces
  the previous record; the ticket and cookie prevent treating it as a new job.

This record is **not source admission**. It neither owns references after FD
loss nor permits dereferencing its borrowed identity pointers. A future
consumer must retain and authenticate the actual owner/file/device/backing
generation and its complete source lifetime. There is no ioctl, user pointer,
new control, source rearm, changed LAB catalogue or changed frozen guard.
The ordinary 2D-to-3D sequence remains refused until that contract is wired.
No new OpenGL call, physical pass or performance improvement is claimed here.

Offline evidence:
`/home/gama/sgx535-offline/owned-twod-retirement-20261103/v1/`.
`tests-v2/RESULT.json` covers 21 builds and 42 runs on native, UBSan and i486:
original, vendor-copy, fill and LAB native adapters with the output disabled
and enabled, plus GEM/copy/fill lifecycles. The GEM tests additionally inject
20 malformed receipt cases while preserving the previous fault/quarantine
checks. Native tests check exact restored state, ticket/cookie, effective
packet lengths and unsuccessful retirement leaving the output unapproved.
`layout/RESULT.json` measures the record and owner sizes. The complete module
build is separate from these modeled GPU/IRQ/backing tests. Neither is a
physical source-transition qualification.

The remaining integration must begin exclusion **before** native base/FIFO
publication and cover the restoration producer interval too. A post-retirement
record cannot retroactively establish it. It must also permit ordinary CPU
resource construction between `glClear` and `draw_vbo`: acquiring the existing
private backing freeze too early would reject the later scene allocations.
Reuse the existing owner, endpoint locks, job holds, MMU and IRQ machinery;
preserve explicit rejection of foreign producers, stale identities, incomplete
retirement, late events and uncertain restoration. Do not clear the old guard
or omit reporting the fill to make the physical test pass.
