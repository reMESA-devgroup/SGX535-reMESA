# Kernel portability: one versioned userspace protocol, reviewed kernel backends

Other Debian-based distributions, including the volunteer's Q4OS installation,
are in scope. The volunteer has not yet run the collector; exact Q4OS release,
kernel, architecture and Vaio model remain unknown. Distribution branding is
not a module compatibility check, and no tester is being asked to downgrade
to the Mini 12's kernel.

The [official Q4OS release history](https://www.q4os.org/blog.html) identifies
Aquarius with Debian Bookworm/6.1 kernels and Andromeda with Debian Trixie.
It also distinguishes their 32-bit availability. That motivates reviewing both
6.1 and 6.12 families; it does not identify the volunteer's installed kernel.

## Verified wire-format groundwork

`tools/remesa/check_uapi_portability.py` compiles the existing reMESA envelope
headers against supplied original DRM UAPI headers and records sizes, ioctl
numbers, versions and explicitly listed member offsets. It never opens a device
or issues an ioctl. It can compare a new build with a previously recorded layout:

```sh
python3 tools/remesa/check_uapi_portability.py \
  --drm-headers /path/to/kernel/include/uapi/drm \
  --output /new/baseline-directory

python3 tools/remesa/check_uapi_portability.py \
  --drm-headers /path/to/other/kernel/include/uapi/drm \
  --reference /new/baseline-directory/RESULT.json \
  --output /new/comparison-directory
```

`--cc`, `--runner` and `--static-i486` allow the same probe to compile and run
under the existing i486 cross compiler/QEMU environment. Cross builds use the
compiler's Linux types/ioctl headers plus copies of the selected DRM headers.
This is a userspace ABI probe, not a full kernel-header or module compilation.
The probe erases the raw `__user` annotation, as headers_install does for
userspace, while preserving the input files byte for byte.

Evidence root: `/home/gama/sgx535-offline/kernel-portability-20261029/`.

`v8/MATRIX.json` records the baseline and five comparisons: antiX 5.10.240,
Debian 6.1.176-1 and Debian 6.12.107-1 DRM headers, each on native 64-bit host
and static i486/32-bit userspace. Eleven selected envelopes match on sizes,
ioctl values, versions and recorded offsets. C object alignment may differ
between architectures; it is recorded separately from the compared wire data.
The shader/readback envelopes have no assigned ioctl and are recorded with zero,
which does not install an endpoint. No protocol or existing ABI header changed.

`v8/NEGATIVE.json` records rejection of an incompatible reference size.
`v7/antix-native` preserves the initial compile rejection from unexported raw
`__user` annotations and the original probe/runner. This was a header-export
fixture issue; no runtime driver or wire-format correction was made.

These experimental versioned interfaces are not being declared permanent UAPI.
Matching selected layout data does not prove all nested member semantics,
compat-ioctl dispatch, endpoint registration, memory isolation or acceleration.

## Native source contracts that need adapters

The selected official Debian source inputs are preserved with URLs, suites and
SHA-256 inventories in `v3/debian-response`, `v4/FETCH.json`, `v5/DIRECTORIES.json`
and `v6/FETCH.json`. They are selected source files, not complete prepared kernel
trees. The exact 6.x package versions are development review inputs, not the
volunteer's assumed release or a promise of binary compatibility.

| Contract | Pinned 5.10 integration | Reviewed 6.1 / 6.12 sources | Required port work |
|---|---|---|---|
| Private state | `dev->dev_private`, separate allocation | Embedded `struct drm_device`, `to_drm_psb_private` | Typed ownership/accessor adapter; preserve device/module references |
| PCI access | `dev->pdev` | `to_pci_dev(dev->dev)` | Reviewed device-parent conversion, not layout casting |
| GEM allocation/destruction | `gtt_range.gem`, GTT pin/unpin hooks | `psb_gem_object.base`, object free/vm funcs, dma_resv lock | Adapt constructors, pinning, failure retention and final-kref handling |
| Release scheduling | Explicit PCI-remove/unload path | Managed `psb_device_release` action invokes unload | Join/revoke before every managed cleanup; retain MMU/backing graph correctly |
| Power | Retained manual count and private transition correction | Runtime-PM reference helpers with an unconditional lifetime reference | Review native PM semantics; do not copy 5.10 private fields blindly |
| Console | Legacy fbdev lifecycle and lid timer | 6.12 has fbdev.c/setup changes and no psb_lid.c | Follow native fb ownership/work lifecycle for each backend |

In both reviewed modern sources, `psb_gem_object` starts with the GEM base and
uses a separate GTT resource. The old `gtt_range` container hook cannot simply
be reused. Likewise, managed unload cannot be assumed deferred until all GEM
references vanish. These are semantic lifetime adaptations, not version-string
substitutions or reasons to weaken the existing source pins and safety gates.

## Qualification status

| Target | Evidence so far | Port qualification |
|---|---|---|
| antiX 5.10.240 i486 | Existing native physical 3D/2D evidence; newer private candidate CPU tests/build only | Private candidate still physically unqualified |
| Debian 6.1.176-1 | Selected source review and wire probe on native64/i48632 | Module port/build and physical validation pending |
| Debian 6.12.107-1 | Selected source review and wire probe on native64/i48632 | Module port/build and physical validation pending |
| Other kernels / 7.x | No new build or source qualification here | Pending review of the actual target |

The next backend implementation needs the native GEM/device/release adapters
and matching prepared headers/build environment before a module build can be
claimed. Existing shader/PDS/geometry assets and physical oracles remain reused.
Each resulting backend still needs the required safety, retirement, readback and
recovery qualification. Full desktop OpenGL/GLES/2D, installation and eventual
video acceleration remain the goal; wire checks alone do not satisfy it.

Earlier public-source fetch failures are preserved in v1/v2: GitHub raw requests
were rate-limited and kernel.org access failed. Official Debian source/API
downloads succeeded afterward. No downloaded file was executed or installed.
