# Exclude legacy submission while a private session owns the endpoint

The private build now refuses both the one-shot private probe and the retained
laboratory ioctl when `private_session` owns the same memory endpoint. Each check
runs under `endpoint_lock`, which also serializes acquisition/release of that
lease. A denied call returns `-EBUSY` without changing the request envelope.

The probe check precedes power/register identity reads, allocation, job
preparation, input hashing, buffer initialization and source claim. Previously,
its idle/executor/stream checks did not recognize the separate private lease;
an idle owner could reach setup before the later source claim refused it.
The retained-interface check precedes initialization, depth capture and
submission. A prepared source/backing session cannot be consumed or mutated by
these older interfaces in the gap between its own jobs.

The old conditions remain intact. With no private lease, both function bodies
are otherwise unchanged. No new ioctl, flag, parameter or GPU command is added.
The original one-shot consumed/source no-rearm contracts remain monotonic.

## Evidence

Root: `/home/gama/sgx535-offline/private-admission-20261030/v1/`.

- `probe-ubsan/`: all 36 original controller scenarios and one new leased-endpoint
  denial pass. The new case checks zero register reads, allocations, job
  preparation, hashing, masking, source claims or execution; the request is
  byte-identical and the one-shot consumed flag stays false. Dependencies are
  modeled, including register reads; this is not GPU evidence.
- `retained-final/`: the actual retained ioctl is extracted from both parent and
  candidate. An old-code witness reaches modeled init/capture/submit with a
  private owner. The candidate rejects it before all three callbacks. Nine
  cases cover the normal positive flow, private lease, existing stream, wrong
  file/device/context, disabled admission, capability and reserved-field errors.
  Candidate tests pass natively, with UBSan and as a static emulated i486 binary.
  Request fields/lengths use the exact frozen 11-role profile header from the
  physical module build; only unused DRM ioctl-number macros are shimmed.
- `retained/`: preserved initial harness compile failure: witness-only build
  exposed two unused fixture variables under `-Werror`. The fixture declarations
  were scoped to the candidate build; warning flags and driver logic were not
  relaxed. Successful logs are separate in `retained-final/`.
- `SOURCE_REVIEW.json`: removing only the added lease checks reproduces the two
  parent functions exactly. Five non-private source profiles, 31 protected
  files, the cold executor, loan, source protocol, backend, profile, probe UAPI
  and ioctl table remain unchanged.
- `full-module/` and `MODULE_REVIEW.json`: complete 5.10 build and read-only review;
  the candidate is not installed or physically qualified by these CPU checks.

The full module built in 138.112024 s, SHA-256
`0347ba23843bd43ce5c85d74c4f5f81791ee5376f7e2f859811dab5f1877ac73`. The read-only verifier passes the existing
kernel-release, MMIO, entry, file/device retention, GEM, fbdev and lid guards.
Staged and built admission sources match exactly. No candidate was installed.

This closes the identified mixed-admission gap for these two interfaces. The
kernel-internal source session still needs authenticated userspace submission,
input sealing/update integration, and physical multi-job qualification before
continuous rendering or performance can be claimed. It does not qualify general
OpenGL/GLES, desktop 2D, 1600x768 physical output, S3 or other kernels.
