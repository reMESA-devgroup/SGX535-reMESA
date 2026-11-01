# Bound Gallium native scene LAB client

`tools/remesa/gallium_scene_native_client.c` uses the actual Gallium/NIR
frontend, typed persistent GEM attachments, bound raw vertex inputs and
`remesa_context_submit_linked_scene`. It borrows an already-open resource fd
from a supervisor. It never opens a device node, enables a module parameter,
installs a module, reboots, presents a CPU frame or retries an uncertain job.

The old `scene_native_client.c` and its physically validated evidence are
unchanged. This new client is a separate LAB collector, not a FAST driver hook.

## Collection lifetime

The linked builder drops temporary public owner references. Successful scene
submission consumes its capture after retirement, so collecting those GEM
buffers after the client returns is too late.

The new client instead wraps the native scene callback. It records the actual
packet, placement, complete provider inventory and initial READ/CPU-only/guard
images before submission. After the existing Linux adapter authenticates
completion, publication and retirement, the wrapper reads COLOR/Z and checks
the remaining images while the frozen graph still retains all temporary owners.
Only then does it return the real receipt to the normal provider lifecycle.
On an invalid receipt it skips these post-completion reads and retains the
existing quarantine behavior.

There are 20 scene/frontend allocations: 17 GPU bindings, the CPU-only retained
body, the CPU-only DPM plan and the bound source VB. Four A5 guard allocations
bring the pre-submission inventory to 24. After successful capture consumption,
seven objects remain: COLOR, Z, the source VB and four guards. The supervisor
receives their exact IDs in `INVENTORY-REMAINING.bin`. Those parent-owned
resources still require explicit cleanup; child exit is not cleanup evidence.
An uncertain execution returns without destructive cleanup or a remaining-ID
success record.

`remesa_linux_memory_inventory_snapshot` exports host-owned IDs, sizes, VAs,
leases and poison flags, including CPU-only owners. It performs no ioctl or
allocation and changes no ownership. It is not a pin/PTE or GPU visibility
proof: the client separately checks the existing kernel INFO results. Snapshot
outputs cannot overlap provider metadata or acquired CPU map storage. This
diagnostic API adds no work to normal rendering.

## Invocation and current execution restriction

```
gallium_scene_native_client FD 1280x800 OUTDIR FIXTUREDIR --prepare-only
gallium_scene_native_client FD 640x640 OUTDIR FIXTUREDIR --execute
```

The output directory must already exist and be empty; fixture metadata must
match the exact graph. `--prepare-only` does not enter a scene ioctl. It still
uses the resource endpoint and creates real GEM objects when run on hardware.
Neither mode enables that endpoint.

**Do not run the execute form on the existing six-profile kernel.** Its current
Linux adapter selects profiles 5/6, whose exact address/image gates reject this
relocated Gallium graph. Before a physical execution, add independently
qualified profiles 7/8 and an explicit matching adapter selection, preserve
profiles 1–6, and run the established closure/controller/build tests. Do not
replace old profiles or bypass their image/address validation. The client and
inventory work here make no kernel gate change.

The supervising collector must validate the saved real readbacks, source/trace,
full retirement, guards and remaining-object cleanup, then recover stock/N/N.
The existing collector expects the old post-child inventory and cannot be used
unchanged for this new lifetime. Its pixel oracles and recovery mechanisms
should be reused.

## Evidence

Root: `/home/gama/sgx535-offline/gallium-native-client-20261101/v1`.

* `transport-inventory/RESULT.json`: inventory, existing scene transport and
  existing page-upload suites, native/UBSan/i486, with warnings as errors.
* `native-client-final/RESULT.json`: all current driver objects and real Mesa
  Gallium/NIR libraries; two sizes times prepare/execute/bad-receipt cases.
  The execution cases use an explicitly separate syscall model that never
  renders pixels. An unwrapped actual binary is built separately.
* `CLIENT_VERIFICATION_FINAL.json`: 12 exact READ inputs, three non-GPU/source
  owners, four unchanged A5 guards, 24-object collection and the exact seven
  remaining IDs. The model asserts that post-retirement reads precede the first
  temporary-owner teardown. Invalid receipts have no post-readback or success
  inventory. These are **offline transport/lifetime results, not SGX rendering**.
* `MINI12_LOADER.json`: the unwrapped binary was transferred with SHA-256
  verification, and `/lib/ld-linux.so.2 --list` resolved libm, libatomic and libc
  on the Mini 12 stock boot `589351ee-f347-4e5e-8f0e-839a988b07e8`. Client main,
  GEM, scene submission, MMIO and GPU execution were not invoked. This proves
  dependency compatibility only; it is not physical rendering qualification.

OpenGL/GLES capability admission, continuous execution, desktop acceleration
and installation remain unfinished. No FPS or API-conformance claim follows
from these results.
