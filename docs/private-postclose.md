# Private postclose after unplug: retain resources, discard file identities

The private driver now protects its postclose and lastclose callbacks with the
same DRM enter/exit interval as its other CPU entries. A callback admitted before
unplug follows the existing cleanup path; unplug waits for that interval. After
unplug, lastclose skips the framebuffer helper and postclose invokes only the new
software orphaning helpers.

The orphaning path does not call backend `close` or `quarantine` callbacks, unmap,
unpin, release pages, drop module/device/power references, or access MMU/registers.
It clears matching `drm_file` pointers from bridge, trusted-stream and memory
endpoint state. It marks active stream/jobs/backing quarantined and preserves
their handles, operations, cookies, command storage, borrowed views, pages and
reference ownership. It poisons the existing stream source in software. Wrong
device/file identities are ignored; repeating the operation is harmless.

Lock order is explicit: bridge `stream_lock` protects bridge/trusted metadata and
its existing source-poison call. That lock is released before taking
`endpoint_lock` then the memory lock. No source-to-endpoint lock inversion or
hardware callback is introduced. The trusted helper is an internal entry whose
caller holds the bridge stream lock.

A monotonic `private_device_lost` field is appended to the private memory owner.
Its close function refuses later cleanup with ENODEV, including an empty owner:
even dropping power references after device teardown could touch invalid driver
state. This flag is internal and adds no userspace ABI or enable parameter. Work
runs only at file/device loss, never in the production frame loop.

## Remaining removal blocker

**These helpers do not make PCI unload safe for a retained GPU job.** Original
unload can still free the MMU/default directory behind that retained job. Joining
CPU callbacks and retaining their backing do not establish GPU stop/isolation.
That policy, late standard GEM destruction and rebind remain unresolved; the new
module must not be installed or physically enabled as a qualified private driver.

In the inspected Linux 5.10 `drm_file_free`, standard GEM handles are released
before the driver postclose callback. Guarding postclose therefore does not
protect those earlier standard GEM destructor calls. Other KMS/console/PM paths
also retain their own lifetime requirements. No reset, forced retirement, or
wholesale deferred unload has been substituted for that missing protocol.

The [standard GEM counterexample](private-gem-removal-counterexample.md) now
reproduces both an unjoined destructor started before unplug and a destructor
started after removal, using actual GEM/GTT-free/remove bodies. This supplies
an executable regression target for the missing destruction protocol, not a
physical safety pass.

## Evidence

All evidence is CPU-only; there was no device access or Mini 12 change.

- `/home/gama/sgx535-offline/private-postclose-20261029/v2-orphan-test/RESULT.json`:
  actual three orphaning fragments and actual memory-close body with reduced
  metadata and modeled mutex/backing/source APIs, native/UBSan. Checks matching
  and wrong identities, idempotency, preserved handles/views/cookies/references,
  no cleanup/backend callbacks, lost empty-owner close refusal and normal close.
- `/home/gama/sgx535-offline/private-postclose-20261029/v1/entry-test/RESULT.json`:
  actual entry guards with pthread DRM contract model, native/UBSan, compat on/off.
  Postclose/lastclose normal callbacks require entry; after unplug only orphaning
  is called. Existing ioctl/mmap/fault/remove-join regressions also pass.
- `/home/gama/sgx535-offline/private-postclose-20261029/v1/i486-tests/RESULT.json`:
  the orphaning and entry models statically compile for i486 and pass under QEMU.
- `/home/gama/sgx535-offline/private-postclose-20261029/v1/full-module/RESULT.json`:
  full i386 Kbuild/modpost/link passed in 97.524 seconds, one compiler at a time,
  768 MiB process address-space ceiling. Existing kernel/cache keys unchanged.
- `MODULE_VERIFICATION.json` in that full-module directory requires the guarded
  callbacks and all orphaning functions, checks previous source/entry symbols
  and six disabled BSS controls. ELF presence is not physical qualification.

Uninstalled module SHA-256:
`f2513a5c6f55e15dfa112484dc7f718cabbab9864ebec8c4bf6b19d06c4f05c7`.

The initial orphan test's lock model wrongly required the endpoint lock even for
standalone memory close. Its failed assertion, log and original test are retained
in `v1/orphan-test`; the model was corrected to cover both documented call forms.
No driver lock failure or hardware fault was observed in that attempt.
