# Bound fbdev mappings and preserve open-file ownership

The private profile replaces the native console mmap/fault handlers with
GEM-owned, bounded mappings. Initial mmap requires an aligned nonempty VMA at
offset zero, within the console GEM allocation. The stolen-memory allocation
must be tracked, belong to this device, fit the firmware stolen region and the
target's 32-bit physical-address window. No mapping metadata or ownership is
published on validation failure.

A VMA now holds a GEM reference instead of a raw framebuffer pointer. Open and
close balance cloned/split/forked mappings. Fault checks the VMA span, object
page count, adjusted VMA offset and fault page identity, then inserts only the
requested PFN. Valid partial-unmap/split offsets are supported. Cache policy is
noncached as in the native mapping. Mmap and fault are guarded by DRM entry/exit;
postloss mmap returns ENODEV and fault returns SIGBUS without private-state/PFN
access. The GEM tracker preserves the object/device/module dependencies.

## Deferred fb_info and framebuffer destruction

Keeping GEM pages alone does not keep `fb_info`, its helper or a framebuffer
alive. The target fb core counts open fb files and invokes `fb_destroy` at its
final `put_fb_info`. All three private fb operation tables now supply that
callback. Detach cancels helper work and unregisters the fb interface once;
open-file references defer helper/info cleanup. Detach never dereferences the
helper after unregister, which can synchronously invoke final destruction.

At final fb_info destruction, the helper is unlinked from the device and
finished, then its framebuffer reference is released with
`drm_framebuffer_put`. The native forced cleanup/free is not used: a KMS plane
or client can still own a framebuffer reference. Its existing
`drm_gem_fb_destroy` callback releases GEM and framebuffer storage when the
last real framebuffer reference drops. Helper and info can therefore be freed
while a valid KMS framebuffer reference remains.

Retained device unload decides ownership before mode cleanup and uses fbdev
detach instead of destroying the KMS graph behind open files or retained GEM.
It still joins the existing poll/vblank/IRQ/chip/opregion cleanup calls. An
appended private flag makes unregister idempotent. After initialized modeset
state exists, error-unload also unplugs/joins CPU entry before teardown; an
earlier partial-load failure does not call modeset unregister on uninitialized
lists. No endpoint, GEM-list or MMU mutex is held across unplug joining.

The private fb operation tables wrap helper/palette/pan/copy/fill/image/sync/
debug/ioctl callbacks in DRM entry intervals. The legacy modeset and cursor
entry points are also guarded: the target fb-helper/SysRq restore route can
disable a cursor before calling set_config. Original live operation bodies,
return values and arguments remain intact.

## Initialization failure

On this exact target, `register_framebuffer` publishes its slot and initializes
the fb_info count before calling fbcon; a later fbcon error can therefore occur
after publication. The driver distinguishes that case from an unpublished
failure using the target count contract. Published failure unplugs/joins and
unregisters through the same final-file ownership path, returning without
accessing a helper that may have been destroyed. Unpublished failure releases
the helper's framebuffer reference and finishes helper/info immediately. Early
helper-init failure clears the device helper pointer after freeing it.

This use of count depends on the inspected antiX 5.10 target implementation and
must be reviewed for a kernel port. It is not a new stable UAPI or a claim that
all upstream kernel versions use the same registration sequence.

## Limits and evidence

This is a CPU lifetime/mapping correction, not physical driver qualification or
new SGX acceleration/FPS evidence. GPU stop/isolation and collection of retained
GEM/MMU state remain incomplete; the earlier reboot-only quarantine policy is
still in effect. Other display/PM/legacy entry paths need final review. Existing
boot settings, the installed Mini 12 stock driver and historical GPU evidence
remain unchanged. No per-frame diagnostic hash/readback/checkpoint or software
replacement for GPU rendering is introduced.

Evidence root: `/home/gama/sgx535-offline/private-fbdev-lifetime-20261029/`.

The fbdev fixture compiles the actual private mapping/ops/detach/config guards,
staged initialization/destruction bodies and original configuration body. It
compares against actual previous mmap/fault bodies. Metadata and fb/VFS/GEM/
PFN/kernel APIs are modeled; CPU entry joining uses real pthread scheduling.
It does not execute a kernel fb file or map physical memory. Cursor guard live
bodies are modeled separately; the real bodies compile in the full module.

`v7/fbdev-test/RESULT.json` covers old one-page backing/two-page VMA admission
and new rejection, one-PFN faults, region/32-bit boundaries, VMA split references,
postloss operation/config/cursor/fault denial, admitted callback joining,
last-file and synchronous fb_info destruction, extra KMS framebuffer references
and helper/prepublication/published registration errors.

Earlier artifacts are preserved: `v1/fbdev-test` failed compilation because a
fixture counter shadowed `puts` and the native destructor has an unused
parameter; `v3/fbdev-test` exposed a fixture API declaration mismatch with the
target helper-init signature and a missing log macro. The corrected models are
in `v2-fbdev-test`, `v4-fbdev-test`, and `v5/fbdev-test`. These are fixture build
issues, not kernel or hardware failures. Intermediate full modules in `v1`,
`v3` and `v5` remain uninstalled and unqualified and are not the final candidate.

Final evidence:

- `v7/i486-tests/RESULT.json`: seven static i486/QEMU fixtures pass, including
  fbdev ownership/bounds/error paths, combined GTT/GEM and earlier entry,
  revocation and unload controls. GEM also passes four failure/denial modes.
  Compiler ceiling 768 MiB virtual address space; QEMU ceiling 6 GiB for its
  guest mapping reservation, with CPU/time limits.
- `v7/full-module/RESULT.json`: full i386 Kbuild/modpost/link in 108.456 seconds,
  one build job, 768 MiB process address-space ceilings. Kernel cache keys and
  observer symbols unchanged; uninstalled and physically unqualified.
- `v7/full-module/MODULE_VERIFICATION.json`: read-only ELF checks require fbdev
  mapping/final-file callbacks, config/cursor entries and earlier guards. Six
  controls remain zero BSS booleans; this is not live N/N evidence.
- `v7/NORMAL_PROFILES.json`, `v7/SOURCE_IDENTITY.json`: five normal profiles
  unchanged against `e931a5b`; 210 staged/final build input files identical and
  31 protected files unchanged. `v7/VERIFIER_NEGATIVE.json` rejects the prior
  module when fbdev lifetime callbacks are required, without modifying it.

Uninstalled module SHA-256:
`4e7492a1a77496e87110da57cdea60a3f90ca6ba8d0ba52f8844ba537c538839`.

The final source also preserves framebuffer references held by KMS planes.
This supersedes intermediate modules that still used native forced FB freeing.
The target fb-helper legacy commit calls cursor-set before set-config; both
cursor operations and set-config are now guarded. No complete hardware/PM
lifetime certification is inferred from these CPU tests.
