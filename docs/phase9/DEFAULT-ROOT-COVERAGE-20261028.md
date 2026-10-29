# Default MMU root coverage before private execution

Private-root bind/restore now verifies the actual default directory PDEs against
its retained CPU page-table metadata before any root write. This closes a
specific prerequisite for runtime integration: cleaning pages listed by CPU
pointers alone did not prove that the restored root referenced those pages.
The executor still does not call the private-root primitives. No MMIO, GPU,
Mini12, module installation or boot change was performed.

Evidence root: `/home/gama/sgx535-offline/default-root-coverage-20261028/`.
Authoritative current-source result: `v2/RESULT.json` and native/UBSan/i386 logs.
The new helper is in `kernel/remesa/gma500/remesa_mmu_switch.inc`; tests are
in `kernel/remesa/gma500/tests/test_mmu_switch.c`.

The original qualified antiX `mmu.c` defines a table PDE as its page PFN<<12
OR `pd_mask`. Absent tables use `invalid_pde`; the dummy PT contains
`invalid_pte` in every slot. `psb_drv.c` calls
`psb_mmu_driver_init(dev,1,0,0)`, so its default directory starts in trap mode
with invalid entries0. The original allocator also supports the separately
tested nontrap/invalid_type0 mode, using owned dummy frames with VALID set.
Other default-directory modes remain unsupported by this private transaction.

Under the existing MMU write semaphore and required caller exclusion, the new
preflight checks driver/mask/root identity, dummy-frame representation, each
PT's owner/index/page address and all1024 actual PDEs. Present PDEs must match
the precise PT page; absent PDEs must match the selected invalid entry. It
also checks all1024 dummy-PT entries. It does not inspect arbitrary default
GPU data, rehash buffers or add anything to the current normal rendering loop.
This proves CPU directory/cache-coverage consistency, not data-page ownership,
PTE access-policy correctness, immutability or hardware translation behavior.

Bind checks this before publishing a private root. It may already have cleaned
the private tree; a rejected default preflight issues no root/MMIO writes.
Restore completes its default-tree preflight before cleaning any restore page
or writing the default root. A failure preserves the active/quarantined owner
and all retained resources, using the existing failure path. No guessed reset,
retry, automatic freeing or gate bypass was added.

Each native/UBSan/i386 run passes49736 checks with the original BIF invalidation
function extracted verbatim. Fault injection covers every default PDE and
every dummy-PT entry, wrong modes/mask, failures after private bind, and both
trap and dummy-backed success cases. Failed restore performs no cache clean or
root write. Existing transaction/retention tests continue to pass; confinement
regression remains40459 checks. The full original antiX MMU translation unit
compiles with the new helper. All31 protected files remain unchanged.

v1 passed the initial expanded tests with a dummy-backed main fixture. v2
adjusts the main fixture to the actual psb_drv.c trap-mode initialization and
retains a separate dummy-mode case. Both results are preserved. Exact original
MMU/driver sources, generated object and source hashes are recorded externally.
These CPU tests are not a private-context physical qualification.

The next runtime requirement is still whole-interval exclusion of console2D,
fixed3D, native2D, other MMU writers, IRQ ownership and power transitions, with
retained resource closure and an owned completion/retirement receipt before
restore. The existing2D reservation supplies part of this exclusion but does
not, by itself, satisfy the complete private-root contract. After integration,
physical tests must verify identity, submission, completion, retirement,
readback, guards and recoverable restoration. PDS encoding research is no
longer the default focus; revisit it only if execution evidence points there.
Full userspace3D/2D and installable-driver completion remain unproven.
