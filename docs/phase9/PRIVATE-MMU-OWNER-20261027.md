# Owned unbound SGX page-directory preparation

`remesa_mmu_private.h/.inc` adds ownership around the
[private-directory auditor](PRIVATE-MMU-AUDIT-20261027.md). It is kernel-side
preparation only, not wired into a native job, public ioctl or selected module.
No hardware page-directory publication or context switch is implemented here.

`remesa_mmu_private_prepare()` accepts up to32 sorted/disjoint trusted kernel
bindings. It copies the bindings and page-pointer arrays, holds every data page
with `get_page()`, allocates a new trap-on-invalid directory with the original
MMU allocator, inserts exact read/write/cache permissions, then calls the full
auditor. Changes to the caller's arrays after preparation cannot redirect the
owned mappings. The caller must keep the driver/default directory alive and
serialize device activity and inputs; this is not a user-pointer interface.

Original insertion calls are split at PDE boundaries and contain at most1024
pages. This includes the last32-bit VA page and avoids passing a wrapping
whole4GiB span to the old32-bit address arithmetic. Binding size, allocation
capacity and actually mapped pages remain distinct.

Ordinary failures destroy the unbound directory, including partially inserted
tables, **before** dropping any data-page reference. Release is idempotent for
a NULL owner. A bound directory is refused with `-EBUSY`, preserving the entire
owner and all page references. If an unexpected bound directory prevents
rollback during preparation, `-EUCLEAN` returns the retained owner through the
output pointer rather than silently discarding it. Callers must handle that
exceptional ownership result. No API here exposes or binds the private PD.
Merely changing `hw_context` is not hardware retirement or restoration proof.

## Evidence

`tests/test_mmu_private.c` uses the actual owner/auditor code, original `mmu.h`
and CPU shims for allocation, mapping and page-reference operations. Native,
UBSan and i386 each pass367 owner/auditor checks. Tests include:

- Successful READ/WRITE/RW preparation across PDE boundaries and the final VA.
- Independent copied metadata despite subsequent mutation of caller arrays.
- Failure at each ownership/workspace/directory allocation stage.
- Partial insertion failures at every mapping call, with complete cleanup.
- Permission corruption, invalid PFNs, NULL pages and physical/control aliases.
- Duplicate references to the same `struct page`, with balanced rollback.
- Refusal to destroy a bound PD, and preserved ownership on failed rollback.

The existing auditor regression still passes40,459 checks. The only change to
its test source is an explicit `return 0`, allowing that test's entry point to
be renamed and reused by the ownership model. All runs have empty stderr.

The complete original antiX5.10.240 `mmu.c`, with auditor and owner includes
appended in a private source copy, compiles with the retained i686 command and
real kernel headers. Its object defines the auditor, prepare and release
symbols. Original module sources/commands remain unchanged. These are CPU-model
and kernel-object build results, not real Linux page-lifetime or GPU execution
qualification. No new module was installed.

Exact sources, commands, hashes and logs:

`/home/gama/sgx535-offline/private-mmu-owner-20261027/v2/RESULT.json`

`/home/gama/sgx535-offline/private-mmu-owner-20261027/v2/RUN.py`

The initial367-check version supersedes the329-check first pass retained in the
parent evidence directory. The first compilation stopped on test-only warnings:
adjacent `if` statements and the renamed main's implicit return. Both were
corrected without weakening compiler warnings or runtime checks.

## Remaining integration

The normal draw path still needs a trusted resource/permission closure wired
to this owner, device lifetime holds, CPU/GPU cache publication, atomic context
binding/default restoration, fences and retirement/failure handling. None of
these are supplied by successful CPU preparation. The PDS CODE84 contract and
paired desktop-GL/GLES2 physical qualification remain pending. The working
GLXGears path, FAST profile and recoverable boot are unchanged.
