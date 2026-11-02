# Native homogeneous gears client and sealed W2 inputs

`tools/remesa/gears_homogeneous_native_client.c` reuses the existing native
collector with the bound homogeneous Gallium entry point. It binds the actual
viewport, blend/depth states, authored NIR, raw position/normal buffer and
constant buffer, using the same Linux/GEM provider and complete scene owners.

The separate binary accepts `--prepare-only` and `--execute`. Its compile-time
homogeneous path selects only the explicit state15 profiles 3/4 through the
matching Linux transport. The original binary still selects profiles 1/2.
Execution requires the matching gated kernel; this new complete native path
has not yet been physically qualified.

The candidate uses raw PN and coefficient bytes extracted from the sealed
positive W2 physical evidence. No vertex transform is performed on the CPU.
The actual bound context and the native client produce identical immutable
inputs at 1280×800 and 640×640; the native client verifies all 13 READ images
before entering the scene ioctl. Preparation alone returns definite NOT_ISSUED
and unwinds temporary owners. Execution collects diagnostic readbacks after
authenticated retirement, before releasing temporary scene owners. The caller
still owns the borrowed descriptor and the remaining frontend/guard buffers.

`tools/remesa/gears_homogeneous_oracle.py` builds a separate CPU expectation
from those fixed input hashes and the original rational sample-centre rule.
The 1280×800 COLOR expectation matches the retained physical readback exactly:
135 white pixels. Defined Z24 matches inside its old 48×48 footprint; the new
whole-target clear requires background `0xffffff`. The 640×640 expectation is
55 white pixels and is CPU-only, with no new physical qualification.

Oracle verification reuses the existing manifest-sealed COLOR/Z24 checker.
It rejects blank or corrupted targets and ignores only undefined depth X8.
No expected image is uploaded to the GPU or used for presentation.

Evidence: `/home/gama/sgx535-offline/homogeneous-native-client-20261102/v1`.
That earlier preparation milestone includes unwrapped i486 binaries, syscall models,
explicit execute-refusal checks, six legacy regression cases, prior physical
input/expected-pixel comparisons, independent oracle tests and both W2 captures
through all three retained kernel models. Hardware callbacks are modeled;
no new GPU operation took place.

`CANDIDATE_INPUTS.json` records the proposed selectors 3/4. Its augmented sealed
input record, `CANDIDATE_INPUTS_WITH_PATHS.json`, feeds the separate generator
`tools/remesa/build_gears_homogeneous_probe_profiles.py`. The generated
`remesa_gears_homogeneous_probe_profiles.h` registers those two exact scenes in
newly staged kernels. The old selectors 1/2 and their table remain unchanged.
The request and trace layouts and the shared once-per-boot gate are unchanged.
Registration does not qualify the new physical path or enable a gate.

The Linux memory adapter `remesa_linux_memory_gears_homogeneous_lab_dispatch`
selects 3/4 explicitly. The old adapter still selects 1/2. Both use the same
ownership, address, permission, completion, retirement and uncertainty checks.
An entered failure retains and poisons all endpoint resources without retry.

Offline evidence for this integration is under
`/home/gama/sgx535-offline/homogeneous-native-wire-20261102/v1`.
It covers all four transport selectors, separate old/new controller catalogues,
wire layout preservation, altered state rejection and a full module build.
The module is not installed. Native execution integration and its tests are at
`/home/gama/sgx535-offline/homogeneous-native-execute-20261102/v1`.
The actual i486 client passes preparation, modeled success, malformed receipt
and interrupted ioctl at both sizes, with the old client tested alongside it.
Uncertain outcomes retain all 25 owners without retry or teardown. Separate
supervisor tests cover collection failures and remaining-owner cleanup. Blank
model COLOR/Z readbacks fail the pixel oracle, as required.

The supervisor requires `--homogeneous` to load the new catalogue; default mode
refuses it. Without `--execute`, it writes a plan and opens no device. A
relocated asset tree uses the same `--asset-root` interface and pinned inputs.
Next qualify the complete path on a fresh recoverable N/N boot at each size. Do not
submit these images through old selectors or treat model receipts as GPU
evidence. Complete OpenGL 2.1 remains unproved.
