# Gallium linked scene ownership and submission

The explicit linked-scene path now connects bound Gallium NIR programs, raw
vertex/index inputs and typed GEM COLOR/Z attachments to the existing scene
builder and provider submission. It reuses the qualified encoders; it does not
replace vertex execution with CPU transforms or initialize attachments in CPU.

This is an integration boundary, not a complete OpenGL implementation.
Ordinary `draw_vbo`, resource-only restrictions, advertised capabilities and
kernel admission remain unchanged. The accepted semantic link is still the
existing POSITION/NORMAL/LIGHT white-Lambert link. The explicit caller supplies
the typed ISP/viewport/clear contract and must authenticate native admission.

## Ownership

Call `remesa_screen_enable_linked_attachment_storage` before creating children.
The actual persistent device must validate its identity and placement contract.
COLOR uses the surface allocator and Z uses the existing typed depth allocator.
The opt-in is separate from the CPU depth LAB and does not advertise graphics.

`remesa_context_prepare_linked_scene` derives attachment owners, dimensions,
pitch, semantic linkage and geometry from bound state. Options cannot substitute
these fields. Raw inputs are snapshotted using the existing mapper, including
its writer synchronization and lease cleanup. The frozen capture retains its
backing after frontend shader/context/resource destruction.

`remesa_context_submit_linked_scene` performs preparation and submission as one
externally serialized call. It allocates the fence wrapper before dispatch and
reserves both attachment transitions on local pass copies. A conclusively
unissued error releases the capture and leaves the attachment passes unchanged.
Success publishes **one** reference-counted pipe fence to COLOR, Z, the context
and the caller. Independent attachment waits use the same authenticated backend
retirement/publication receipt; final fence destruction occurs once.

An uncertain native submission or malformed receipt poisons both attachments
and the context, suppresses stale flush fences and retains backend quarantine.
An uncertain allocation during preparation also blocks further context use;
the allocation backend retains the uncertain handle while allowing disposal of
unrelated, never-submitted backing. Destruction is never evidence of retirement.

The optional device-ops structure has grown. Rebuild all current driver objects
together; do not combine a new screen object with an older reMESA archive.

These methods add no per-frame hashes, pixel comparisons or diagnostic dumps.
The standalone tests and exported fixtures below are offline diagnostics only.

## Evidence and limits

Evidence root:
`/home/gama/sgx535-offline/frontend-linked-scene-20261101/v1`.

* `coherent-submit-final/RESULT.json`: all current driver objects built together
  against the existing Mesa 25.2 Gallium/NIR libraries, 18 frontend cases plus
  the existing linked vertex/context regression. Includes 1280x800, 640x640,
  641x479 and 1600x768 storage, shared-fence lifetime, unissued retry, uncertain
  issuance, uncertain allocation and an unacknowledged receipt. GEM callbacks
  and native receipts are modeled: **zero physical GPU operations**.
* `default-driver-regression-final/RESULT.json`: unchanged default Gallium
  callback/resource/submission test linked with those coherent driver objects.
* `default-callback-matrix/result.json`: five existing transfer/framebuffer/
  capability/driver/resource-sync suites, native and UBSan, compiled with
  `-Wall -Wextra -Werror`. No physical device operations.
* `no-mesa-api/RESULT.json`: the three new API symbols link in the standalone
  build and return `ENOTSUP`; no implicit fallback implementation.
* `FRONTEND_RELOCATION_FINAL_RESULT.json`: all 12 READ images match the existing
  constructor output in three resolutions. Eight images are byte-identical;
  four differ only at four checked address words. The source VB occupies one
  additional DEFAULT page, moving three generated allocations by 4096 bytes.
  Raw P/N/L and USSE/PDS CODE remain identical.
* `coherent-submit-v2/frontend-run-16.log` preserves an initially incorrect
  test expectation that all backing survive an uncertain CREATE. The corrected
  case distinguishes allocation ownership failure before issuance from an
  uncertain GPU submission and checks retention of the uncertain handle.

Earlier **physical** provider/fence rendering at 1280x800 and 640x640 remains
recorded separately in
`/home/gama/sgx535-offline/provider-scene-capacity-20261101/FINAL_RESULT.json`.
Those results do not physically qualify this newer Gallium integration.

## Physical follow-up and remaining execution boundary

The integrated bound-context path has subsequently passed physical COLOR/Z24,
retirement, input/guard integrity and recovery qualification at both primary
sizes. See [gallium-physical-qualification.md](gallium-physical-qualification.md).
The requirements below describe the qualification boundary that was preserved;
they are not a request to repeat the completed tests or reimplement encoders.

The physical client used this bound-context path with the real Linux memory
provider and collected the complete allocation inventory, including CPU-only
owners. Exact closed profiles 7/8 were appended for the relocated addresses;
all six previous profiles were preserved. The independently checked boundary
includes identity, issue/completion, retirement, COLOR/Z readback, READ/non-GPU
integrity, guards, cleanup and stock/N/N recovery. Image/address gates were
preserved throughout.

Ordinary GL state translation, dynamic production admission, repeated sessions,
load/preserve semantics, API coverage, desktop presentation and installation
remain separate unfinished work. This change claims neither sustained FPS nor
OpenGL 2.1/GLES 2.0 conformance.

## Bound opaque depth state

The typed attachment mode now admits the existing disabled-depth or LESS/write
DSA contract at `create_depth_stencil_alpha_state`, using the same validator
already used by the qualified gears state. Legacy and CPU-LAB admission stay
unchanged. Previously, this frontend rejected enabled depth even though the
scene's ISP encoder and physical depth execution were already available.

`remesa_context_linked_isp_snapshot` derives the existing typed ISP request from
bound opaque/full-RGBA blend and depth objects, paired with COLOR/Z attachments.
It canonicalizes disabled depth to ALWAYS/no-write, preserves latched invalid
state errors, rejects failed contexts/attachments and output aliases, and needs
no GPU operation, allocation, mapping or synchronization. The returned value
owns no state pointers. The native LAB client now binds those actual Gallium
objects and uses the snapshot instead of constructing a literal ISP request.

Evidence is in
`/home/gama/sgx535-offline/gallium-bound-isp-20261101/v1/FINAL_INTEGRATION_RESULT.json`:
22 actual Gallium/NIR context cases, the existing context regression, ten
native/UBSan legacy suites, the standalone stub and six Linux-transport client
cases. All previous packet/placement and READ images remain byte-identical in
four storage sizes. These new binding tests use modeled GPU receipts; the
earlier physical qualification remains the physical baseline, not a new physical
approval of this frontend change. Ordinary draw/caps remain unchanged.

## Bound window viewport input

`remesa_context_window_viewport_snapshot` returns the existing typed viewport
request from the bound Gallium viewport and COLOR dimensions. It shares the
encoder's existing validation and arithmetic; callers can pass the returned
value to the scene-state builder without reconstructing rectangle or depth
parameters. Output must be aligned and disjoint from context, state and bound
resource headers. Errors preserve it. This host operation performs no mapping,
allocation, synchronization or GPU submission.

The returned `MATRIX_WINDOW` request is still distinct from the physically
qualified profiles 7/8, which use `INITIAL_ZERO` and window-coordinate vertex
inputs. Switching their viewport to a matrix would apply a second transform.
Physical qualification of the matrix branch needs a separate bounded clip-space
input experiment, with the existing POSITION passthrough program and encoder;
it must not silently change the qualified profiles or ordinary draw admission.

Offline evidence for the typed binding is kept at
`/home/gama/sgx535-offline/gallium-bound-viewport-20261101/v1/`.
