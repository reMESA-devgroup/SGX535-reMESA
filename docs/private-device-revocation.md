# Revoke private ownership at device loss, before unload

The private PCI remove path now invokes device-wide software revocation after
`drm_dev_unplug` has joined guarded CPU entry and before `psb_driver_unload`.
This closes a gap in file-only orphaning: a process can keep its DRM file open
through PCI removal, so postclose need not occur before unload.

The new entry does not require a caller file identity. Each component checks
its own device identity. The bridge clears its matching owner and quarantines
an active stream; the trusted component clears its file identity, quarantines
a claimed stream and poisons source authority; the memory component clears its
file identity, sets the monotonic device-loss flag, and quarantines live jobs
and backing. Existing pages, mappings, jobs, cookies, commands, borrowed views
and device/module/power references remain retained. A normally closed trusted
stream has stale file metadata cleared without poisoning its released source.

Null and unrelated devices are ignored. Repeated device revocation, including
a later file postclose, does not repeat trusted source poisoning. The original
file-only interfaces retain their behavior, including ignoring null files.
The bridge lock is released before endpoint-lock then memory-lock acquisition.
No backend callback, unmap, unpin, page release, MMIO or power cleanup is added.
No work is added to the production frame loop or to normal nonprivate profiles.

This is **software revocation, not safe device teardown**. In particular,
`drm_dev_unplug` includes unregister, so this new callback does not run before
unregister's client destruction. It does not fix standard GEM destructor races,
stop SGX DMA, preserve MMU/GTT through the subsequent original unload, or
authorize rebinding. The [GEM counterexample](private-gem-removal-counterexample.md)
still reproduces both unsafe schedules after this change. Retention of the
complete device dependency graph and a stop/isolation or quarantine protocol
remain required before qualifying a private physical driver.

Evidence is CPU-only under
`/home/gama/sgx535-offline/private-device-lost-20261029/v1/`:

- `orphan-test/RESULT.json`: actual helper fragments plus actual memory close,
  reduced metadata and modeled mutex/backing/source APIs, native/UBSan. Covers
  open-file device loss, absent bridge ownership, matching/wrong/null devices,
  repeated loss, closed-stream metadata, retained resources and close refusal.
- `entry-test/RESULT.json`: actual remove and guards, modeled DRM SRCU contract
  with real pthread joining, native/UBSan and compat both ways. Device revocation
  is reached only after readers join and before unload.
- `gem-counterexample/RESULT.json`: actual GEM/GTT-free/remove bodies continue
  to reproduce the unresolved destructor ordering failures. This is an expected
  counterexample with `driver_safety_pass=false`, not qualification.
- `NORMAL_PROFILES.json`: five normal generated-source profiles unchanged
  relative to `62f6b32`.
- `i486-tests/RESULT.json`: static i486 builds and QEMU runs of revocation,
  entry/join with compat on/off, and the remaining GEM counterexample. Compiler
  address-space ceiling 768 MiB; QEMU receives a 6 GiB virtual address ceiling
  for its guest mapping reservation, with a CPU/time limit.
- `full-module/RESULT.json`: full i386 Kbuild/modpost/link in 119.246 seconds,
  one build job, 768 MiB per-process address-space ceiling; kernel cache keys
  and observer symbols unchanged. Module remains uninstalled and unqualified.
- `full-module/MODULE_VERIFICATION.json`: read-only ELF verification requires
  all three device-revocation symbols plus existing entry/source/file guards.
  Six controls remain zero-initialized BSS booleans; this is not live N/N proof.
- `VERIFIER_NEGATIVE.json`: the preceding module is rejected when device
  revocation is required; the binary is unchanged by verification.
- `SOURCE_IDENTITY.json`: 202 staged/build source files are identical and all
  31 protected files remain unchanged.

Uninstalled module SHA-256:
`85be4a590c969de91bffdff4a74d8c0ae62fb60530b76b9c2e695748452bb999`.

There was no device access, installation, reset or boot change. Existing
physical evidence and the previously validated glxgears path are unchanged.

The subsequent [lost-endpoint retention component](private-device-retention.md)
now skips destructive SGX/MMU/GTT teardown for a matching retained endpoint and
blocks a new probe. It does not resolve the remaining standard GEM protocol.
