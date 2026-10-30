# Exact stolen-framebuffer mapping preflight

The private MMU module now provides psb_mmu_verify_stolen_rw_sequence for the
console's existing contiguous stolen-memory mapping. It checks complete PDE/PTE
values using the unchanged native PTE encoder and the original type0 insertion
profile: VALID, readable/writable, uncached. A PFN-only walk cannot attest those
permissions. The cached GEM verifier is unchanged.

The verifier rejects unaligned or overflowing 32-bit VA/physical extents, zero
page count, a non-default/unbound PD, invalid root PFN/mask, pending TLB work,
missing or inconsistent PT ownership, invalid PT counts/PFNs and any differing
PDE/PTE bit. It checks cross-directory ranges under the existing read semaphore
and table spinlock, without allocating, updating translations or performing
MMIO/cache maintenance.

This is observation, not an ownership grant. The caller must authenticate the
physical framebuffer allocation, retain its backing and serialize relevant PTE
changes across chunks. A successful check alone does not freeze mappings,
establish current hardware root identity, acquire power, or prove visibility,
completion or retirement. The console execution path is not wired to it yet.
It is a prerequisite for the durable framebuffer loan, not a replacement for it.

## Evidence

`/home/gama/sgx535-offline/console-mapping-20261029/v1/` contains:

- `native/RESULT.json`: actual verifier, native PTE encoder and source constants;
  optimized and UBSan tests each pass 155 controls, including every bit of both
  PTEs/PDEs, permissions/cache tampering, top-of-address-space boundaries and
  malformed table ownership. Fixtures remain byte-identical after every check.
- `i486/RESULT.json`: the same 155 controls pass as static i486 under QEMU.
- `SOURCE_REVIEW.json`, `NORMAL_PROFILES.json`: only a private mmu.c include is
  added; prior generated prefix, native PTE encoder, cached verifier and five
  non-private generated maps are unchanged.
- `MAPPING_CONTRACTS.json`: source-bound original stolen insertion, type0 encoder,
  PFN-only walker and observation/ownership boundary.

Page and locking dependencies are reduced CPU models. No real kernel page
tables or GPU operation were exercised. The function is kernel-internal and
adds no ioctl, execution knob, frame hashes or normal rendering-loop cost.
Physical boot, private-root execution, source quiescence and S3 qualification
remain separate. No acceleration or installability approval is claimed.

Full i386 Kbuild/modpost/link passed in 96.480 seconds, one job and
768 MiB per-process AS limit. Read-only ELF guard checks passed and the new
verifier symbol is defined. Original build inputs/symbols and31 protected files
are unchanged; 217 staged/build inputs and current fixtures are source-bound.
Existing build warnings remain in the log. Uninstalled/unqualified module
SHA-256 `527cbbf84e59b1878c4bccea10ba889d191fd2878cdf72d1b3c2d1c17dfdf4a1`. GPU operations: zero.
