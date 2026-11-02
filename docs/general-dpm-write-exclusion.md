# DPM pool write exclusion for a general source session

`remesa_private_session_seal_dpm` carries the restricted FAST pool write
exclusion into the existing general backing/source session. It uses explicit
table and parameter IDs, including generations, rather than retained roles7/8
or fixed GPU addresses. It is kernel-only and adds no ioctl or runtime gate.

The trusted submitter first prepares an admission job and uses the existing
`remesa_gem_job_claim_zero_dpm`. Before source acquisition, it seals both pools
under those actual job holds, cancels the unissued admission job and starts
the existing source session. Subsequent general jobs must name those same two
generations and claim the same DPM binding mask. The general execution adapter
checks this metadata before entering the executor. The retained eleven-role
executor and input seals refuse the general pool seal.

Both pools must validate before either ID is recorded: complete pinned
32MiB backing, correct SURFACE/RASTER placement and alignment, single exclusive
job hold, no CPU alias/lease/VMA or conflicting seal, and authentic fresh-zero
provenance for the initial seal. Mixed retained input/work seals, residual IDs,
wrong owner/ticket, file/device loss and poisoned source state are refused.
No pages are scanned, cleared, mapped, flushed or hashed by the seal/check.

For the backing lease's lifetime, the ordinary memory request path denies
WRITE, writable LEASE and explicit user SYNC on either exact pool generation.
Read-only leases retain ordinary job exclusion. Other objects retain their
normal upload rules. The topology lease continues to prevent unmap/unpin or
replacement of the pool backing. Release checks complete idle metadata before
clearing either ID; a partial/corrupt seal is retained on failure. Source loss,
outstanding jobs/leases and quarantine keep their existing refusal/retention.

## What this changes and what remains

The old topology lease alone still allowed an idle writable pool lease. That
was incompatible with importing the donor FAST cache/reuse policy. This new
exclusion closes that gap without reinterpreting the old eleven-role ABI.

Pool ownership is separate from initialization and CPU/GPU residency. The
general executor still requires its cold DPM claim and conservative cache
maintenance. Publication still consumes `zero_pristine`; the seal does not
restore it. The tests explicitly demonstrate that ownership validation can
remain valid while a later cold claim returns `-ENODATA`. No warm capability,
new physical admission, per-frame clear or cache exemption is created here.

The next component is the donor's separate CPU-cached/CPU-dirty/GPU-dirty
tracking and a retirement-bound warm claim under this exclusion. Actual CPU
reads must regain coherent ownership; source/file/device loss or ambiguous
execution must prevent reuse. REGION initialization remains separate. See
[the donor comparison](general-dpm-reuse-contract.md).

## Verification

Evidence root: `/home/gama/sgx535-offline/general-dpm-seal-20261102/v1`.

- `tests-loss-final/RESULT.json`: actual memory, cold-claim, seal, request and
  release code, with injected GEM/MMU/PM primitives. Native/UBSan/statici486
  pass330 checks including the existing169 input/work checks. The full two
  pools are allocated by the model. Faults on either pool never publish a
  partial seal. File/device loss retains both IDs and refuses execution/release.
  Extra pool clears and seal-side map/flush work remain zero.
- `tests-backing`, `tests-source`, `tests-scene`: existing session helpers
  pass356,674 and745 checks respectively per variant; these model the
  execution/source callbacks, not GPU completion.
- `negative-old-write-boundary/RESULT.json`: removing only the new guard
  from an isolated generated CPU model grants the writable pool lease and
  triggers the expected assertion. Production source and LAB gates stay intact.
- `kbuild/RESULT.json`: full antiX5.10.240 i486 module builds without new
  warning messages; all three session symbols are defined. The candidate is
  `module/gma500_gfx.ko`, SHA256
  `200880fb99914775ace14fcab5c8f88a3734d2f48147944111a7d67db940d287`.
  It has not been installed or physically qualified.

The input/seal runner now applies the same capacity and zero-origin transforms
as the current module. Its first attempt omitted the preceding capacity
transform and refused an anchor; a test-only assignment then exposed that the
shim's VMA counter is atomic. Both failures are recorded. The negative
harness also retained a main-renaming compile error and a verifier mismatch
between literal `EPERM` and its preprocessed value; verification used the
already-completed negative run and did not repeat execution.

Reproduce without opening a device:

```sh
python3 tools/remesa/test_private_inputs.py \
  --native RETAINED_NATIVE_DIRECTORY --shim INDEXED_TEST_GEM_SHIM \
  --output NEW_DIRECTORY --dpm-session
```

Pair `--i486-cc` and `--qemu-i386` for the i486 model. The runner explicitly
excludes the unrelated scene-executor fragment; the complete module build
checks production integration. These results establish no new GPU operations,
FPS, warm physical reuse, full OpenGL/GLES, desktop2D or driver installation.
