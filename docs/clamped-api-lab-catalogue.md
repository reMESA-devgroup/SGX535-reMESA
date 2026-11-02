# Actual OpenGL input catalogue for the clamped native vertex program

The closed one-shot LAB controller has two additional explicit selectors.
Selectors1/2 keep the original state14 inputs; selectors3/4 keep the physically
tested state15/W2 inputs. Their three generated catalogue headers are unchanged.
New selectors5/6 identify the actual Mesa API captures at 1280×800 and 640×640,
with CODE344, aligned USE capacity352 and authenticated state15.

These exact inputs now have finite physical qualification through the
[actual API client](opengl-native-client.md). The input files
come from actual Mesa matrix/material/light and raw vertex/normal arrays,
compiled through the existing shader path into the complete owned scene.
They contain no expected framebuffer uploaded as rendered output. The API's
normal-w and unconsumed coefficient alpha lanes are preserved bit for bit.

`tools/remesa/build_gears_clamped_api_probe_profiles.py` checks the sealed
CODE344/PN96/coefficient144 inputs, packet/placement, eighteen disjoint owned
bindings, READ images, raw P/N packing, coefficient upload and effective
state15/PDS/TA contract. It checks that the committed CODE ends at344, the
eight reserved bytes are zero, and the fragment body starts at352. It reuses
the existing catalogue serializer and state15 validation. Default CODE320
generation and the previous catalogue output remain unchanged.

The existing default-off privileged LAB ioctl selects the new immutable
catalogue explicitly. Request size152, result offset96 and trace size4280
remain unchanged. The old seventeen-binding endpoint remains separate.
There is no new ioctl, caller-injected CODE, address or packet. All existing
owner/generation/address/permission/cache checks, exact READ-image digests,
one-shot exclusion, source reservation, identity checks, completion,
retirement and failure quarantine still apply.

The new userspace adapter is
`remesa_linux_memory_gears_clamped_api_lab_dispatch`. It selects only5/6;
the previous adapters continue selecting1/2 and3/4. This adapter performs no
pixel checks or hashing and cannot enable a kernel gate. Registration does
not activate it on ordinary screens or FAST frames. The frontend's
pending raster class remains refused by default. The subsequent
[finite API raster contract](homogeneous-api-raster.md) requires separate
context-local enabling and authenticates its bound recipe before native owner
allocation. The real Linux/GEM API client and physical qualification remain
required before claiming execution of this new recipe.

Offline evidence is under
`/home/gama/sgx535-offline/clamped-api-catalogue-20261102/v1`.
The deterministic generator tests reject altered metadata and rehashed
CODE/clamp/tail/capacity/state/PDS/packet inputs. Controller and transport
tests exercise success and refusal/lifetime behavior with modeled hardware.
The new catalogue tests reject37 mutations and the previous state15 generator
still rejects24. The controller runs44 cases per eighteen-binding family and
58 for the legacy family in native, UBSan and i486 variants. Fifteen transport
suite runs pass across those variants. The real antiX5.10.240 i486 module
build completes with no new warning messages; it has not been installed.
They establish no GPU operations, pixel results, FPS or OpenGL conformance.
The earlier physical OpenGL triangle and GLXGears results remain valid within
their documented scope.
