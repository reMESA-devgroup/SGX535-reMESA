# Separate 18-binding LAB wire format and userspace transport

The gears scene now has a separate userspace wire definition and Linux
transport. The original 17-binding UAPI remains unchanged. The separately
[staged kernel handlers and positive-witness catalogue](gears-kernel-wire.md)
now implement this format. The physical client, collector and recovery image
still require qualification before using it on the Mini 12.

The new request is 152 bytes, with 18 IDs, explicit zero input padding and
the result beginning at offset 96. Its version is 2; selectors 1 and 2 are
reserved for the exact 1280x800 and 640x640 gears catalogues. Command numbers
13 and 14 are distinct from the existing submission/trace commands 11 and 12.
The request carries no caller packet, VA, CODE or pointer.

The complete trace is 4,280 bytes. It preserves the existing prefix layout
and appends the eighteenth binding explicitly. Its prefix version and byte
count must describe the new complete record. The query is 4,296 bytes. The
envelope helpers reject nonzero result/reserved fields, every zero or duplicate
ID, and any supplied trace bytes; these are not replacements for kernel owner,
image, root, source or native execution checks.

`remesa_linux_memory_gears_scene_lab_submit` checks the complete local resource
inventory, shape 18/11/2, exact dimensions and initialized/finalized TA104
profile before one ioctl. It preserves the raw response and authenticates the
immutable prefix, identity, operation/cookie, completion, retirement and cleanup
receipt. Every entered failure poisons all endpoint resources, including the
last owner. There is no retry or fallback to the old interface. Both protocols
share the existing once-only attempt guard; response getters cannot reinterpret
one protocol's result as the other.

The separate dispatch adapter publishes a host receipt only after successful
native authentication. It performs no hashes, reads or gate changes. It is
not installed in the normal rendering path by this change.

The optional trace helper issues a read-only diagnostic query after an
authenticated successful retirement. It checks owner, operation, status,
counts and all 18 IDs, VAs, sizes and access modes against the submitted local
inventory. Raw trace output remains available on a query failure. A malformed
read-only diagnostic result refuses LAB approval without undoing the already
authenticated retirement. Uncertain submission still keeps the endpoint
poisoned and prevents this helper from querying it. A separate diagnostic
collector may be needed to preserve kernel traces of uncertain attempts.

## Verification and boundary

Evidence is under
`/home/gama/sgx535-offline/gears-scene-wire-20261101/v1/`.
Fifteen suites pass across native, UBSan and i486 builds: the new envelopes
and transport, old scene transport, inventory and upload regressions. New tests
exercise both profiles, 26 submitted response/failure cases, 28 trace cases,
all 18 zero/duplicate IDs, every trace input byte, local leases and aliases,
last-owner corruption, response preservation, dispatch and protocol separation.
The old wire sizes and all 31 protected evidence files are preserved.

These transport tests model syscalls and do not open a device. They do not
demonstrate a new GPU draw. An unsupported ioctl is treated conservatively as
entered uncertainty. Verify the matching staged module/catalogue before any
physical attempt; compiling a module does not install it.

The kernel integration connects the ABI to the shared controller and provides
versioned trace publication for success and failure, preserving the original
ABI, ten profiles and single-attempt gates. Next qualify the native client with the preserved
positive geometry/coefficient witness and independently pinned oracles before
fresh recoverable hardware tests. Ordinary OpenGL/GLES admission and complete
driver installation remain separate unfinished work.
