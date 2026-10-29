# Private MMU table cache synchronization — 2026-10-28

`remesa_mmu_private_sync_tables` adds the missing CPU-side cache maintenance
for a retained, unbound private directory. The caller must exclude device
activity and serialize the owner across audit, synchronization and eventual
binding. This function does not bind hardware, issue MMIO, invalidate GPU TLBs,
synchronize payload buffers, or manufacture a fence/retirement token.

It first re-audits the exact retained page allowlist. Unsupported cache geometry,
a bound directory, invalid mappings or workspace allocation failure reject the
operation before any cache maintenance. The original driver's `clflush_add`
is a GPU-VA advance, not CPU cache line bytes; the conversion is explicitly
checked. On x86 it holds the MMU write semaphore, brackets maintenance with
memory barriers, cleans every private PT and both dummy control pages, then
cleans the directory last. No default-directory or payload page is cleaned.
Non-x86 builds return EOPNOTSUPP. Later table modification invalidates the
caller's synchronization assumption and requires another call before binding.
There is no per-frame diagnostic hook, hash, ioctl or new execution permission.

Evidence: `/home/gama/sgx535-offline/private-mmu-table-sync-20261028/v2/`.
`SYNC_RESULT.json` qualifies the narrower cache contract; `RESULT.json` retains
the existing owner runner's conservative CPU-only status. Native, UBSan and i386
each pass 180647 owner/auditor checks. Instrumented callbacks verify every line
once for eleven accepted cache geometries, root last, exclusion/barrier ordering,
no data/default-page flush, and rejection without maintenance. The original
confinement regression passes 40459 checks. The complete original antiX MMU
translation unit compiles with the new exported symbol. All 31 protected files
remain unchanged. These results do not prove physical cache visibility.

Remaining runtime work: exclusive engine ownership/idle proof, complete indirect
resource closure, checked root/bank publication, GPU translation invalidation,
completion and retirement, and default-context restoration with quarantine on
uncertain failure. Existing native 2D still requires the shared default root.
No module was installed and no Mini 12 operation was attempted for this change.

Portability scope confirmed by user: other Linux distributions and kernels;
Android/BSD are not the current priority. Common Mesa code should stay separate
from Linux transport and kernel adaptation. This component uses the current
MMU's trusted internal types and has only been kernel-compiled against the
retained antiX 5.10.240 tree. No other kernel/distro compatibility is claimed;
full builds, ABI/ownership tests and physical qualification are required before
advertising each additional combination as supported.
