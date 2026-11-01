# Closed general-scene admission

The private staging profile now connects the existing general packet preparer,
ordered executor, native backend and retirement path to a separate root-only LAB
ioctl. This is a qualification entry, not a complete OpenGL/GLES submission ABI.
It does not replace or extend the canonical eleven-resource probes.

`DRM_REMESA_SCENE_PROBE` uses slot 11 and a 144-byte request on native and i486
ABIs. Inputs are version, closed profile selector, actual memory context,
seventeen file-local object IDs and a bounded timeout. Response and reserved
fields must enter zero. No caller packet, GPU address, program, pointer or event
is accepted. Both memory admission and `remesa_scene_probe_enabled` default off.
A consumed source claim permits only one attempt per boot.

The generated profiles pin the actual Mesa producer's finite-input 1280x800 and
640x640 packets, placements and twelve complete READ allocations. Only object
IDs/context are translated. Program bytes, GPU addresses, encoders and the
existing full closure validators are unchanged. RW allocations must retain
fresh kernel-zero provenance and have no CPU aliases. Actual powered SGX535
core/revision and PDS base are checked before admission.

An opaque existing backing lease pins the memory endpoint, module, device and
power dependencies even before job preparation or after job retirement. The
whole-device guard and source lease exclude other producers. The executor
performs actual submission, completion, visibility, source/root restoration and
acknowledgment. Only then may the controller release the loan, guard and actual
GEM job, close the source lease and finally release the backing lease. Any
uncertain consumed failure retains the heap controller and backing dependencies;
there is no forced cancellation, retry or synthetic retirement.

The collector `tools/remesa/scene_probe.py` defaults to an offline plan. It does
not install a module, enable admission, reboot or present pixels. Execution
requires a separately verified recoverable test boot with matching profiles.
It uploads READ allocations, adds guards outside the private allowlist, submits
once, verifies the structured receipt and mapping ownership, and collects READ,
COLOR, Z and guard readbacks. It does not read the entire DPM allocation. An
uncertain receipt suppresses explicit memory teardown. Its success status still
requires an independent pixel/depth oracle; it never declares the whole driver
qualified.

Offline plan example (the catalogue must match the generated kernel profile):

```sh
python3 tools/remesa/scene_probe.py \
  --catalogue /absolute/path/PHYSICAL_INPUTS_FINAL.json \
  --size 1280x800 --output /absolute/new/plan
```

`--asset-root` relocates the pinned files under size-specific directories;
`--execute` explicitly opens the device. Never launch an old one-shot supervisor
against a new boot or change its pinned identities merely to bypass a refusal.

The controller harness covers 44 cases in native, UBSan and static i486 builds.
It models GEM, hashing, source, MMIO and execution dependencies: passing it is
not physical GPU evidence. The client harness covers successful cleanup and
malformed or uncertain receipts. Existing LAB controller tests remain separate.
The complete i486 kernel module and identity-bound initramfs build offline.
Physical qualification of this new seventeen-resource entry remains pending.

Hashes and readbacks here authenticate a one-shot LAB experiment. They do not
run in FAST frames. General frontend API coverage, complete desktop integration
and a validated installable driver remain required after this qualification.
