# Retain a lost private endpoint's device dependency graph

Private removal now preserves the remaining SGX/MMU/GTT device state when its
memory endpoint has been marked lost. The endpoint already owns device,
module and power references from successful `remesa_gem_memory_open`; its
monotonic loss flag prevents later close from dropping those references. This
change adds no new allocations, reference acquisition or per-frame work.

The original unload still invalidates source history, disables KMS polling and
vblank, uninstalls IRQ, removes backlight and mode clients, tears down chip CPU
helpers and finishes the ACPI opregion notifier/work. After those existing
calls, a matching lost endpoint takes the retained branch. Runtime PM is
disabled through the existing `gma_power_uninit`, and unload returns before:

- freeing fault/default/private MMU directory dependencies;
- removing the default directory's stolen mappings or destroying its driver;
- destroying GTT mappings, scratch backing or VDC/SGX/aux register mappings;
- dropping auxiliary PCI references, BIOS data or the main private allocation.

The CPU cleanup calls above are not a GPU stop, retirement or isolation proof.
This is conservative failure quarantine; retained allocations and references
remain until reboot. There is no retry, reset, forced ACK, fake completion,
cleanup timer or background recovery. Repeated operation on this lost endpoint
is not a supported render path.

The current backend has one global memory endpoint. After loss, every new
PCI binding attempt in the private build is refused with EBUSY **before** `pci_enable_device`, DRM device
construction, register access or a new MMU. This also prevents overwriting the
old global backend state by rebinding the retained module. A normally closed
endpoint is removed from that global slot; it does not block a new binding.
Normal nonprivate generated profiles are unchanged.

## Remaining safety work

This component is not a qualified installable driver. Standard GEM destruction
still needs its own CPU join and lost-object protocol. In particular, keeping
register mappings alive does not authorize late MMIO after physical PCI loss.
Removal without a retained private endpoint still follows original teardown.
The separate GEM counterexample models that nonretained case and still
reproduces both unsafe destructor schedules. Do not generalize this component
to all unplug, PM, console or GEM lifetime paths.

The retained private job uses separately allocated memory endpoint backing and
its confined private MMU root. Standard display-GTT backing is excluded from
that private VA allocator. The mode/client cleanup calls must still be checked
together with the final GEM protocol, not inferred safe from the unload test.
There is no newly approved physical execution or new FPS measurement.

## Offline evidence

Root: `/home/gama/sgx535-offline/private-device-retention-20261029/v1/`.

- `unload-test/RESULT.json`: actual staged `psb_driver_unload` and
  `psb_pci_probe`, native/UBSan. Reduced structures and modeled endpoint,
  allocation, teardown and hardware APIs. Matching lost endpoint preserves all
  modeled SGX/MMU/GTT dependency fields, executes existing CPU cleanup calls,
  and blocked probe performs no PCI enable/allocation/load. Normal probe and
  nonretained unload controls remain operational.
- `orphan-test/RESULT.json`: actual file/device helper fragments and memory
  close with modeled locks/resources. Tests matching/wrong/null identities,
  loss/rebind queries, retained references and late-close refusal.
- `entry-test/RESULT.json`: actual guards/remove with pthread DRM model,
  compat on/off, native/UBSan. CPU entry join/revocation ordering is unchanged.
- `gem-counterexample/RESULT.json`: actual GEM/GTT-free/remove bodies with a
  modeled nonretained unload. Both destructor counterexamples still reproduce;
  `driver_safety_pass=false`.
- `i486-tests/RESULT.json`: five static i486/QEMU fixtures complete, including
  actual unload/probe controls. CPU-only; compiler 768 MiB virtual ceiling,
  QEMU 6 GiB virtual guest reservation ceiling and CPU/time limit.
- `full-module/RESULT.json`: full i386 Kbuild/modpost/link passed in 124.321
  seconds with one build job and 768 MiB process address-space ceilings. Kernel
  cache keys and observer symbols unchanged; no installation.
- `full-module/MODULE_VERIFICATION.json`: final read-only ELF check includes
  both retention/rebind queries and previous source/entry/loss guards. Six
  controls are disabled BSS booleans, not an observation of live N/N.
- `VERIFIER_NEGATIVE.json`: preceding module rejected when retention queries
  are required, with its binary unchanged.
- `NORMAL_PROFILES.json` and `SOURCE_IDENTITY.json`: five normal generated
  profiles unchanged against `7191072`; 202 staged/build input sources identical
  and 31 protected files unchanged. Final sources also compared after Kbuild.
- `SOURCE_LIFETIMES.json`: hashes and bounded observations from the actual
  target kernel/driver sources, including parent-device references, runtime PM
  barrier, explicit resource allocation and IRQ/work teardown APIs. Source
  inspection does not establish physical DMA stop or complete unplug safety.

Uninstalled/unqualified module SHA-256:
`a756ed0a99d90f36e60d8af6168e508a9c3b8468319782919023d1e2f1e7ccaf`.

The pinned native checkpoint and earlier hardware evidence remain unchanged.

The subsequent [standard GEM lifetime component](private-gem-lifetime.md)
extends graph retention/rebind refusal to tracked standard GEM objects, joins
preloss destruction and quarantines postloss destruction without late MMIO.
Physical qualification and a recoverable collection protocol remain incomplete.
