# Retained CPU-only resources stay outside private GPU mappings

Private GEM preparation now distinguishes the complete retained job from the
subset visible to the GPU. Rule `REMESA_GEM_PRIVATE_CPU_ONLY` has access0 and
canonical cached0. Its object remains pinned/held with the job and validated
against its views, but is not passed to the private MMU allowlist. READ/WRITE/RW
rules keep their previous meanings. No private PTE is created for CPU_ONLY;
it is not encoded as a permissive PTE with access flags omitted.

This closes a concrete gap before adapting the fixed backend: the retained
borrowed-owner constructor deliberately assigns no GPU VA/mapped-pages to roles
whose validated domain is LOCAL. Treating every job resource as GPU-visible
would lose that distinction. Caller role classification still requires the
validated job contract; this patch does not guess which arbitrary resource is
CPU-only or enable a draw.

Changes are confined to `remesa_gem_private.h/.inc` and tests. Preparation
continues to authenticate every job object, including CPU-only backing. It
copies all validated rules, filters only GPU-visible bindings, sorts that
subset, and constructs the existing private owner with its actual count.
At least one GPU binding is required; all-CPU jobs return ENODATA before guard
claim or attachment. CPU_ONLY with a GPU cache flag is rejected. The underlying
MMU access API still rejects access0 and is unchanged.

Bound validation still requires every job view, including CPU-only sidecars,
to match its held GEM authority. It derives only the GPU subset for private
root/closure association. Thus a CPU-only sidecar cannot be freed while the
job/loan is retained, and cannot silently gain GPU permissions. Cancel/close,
exceptional-owner retention, guard-release ordering and legacy issue rejection
are preserved. No data readback/hash, CPU mapping, clearing, cache maintenance,
root publication or command issue is added by this classification.

Evidence root:
`/home/gama/sgx535-offline/private-cpu-sidecars-20261029/v1/`.
`RESULT.json` records519 GEM/loan checks per native/UBSan/i386 variant, using
modeled kernel/GEM/MMU callbacks. The mixed case keeps three job holds while
sending exactly two GPU resources to private preparation/validation; caller
rule mutation, all-CPU refusal and cached0 canonicality are exercised.

`KERNEL_RESULT.json` includes24 additional checks per variant executing the
actual private owner/auditor source in its CPU page-table model. The excluded
resource's PDE/PT remains absent, its page receives no private-owner reference,
and the complete remaining tree passes the allowlist auditor. This is source
construction evidence, not a real GPU page walk. Actual antiX MMU and GEM
translation units compile with the private and normal2D profile flags. No
module was installed. Original checkpoints and all31 protected files remain
unchanged; previous physical2D/gears evidence does not approve this candidate.

The next executable-adapter work also has a cache-publication dependency: the
borrowed fixed owner does not populate the nonborrowed CPU mappings expected
by its legacy publisher, while the stream publisher has its own TA-domain
contract. Private publication must use an authenticated private-job path,
without inventing cache-clean state or bringing back redundant full-buffer
work. Translation/maintenance helpers are available, but the existing backend
is not yet private-context-aware. Full GL/GLES2, desktop2D, VA-API, installability
and physical validation remain unfinished.
