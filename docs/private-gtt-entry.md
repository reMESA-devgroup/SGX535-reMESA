# Guard direct GTT operations during unplug

The private build now puts the public `psb_gtt_pin`, `psb_gtt_unpin` and
`psb_gtt_roll` operations inside DRM entry/exit intervals. Their native bodies
remain unchanged under private names. This covers direct callers outside the
standard GEM destructor, including display/cursor pin changes and console
rolling. The caller still owns the range/GEM lifetime; the guard protects
driver state and hardware access, not arbitrary or stale caller pointers.

After unplug, pin returns ENODEV and void unpin/roll return without entering
the native functions. They do not access device-private fields, GTT/MMU state,
registers or power APIs. Pin rejects a missing range/device. Roll additionally
requires the range's device to match its argument and `0 <= roll < npage`.
The old native roll checked only the upper bound and could consume a page
before the logical beginning of its page array for negative offsets.

A native operation admitted before loss remains inside its DRM reader until
completion, so unplug joins it. Nested admission is conservative: an outer
reader does not override unplug when a later inner GTT entry is denied. The
GEM destructor then observes that its mmap pin did not drop and retains the
object and its device/module ownership. No false unpin or release is inferred.

The live bodies preserve original pin counts, idle waits, MMU publication,
readback barriers and failure results. No hashes, frame readbacks, diagnostics,
fences or rendering fallback were added to the frame loop. Native checkpoint
and five normal nonprivate source profiles remain unchanged.

## Offline evidence

Root: `/home/gama/sgx535-offline/private-gtt-guard-20261029/v1/`.

- `gtt-test/RESULT.json`: actual pin/unpin/roll bodies and new guards, reduced
  metadata and modeled kernel/page/MMIO APIs, native/UBSan. Real pthread
  unplug joining. Checks live success/errors, native negative-roll canary
  witness and guarded rejection, wrong/null identities, postloss denial with
  private pointer NULL, and a later inner denial under an existing reader.
- `gem-test/RESULT.json`: actual GEM and GTT entry wrappers together, plus
  actual common/console constructors and original cleanup bodies. Native GTT
  work and other kernel APIs are modeled. Covers joined inner/outer readers,
  postloss retention and a new `nested-loss` case that denies the inner GTT
  operation without releasing the GEM allocation. Earlier failed idle,
  remaining-pin and invalid-pin cases remain covered.
- `i486-tests/RESULT.json`: seven static i486/QEMU fixtures complete, including
  GTT, combined GEM/GTT, earlier revocation/entry/unload controls and the
  remaining framebuffer counterexample. GEM also passes all four failure/denial
  modes. Compiler virtual ceiling 768 MiB; QEMU virtual ceiling 6 GiB for guest
  mapping reservation, CPU/time bounded.
- `full-module/RESULT.json`: full i386 Kbuild/modpost/link passed in 113.434
  seconds, one build job with 768 MiB process address-space ceilings. Kernel
  cache keys and observer symbols unchanged. Module uninstalled/unqualified.
- `full-module/MODULE_VERIFICATION.json`: read-only ELF verification of prior
  source/entry/revocation/GEM functions and six disabled BSS controls. These
  controls are not observations of live N/N state.
- `GTT_ELF_BINDINGS.json`: all three public GTT functions in this module contain
  relocations to DRM enter/exit; those in the preceding module do not. This is
  compiled binding evidence, not whole control-flow or hardware proof. Both
  binaries remain unchanged by inspection.
- `NORMAL_PROFILES.json` and `SOURCE_IDENTITY.json`: five normal profiles
  unchanged against `32fe4d1`, 31 protected files unchanged and 205 staged/build
  source inputs identical, checked again after Kbuild.

Uninstalled module SHA-256:
`0e4b37081123e5abdb9ee33c79af2240c725789a000f27431b93a77f8822a59a`.

All tests are CPU-only; no device, actual PTE or GPU submission was accessed.
The negative-roll witness uses an allocated sentinel slot before the logical
page span, avoiding a host memory violation while recording the wrong page.

## Remaining framebuffer blocker

Source review found a separate console mapping problem: `psbfb_mmap` does not
bound VMA length by the GEM allocation, and `psbfb_vm_fault` inserts PFNs for
the whole VMA. It also stores a framebuffer pointer while VMA open/close do not
hold an ownership reference. Existing fbdev teardown explicitly frees that
framebuffer/helper; a GTT entry guard does not repair those outer lifetimes.

The witness at
`/home/gama/sgx535-offline/private-gtt-guard-20261029/fb-mmap-witness/v2/RESULT.json`
extracts the actual current mmap/fault bodies. Native/UBSan and i486/QEMU runs
confirm that a one-page GEM accepts a two-page VMA and inserts a second modeled
PFN outside that object's backing. This is not a physical memory exposure
observation or a full dangling-pointer reproduction. The initial fixture omitted
`errno.h`; its failed compilation and original fixture are retained in the
parent directory. Only that model include was corrected for the successful run.

The next framebuffer change needs bounded mapping, stable VMA backing ownership
and a teardown protocol for open fb files and mappings. Other outer KMS/console
callbacks also remain to be joined/reviewed. This module is not yet physically
qualified; no installation, reset, boot change or new FPS result is implied.
