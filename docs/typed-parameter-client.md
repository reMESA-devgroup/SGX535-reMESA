# Changing a captured matrix between GPU jobs

The retained scene now has a data update path. It reuses the PARAM allocation,
the captured graph and the existing source encoder. A data acknowledgement
does not represent GPU work: a second submission must execute and retire before
the changed result can be read.

`tools/remesa/typed_parameter_native_client.c` is a separate LAB collector. It
uses the same Gallium/NIR setup as the physically checked frozen-input client,
opens an explicit version 2 general session, submits once, binds a new constant
snapshot, updates the captured draw and submits again. It closes the native
session before releasing the capture. Failures stop the collector; it does not
retry, guess ownership, enable gates, install modules or present CPU pixels.

This is still the bounded homogeneous profile, not general OpenGL admission.
The original version 1 client and its tests keep their original input hashes
and immutable-image checks. Diagnostic files and full readbacks in this
collector do not run in the normal data update path.

## The witness

`prepare_typed_parameter_witness.py` reuses the existing sealed W=2 inputs and
the original COLOR/Z oracle. It changes only matrix coefficient 28, from -2 to
-1.96875. At W=2 this is an exact translation of 10 pixels at 1280×800 or
5 pixels at 640×640. W, depth, lighting, geometry and program bytes are unchanged.
The translated result must also clear the old triangle's uncovered pixels.

The generator checks the translation with exact rational coordinates. The
second oracle is an integral translation of the first, with background pixels
inserted and no non-background pixels discarded. These files are CPU
expectations for validation; they are never uploaded or presented as rendering.
Changing the original sealed witness is refused.

For each size, use a new output directory:

```sh
python3 tools/remesa/prepare_typed_parameter_witness.py \
  --fixture /path/to/qualified/1280x800 \
  --size 1280x800 --output /path/to/new/witness-1280x800
```

The output contains the initial fixture plus `coefficients-update.bin`, two
separate oracle directories and `WITNESS.json`. Seal the manifest before any
physical execution. The update writes all 36 coefficients, but exactly one
DWORD is expected to differ in the PARAM image, at byte offset 116. Prefix,
padding, USE/PDS/TA bytes and every other READ object must match their originals.
Buffer order is not a role: the PARAM object is resolved from its actual use
entry, not assumed to be the final binding.

## Evidence and remaining physical work

The integration build and CPU tests are under
`/home/gama/sgx535-offline/typed-parameter-transport-20261103/v1`.
The native client, interrupted/malformed-update models and presealed witnesses
are under `/home/gama/sgx535-offline/typed-parameter-native-client-20261103/v1`.
The actual client links the current coherent driver objects without an ioctl
wrapper; only its separate model executable interposes calls.

`typed_parameter_session_native.py` now supervises this client, with a plan-only
default. It checks the pinned client, witness, actual source inputs, control
words and reference plan before opening the device. The separate
`verify_typed_parameter_session.py` checks the exact data acknowledgement and
separate frame expectations, including the complete expected PARAM image and
all other READ/CPU-owner/guard bytes. Native operations, host operations and
data epochs are checked independently. Cleanup follows only a fully checked
archive and idle ownership; uncertainty suppresses guessed cleanup.

The verifier tests use synthetic archives explicitly marked as such. They
cannot establish hardware provenance. The tests cover both sizes, every byte
of the data request/response/result, protected image bytes and the old/new
verifiers rejecting each other's protocol. Supervisor tests cover child,
timeout, acknowledgement, identity, ownership and cleanup failures. Evidence
is under `/home/gama/sgx535-offline/typed-parameter-supervisor-20261103/v1`.

These results do not qualify changing matrices on the physical GPU. Physical
execution still requires the existing outer boot/source/recovery checks.
The old version 1 verifier deliberately rejects version 2 and changed READ
images. Do not weaken it or count this path as approval of those tests.

A fresh matching module/initrd, first-load source, runtime controls and stock
rollback must be checked by the outer experiment controller. The collector
borrows that controller's file descriptor and has no standalone device-open
or boot-management authority. Neither model execution nor a pixel comparison
alone establishes hardware acceleration, a GL version or FPS.
