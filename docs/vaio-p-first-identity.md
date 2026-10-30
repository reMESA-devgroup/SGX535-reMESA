# Vaio P: initial platform identification

This package collects platform information to prepare future reMESA testing.
It does not install drivers, change boot settings, load or unload modules, or
submit work to the GPU. The name "Vaio P" alone does not establish the same PCI
device, SGX revision, MMU, display panel, or kernel as the tested Dell Mini 12.

On Linux with Python 3, extract the package and run without sudo:

```sh
python3 collect_platform_identity.py --output vaio-p-identity.json
```

From a repository checkout, use:

```sh
python3 tools/remesa/collect_platform_identity.py --output vaio-p-identity.json
```

The collector reads distribution, kernel, CPU, RAM, model and BIOS information
(excluding serial numbers), display PCI devices, bound drivers, and DRM card
names. It does not open `/dev/dri`, PCI `resource*` files, or registers. It does
not collect hostnames, UUIDs, network addresses, or complete logs. The only file
created is the requested JSON; existing files are not overwritten. Unavailable
fields are `null`, and inaccessible PCI/DRM directories are marked unreadable.

Share the JSON for comparison. PCI ID `8086:8108` alone does not establish
SGX535 rev121: the PCI revision is different from the internal SGX revision.
Missing module parameters do not establish disabled N/N controls. This inventory
does not qualify GPU acceleration.

Keep the current system and boot configuration. The private driver candidate
is not physically qualified for this Vaio. This package contains no kernel
module and performs no rendering test. A kernel different from the current
antiX 5.10.240 target needs its own build and review before installation.

The volunteer has reported Q4OS, but its exact release, installed kernel and
model have not been collected yet. Q4OS is not treated as requiring the antiX
kernel. Work on other native kernel backends and the shared userspace wire
contract is tracked in [kernel portability](kernel-portability.md); those ports
are not yet qualified for installation.

## Required drawable size

The requested Vaio P drawable is **1600x768**. Keep it in the validation matrix
alongside 1280x800 and 640x640, with 641x479 for padding/rounding coverage.
`mesa/sgx535/tests/test_drawable_resolutions.c` checks the existing storage,
COLOR/Z descriptors, scene layout and background-geometry encoders at all four
sizes. Its expected Vaio words are arithmetic contract checks, not new captures
from the original proprietary producer or physical GPU evidence.

For linear BGRA8/Z24X8 at 1600x768, pitch is 1600 pixels (6400 bytes), and each
plane is 4,915,200 bytes / 1200 pages. Independent front, back and Z planes total
14,745,600 bytes. Scene storage requires 125,728 bytes, rounded to 31 pages.
CPU-native, UBSan and emulated i486 results are preserved under
`/home/gama/sgx535-offline/vaio-resolution-20261030/v1/`.

This does not enable physical 1600x768 rendering. The current retained gear
runtime, viewport/clear validation, kernel fixed-profile dimensions and legacy
COLOR mmap still contain 1280x800 contracts. They must be replaced by a bounded
variable-size rendering path and separately qualified. Do not relax the old
profile's dimensions to claim support, and do not install the Mini 12's kernel
module on an unidentified Q4OS kernel.
