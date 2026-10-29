# Join private CPU entries before PCI teardown

The private staged driver now wraps native/compat ioctls, mmap setup, the normal
GEM fault callback, and the private COLOR fault with `drm_dev_enter/exit`.
PCI remove calls `drm_dev_unplug` before the existing unload function. Entries
admitted before unplug finish before those guarded CPU accesses can overlap
unload; later ioctls/mmap return ENODEV and faults return SIGBUS.

The ordering uses the existing Linux DRM SRCU API, not a new mutex around the
endpoint. Remove does not hold endpoint or MMU locks while waiting. Native ioctl
protection begins before `psb_unlocked_ioctl` reads `dev_private`; COLOR fault
protection spans its lease lock and PTE insertion. Original return values and
cleanup of the entered critical sections are preserved. When CONFIG_COMPAT is
off the compat callback remains NULL, as in the retained driver.

The target Linux 5.10 implementation sets `dev->unplugged`, joins SRCU readers,
then unregisters the device. See the [DRM lifecycle documentation](https://www.kernel.org/doc/html/v5.10/gpu/drm-internals.html).
This only protects paths that actually enter the critical section.

## Incomplete lifecycle integration — no private hardware qualification

**This is the CPU entry/join phase, not a complete removal fix.** The existing
unload still lacks a policy for resources retained after an uncertain GPU job.
Joining a returned ioctl is not GPU retirement. Private roots/tables/pages must
not be reclaimed without a stop/isolation proof or a valid retained device-loss
lifetime. That remains unresolved and private physical tests remain blocked.

Postclose, late GEM object destruction, lastclose/KMS/console/PM callbacks and
device rebind are not made safe merely by these wrappers. In particular the
standard GEM destructor can touch GTT/MMU after the last user reference; it needs
its own device-loss cleanup semantics. Moving all unload work to final DRM
release still does not solve late MMIO/PM on a new binding. No such workaround,
synthetic retirement, diagnostic wait or speculative reset was introduced.

The wrappers apply only to new private stages. Existing non-private generated
profiles, checkpoint sources, installed modules and historical physical results
are not modified by this change. No new flags enable execution.

## CPU evidence

`tools/remesa/test_private_entry_guard.py` compiles the actual entry wrappers,
COLOR fault, and old/new PCI remove bodies. A pthread model implements the DRM
enter/exit/unplug contract and records backing operations. It does **not** test
the Linux SRCU implementation, hardware stop, GPU retirement or memory reclamation.

The old remove body overlaps an admitted blocked ioctl; the new body waits for
it and denies new native/compat ioctl, mmap and both fault paths after unplug.
Return values and reader counts are checked. Both CONFIG_COMPAT branches are
compiled in native/UBSan and i486/QEMU variants; that is branch coverage, not
execution of the kernel's compat ABI translator.

Artifacts:

- `/home/gama/sgx535-offline/private-entry-guard-20261029/v2-controller-test/RESULT.json`
- `/home/gama/sgx535-offline/private-entry-guard-20261029/v1/i486-tests/RESULT.json`
- `/home/gama/sgx535-offline/private-entry-guard-20261029/v1/full-module/RESULT.json`

Full i386 Kbuild/modpost/link passed in 98.167 seconds with one compiler and a
768 MiB process address-space ceiling. Module SHA-256 is
`eb0df99aa7e2346207101ba5cfb55a401838d5ecb4a9cc33df02db0e3d73f24a`.
Read-only ELF verification requires the guarded callbacks and DRM lifecycle
imports, plus the prior serialized-source function, and checks six disabled BSS
controls. These are compile/link properties, not live N/N observations. The module
remains uninstalled and physically unqualified. `NORMAL_PROFILES.json` in the v1
artifact directory checks unchanged generated code for five non-private profiles.

The initial v1 controller build failed because its fixture named a counter `puts`
and its function extractor matched `color_fault()` in a comment. The original
logs/sources remain preserved; both test-only mistakes were corrected. The
extractor now locates function definitions rather than the first textual use.
No physical driver fault was observed in that failed build.
