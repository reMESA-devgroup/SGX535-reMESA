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

The subsequent [device-wide revocation](private-device-revocation.md) also
clears private ownership during PCI removal when userspace keeps its file open.
It runs after the DRM unplug/join and before original unload. It retains the
same resources in software; it does not fix the MMU/GTT destruction blocker.

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

## Follow-up: discard identities after bound postclose too

Native `remesa_stream_postclose` keeps the bridge owner and trusted `drm_file`
identity when an issued stream is quarantined or an idle stream's close fails.
The memory endpoint already clears its file identity on failed cleanup. Returning
to DRM file destruction with the other identities intact leaves stale metadata.
Successful trusted close also leaves an old file pointer after clearing `claimed`.

The private wrapper now calls software orphaning after native postclose, inside
the same DRM entry interval. It removes matching identities after either cleanup
outcome. Failed/issued streams retain backing, operation and reference ownership
in quarantine. An unclaimed, successfully closed trusted stream only loses its
file identity; it is neither poisoned nor quarantined. The denied-entry branch
still performs only software orphaning. No userspace ABI or frame-loop work changes.

Evidence root:
`/home/gama/sgx535-offline/private-bound-postclose-20261029/`.

- `v2/file-lost-test/RESULT.json`: native and UBSan compile actual native bridge
  postclose, orphan fragments and memory-close body. Backend close/quarantine,
  memory-postclose and backing APIs are modeled. Covers issued retention, failed
  idle close, successful close without poisoning, and prior loss regressions.
- `v1/entry-test/RESULT.json`: native/UBSan entry and pthread join models, with
  compat on/off, require orphaning after native cleanup and before entry exit.
- `v2/i486-tests/RESULT.json`: static i486/QEMU file-loss and both entry variants.
- `v2/SOURCE_IDENTITY.json`: staged/build source identity and protected files.
- `v2/NORMAL_PROFILES.json`: five non-private generated source maps unchanged
  against `e578290`; changed runtime fragments are private-only components.
- `v2/full-module/RESULT.json`: complete i386 Kbuild/modpost/link passed in
  92.857 seconds with one build job and a 768 MiB address-space limit per process;
  original kernel/cache inputs and symbols unchanged. Module remains uninstalled.
- `v2/full-module/MODULE_VERIFICATION.json`: read-only ELF/symbol/disabled-control
  checks passed, including prior GEM/fbdev/entry guards. This does not prove the
  postclose ordering; the source regression tests provide that narrower evidence.

Unqualified module SHA-256:
`f875dcbab3485bbc086d433b22b127df509eb21020b7e392dab21207029d58d7`.

These are CPU models, not physical retirement or kernel SRCU qualification.
Later [GEM retention](private-gem-lifetime.md) and
[fbdev ownership](private-fbdev-lifetime.md) address parts of the historical
removal blockers described above. Safe GPU stop/collection, remaining owner
callbacks and physical qualification are still incomplete. Reboot-only
quarantine recovery remains; this candidate is not an installed driver.
