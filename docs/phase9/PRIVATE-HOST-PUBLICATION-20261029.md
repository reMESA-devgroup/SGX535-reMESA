# Private GEM upload publication

`remesa_gem_private_publish_locked()` supplies the cold-upload publication step
missing from the borrowed GEM/private-directory integration. The retained
nonborrowed owner publisher requires `bo.cpu` and drops host aliases; the borrowed
owner deliberately owns neither. This API uses the already held GEM page arrays
and preserves the job's cached CPU mappings.

The caller holds the endpoint mutex followed by the MMU write semaphore, with the
private root bound and the existing device/producer ownership contract retained.
All trusted kernel writes must be finished. The caller excludes any further CPU
access through borrowed views until retirement and an appropriate CPU ownership
transfer. GEM holds already prohibit user leases and topology changes.

Before any cache operation the existing bound validator authenticates the entire
resource closure, page identity, view identity, guard and private translations.
Only a prepared, cookieless job can publish. Every GPU-visible backing is flushed
using `drm_clflush_pages`, followed by one `dma_wmb`; CPU-only sidecars are skipped.
GPU write-only destinations also need publication: dirty CPU lines must not later
write back over GPU results. Completion of this operation records `host_published`
in the retained loan. Repetition returns `-EALREADY` without another flush or
barrier. Alias ownership and lifetime are unchanged.

This is deliberately a cold-upload primitive. It does not infer that an earlier
MEM_SYNC cleaned a buffer, establish cross-job cache epochs, or replace the
existing streaming ownership protocol. A fresh loan conservatively flushes all
GPU-visible pages. It must not be inserted into every FAST frame as a substitute
for proper reuse. The normal FAST path is unchanged. No hashes, readbacks, pixel
oracles, dumps, or per-frame allocation have been added.

No executor admission, command publication, GPU submission, visibility receipt,
retirement, restore, or CPU readback ownership is supplied by this function. The
old fixed backend is not switched over merely by defining this helper. Integration
still needs private-aware callbacks and whole-interval MMU writer exclusion,
including safe handling of uncertain completion and queued default flushes.

## Evidence

Authoritative model run:
`/home/gama/sgx535-offline/private-host-publication-20261029/v2/RESULT.json`.
The actual retained GEM service is compiled with modeled MMU/guard callbacks in
native, UBSan and i386 variants. Tests check invalid final views cause no partial
publication, bound-validator refusal, issued/cookied job refusal, exact flushed
page arrays/counts, GPU write-only inclusion, CPU-only exclusion, barrier ordering,
publication state, repeat refusal, and retained borrowed mappings. These models do
not prove physical cache coherence or hardware retirement.

Actual antiX MMU and GEM translation units compile with private/copy/vendor/fill
flags in:
`/home/gama/sgx535-offline/private-host-publication-20261029/v1/KERNEL_RESULT.json`.
The staged implementation/header there match the final implementation; v2 changes
only tests. This is not a full module link, installation or hardware qualification.
The earlier v1 model run is preliminary and superseded by v2's additional borrowed
mapping coverage.
