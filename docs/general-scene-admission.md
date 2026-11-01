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

## First physical admission result

The first new-entry test on Mini12 SGX535 rev121 completed its native event
lifecycle at 1280x800 and released the source, private root and GEM job. The
structured result was operation/cookie 1, phase 9, ledger 7, stages 15, no error
or retained state. However, both complete COLOR and Z readbacks were zero.
This is a failed output qualification, not a rendered frame or an FPS result.
All twelve READ allocations and four guards were intact, cleanup completed,
and the machine returned to the verified stock boot with the original GRUB
configuration. Evidence, raw request/response and outputs are preserved under
`/home/gama/sgx535-offline/general-scene-admission-20261101/v1`;
`PHYSICAL_RESULT.json` and `VERIFY_PHYSICAL.py` make the failure explicit.
The 198 ms ioctl wall time includes CPU publication, execution, synchronization
and retirement; it is not a GPU-only measurement.

The optional `--region-prefix` collector diagnostic adds at most 4 KiB of
post-retirement REGION readback, with its own timing. The closed REGION mapping
must be at most 1 MiB. It never maps or reads the 32 MiB DPM pools. This isolates
TA output from later raster/output issues without changing the programs,
placement, native module or safety gates. Object INFO responses already checked
by the LAB collector are now also saved; this adds no extra INFO calls.

A second fresh test boot reused the identical module, READ allocations and GPU
placement. It reproduced zero COLOR/Z and also returned a zero first 4 KiB of
REGION. The remaining REGION bytes and DPM pools were not read, so this does not
establish that the whole TA output was unchanged. The diagnostic read took
3.009 ms; the ioctl wall time was 186.078 ms. All 21 object INFO records were
saved and verified, twelve READ inputs and four guards remained intact, and
stock recovery completed again. Evidence and verifier are under
`/home/gama/sgx535-offline/general-scene-admission-20261101/v2-region`.
The unresolved issue is the new integrated submission's empty output. Another
identical run without additional discrimination is not useful. Compare the
actual publication/TA start, native plans and existing qualified physical input
contracts before changing any shader or geometry encoder. Further instrumentation
must remain bounded and LAB-only; a complete lifecycle receipt alone must never
qualify rendering.
