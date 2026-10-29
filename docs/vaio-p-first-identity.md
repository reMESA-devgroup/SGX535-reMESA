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
