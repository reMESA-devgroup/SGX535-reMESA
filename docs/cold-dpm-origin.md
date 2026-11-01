# Cold DPM zero provenance and selected kernel views

Fresh GEM pages were already cleared once at first pin. The maintained staging
layer now tracks `zero_pristine` separately from `initialized`: it attests only
that those zero contents have not been exposed to a possible writer. It does
not attest GPU cache visibility, root publication or retirement.

Writable CPU leases, exported writable kernel views, unpin/discard, possible
generic job issue and private publication of GPU-writable backing invalidate
the provenance conservatively. Read-only user leases do not. Failed view
preparation does not consume provenance when no new alias was returned.
The previous first-pin clear remains unchanged; no new clear or buffer scan
is added to the render path.

`remesa_gem_job_claim_zero_dpm()` is a trusted kernel-only operation for a
prepared, unattached job. It authenticates two distinct held IDs, fresh zero
provenance, complete pinned mappings, absence of CPU aliases/leases, and the
existing two32MiB pool profile: SURFACE table plus1MiB-aligned RASTER parameter.
Both must validate before a job-local mask reserves either. Repeating that claim
in the same prepared phase returns EALREADY. The mask resets on job-slot reuse.
Unknown/modified backing returns an error rather than silently clearing it or
assuming it is zero. No ioctl or userspace zero-content credential is created.

`remesa_gem_job_snapshot_selected()` returns the full allocation metadata but
maps only an explicit trusted CPU-view mask. Claimed DPM pools cannot be selected;
the legacy all-view API also refuses such jobs. Their returned cpu pointers are
NULL, page arrays and job holds remain, and no64MiB vmalloc alias is needed.
Existing exported aliases cannot be hidden by changing the mask. Last-map
failure rolls back newly created aliases without partial output.
A private consumer uses the existing complete loan/translation validator, which
already supports optional CPU pointers. The legacy all-CPU snapshot checker is
unchanged and rejects these partial-CPU snapshots by design.

Publication still flushes GPU-visible pages and performs its required barrier.
The new bit does not skip cache work or mean the device consumed zero contents.
GPU-write publication consumes the provenance before possible use, even if
execution later fails. Native execution, ordered DPM loads, retirement and any
post-render reuse contract remain separate requirements. Existing LAB/FAST
profiles and their cold-clear/reuse policies are preserved.

Evidence root: `/home/gama/sgx535-offline/cold-dpm-origin-20261031/v1`.
`tests-v2/RESULT.json` records native/UBSan/statici486 tests with the actual
transformed retained core/private publisher and modeled kernel primitives.
They allocate two complete32MiB model backings, count every first-pin clear,
verify no additional clear/map/flush/scan on claim, test writable/readonly
leases, view exposure, repin, wrong shape/VA, atomic refusal, selected-view
rollback, publication, slot reuse and pre-start invalidation on uncertain issue.
The model PTE implementation is explicitly indexed to support all16K pool
pages; it is not a physical MMU execution or performance measurement.

`capacity-regression-v2/RESULT.json`, `execution-regression/RESULT.json` and
`controller/RESULT.json` preserve capacity/MMU/lifecycle/private/LAB gates.
`NORMAL_PROFILES.json` isolates bookkeeping, the new claimed-pool refusal and
helper include across five profiles; other guard/execution source text matches
the preceding stager. `full-module/RESULT.json` records a complete copied-output
antiX i486 build; `BUILD_WARNINGS.json` shows no new warnings.
`FINAL_BINDING.json` pins source and receipts. Frozen checkpoints are unchanged.

The module is not installed or physically qualified. These APIs are not yet a
trusted general-scene executor or complete desktop/GLES2/GL2.1 support. The next
consumer must explicitly claim pools, choose input views, validate the complete
scene, publish and execute under authenticated device/retirement ownership.
