# Standard GEM destruction races private PCI removal

The current private module at commit `358ef3e` still has an unguarded standard
GEM destructor. A deterministic CPU counterexample now exercises the actual
`psb_gem_free_object`, `psb_gtt_free_range` and `psb_pci_remove` bodies from its
retained full-module source tree. It covers an mmap-pinned GTT range, whose
destruction calls `psb_gtt_unpin`. The actual unpin implementation accesses
`dev->dev_private`, GTT state, blitter registers and the default MMU directory.

Two schedules reach the modeled unpin API after modeled hardware teardown:

1. GEM destruction starts before unplug and pauses in object release. PCI
   removal completes because that destructor holds no DRM entry reader. Resuming
   destruction reaches GTT unpin after unload.
2. A surviving object is destroyed after PCI removal completes. Destruction
   reaches the same unpin path without an entry guard.

The control case releases an object synchronously during unregister. In that
case the actual remove body calls unload afterwards, and no late access is
recorded. This distinguishes unregister ordering from the two unsafe schedules.

The fixture uses real pthread scheduling, but DRM unplug, object release,
unpin, resource release and unload are modeled APIs. It records an invalid
ordering rather than dereferencing freed storage. This is not a physical
use-after-free observation or a test of kernel SRCU. It does not claim every GEM
object requires MMIO: the witness deliberately uses an mmap-pinned range.

Evidence:

- `/home/gama/sgx535-offline/private-gem-removal-20261029/v2/counterexample/RESULT.json`:
  native and UBSan, both counterexamples and the ordering control reproduced.
- `/home/gama/sgx535-offline/private-gem-removal-20261029/v3-i486/RESULT.json`:
  QEMU execution of the static i486 counterexample built in `v2/i486`.
- `v1/counterexample` in the same evidence tree retains the earlier narrower
  fixture that modeled the whole GTT-free API. Version 2 extracts the actual
  GTT-free body and makes the mmap-pinned precondition explicit.

The initial QEMU invocation inherited the compiler's 768 MiB virtual address
limit and could not reserve its guest address space. That pre-execution failure
is retained in `v2/i486/1.log`. The successful invocation uses a 6 GiB virtual
address ceiling for QEMU's 4 GiB guest mapping reservation, with a CPU/time limit;
this is not a 6 GiB physical RAM allocation. Compiler limits remained unchanged.

Reproduce from the worktree:

```sh
python3 tools/remesa/test_private_gem_remove_race.py \
  --module /home/gama/sgx535-offline/private-postclose-20261029/v1/full-module/module \
  --output /absolute/path/to/a/new/evidence-directory
```

Successful execution reports `driver_safety_pass=false`: it confirms a blocker,
not driver qualification. No module or production frame-loop code was changed.

The next implementation must join destructors admitted before loss and retain
the complete dependency graph for objects destroyed after loss, without MMIO
or access to freed device state. Merely skipping unpin, retaining only GEM
pages, or adding an entry guard with no lost-object lifetime protocol is
insufficient. That work must fit the device-wide MMU/GTT retention, hardware
stop/isolation, callback teardown and rebind policy described in
[the postclose follow-up](private-postclose.md). The private hardware candidate
remains unqualified; previously validated hardware results remain unchanged.
