# Native homogeneous gears client and sealed W2 inputs

`tools/remesa/gears_homogeneous_native_client.c` reuses the existing native
collector with the bound homogeneous Gallium entry point. It binds the actual
viewport, blend/depth states, authored NIR, raw position/normal buffer and
constant buffer, using the same Linux/GEM provider and complete scene owners.

The separate binary currently accepts only `--prepare-only`. Its compile-time
path contains no scene-probe dispatch; `--execute` is refused before memory
allocation. A matching native catalogue and explicit selector must be added
before this mode may run on hardware. The original client's execute path and
its existing catalogues are unchanged.

The candidate uses raw PN and coefficient bytes extracted from the sealed
positive W2 physical evidence. No vertex transform is performed on the CPU.
The actual bound context and the native client produce identical immutable
inputs at 1280×800 and 640×640; the native client verifies all 13 READ images
before returning definite NOT_ISSUED and unwinding temporary owners. The caller
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
It includes actual unwrapped i486 binaries, preparation syscall models,
explicit execute-refusal checks, six legacy regression cases, prior physical
input/expected-pixel comparisons, independent oracle tests and both W2 captures
through all three retained kernel models. Hardware callbacks are modeled;
no new GPU operation took place.

`CANDIDATE_INPUTS.json` proposes separate selectors 3/4 but does not register
them. Do not use it as an installed catalogue or submit these images through
old selectors 1/2. The next integration must preserve those old profiles,
select the matching new images explicitly and qualify the complete path on a
fresh recoverable N/N boot. Complete OpenGL 2.1 remains unproved.
