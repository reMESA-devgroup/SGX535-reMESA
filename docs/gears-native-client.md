# Bound gears native LAB client

`tools/remesa/gears_scene_native_client.c` connects the existing bound Gallium
gears scene submission to the separate version 2 kernel LAB protocol. It uses
18 bindings and 11 USE references at 1280×800 or 640×640. The original
17-binding client, protocol and catalogue are unchanged.

The client borrows an already open descriptor. It binds the existing authored
NIR shader, the physically witnessed position/normal input and coefficient
block, and real COLOR/Z attachments. The authored NIR fixture is shared with
`test_nir_gears_vertex.c`; its builder is unchanged. The client does not
transform vertices or rasterize on the CPU.

Before submitting, it checks the packet, placement and all immutable READ
images against the closed fixture. After authenticated kernel retirement,
inside the submission callback, it collects readbacks before temporary scene
owners are released. A complete 18-binding trace is saved, including a raw
snapshot after an uncertain receipt. An uncertain submission does not trigger
readbacks, teardown or a retry.

`tools/remesa/gears_scene_native.py` supervises this client. Its default action
writes a plan without opening a device. Reference images are compared but are
never uploaded by the supervisor. Successful collection checks all 21 owners,
four guards, immutable buffers and the seven surviving allocations before
explicit teardown. Failed or incomplete evidence suppresses that teardown;
closing the descriptor leaves uncertain resources to the kernel's existing
retention policy.

These diagnostics belong to LAB and add no work to normal FAST rendering.
The supervisor reports `RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING`:
a valid receipt and intact memory are not proof of correct pixels.

## Validation and remaining physical boundary

Evidence: `/home/gama/sgx535-offline/gears-native-client-20261101/v1`.
The unwrapped i486 binary compiled against actual Mesa/NIR and the current
driver. Six syscall-model cases cover prepare, execute and uncertain receipt
at both sizes. Supervisor tests cover child failure, timeout, missing or
corrupted receipts, READ/guard/packet/inventory corruption, trace ownership
and the eighteenth binding. The original NIR positive/mutation tests and
independent CODE/tail oracles also pass.

Models execute no GPU work and produce no pixel approval. No module, gate,
boot configuration or hardware was changed for these tests.

Before a physical run, pin independent COLOR/Z24, control, source and guard
oracles and the matching module/client/recoverable boot identity chain. The
new scene clears Z to one across the complete target; the old witness cleared
only its small target. Preserve that distinction rather than copying the old
background-depth oracle. Ignore only the explicitly undefined X8 component.
Use a fresh N/N boot and confirm identity, submission, completion, retirement,
readbacks, buffer integrity, teardown and stock recovery. Never retry a
consumed attempt or send this protocol to an older module.

Full OpenGL 2.1/GLES2 coverage, general clipping/perspective and continuous
production submission remain separate work. This client does not advertise
those capabilities or replace the existing working GLXGears path.

## Independent pixel and control expectations

`tools/remesa/gears_scene_oracle.py prepare` builds a separate LAB expectation
for the sealed positive witness. It accepts only the preserved input hashes
and the two closed targets. Coverage uses the original exact rational
sample-centre construction. The 1280×800 COLOR result matches the retained
physical readback byte for byte; Z24 matches inside the old 48×48 footprint.
The newly cleared remainder is explicitly expected to contain `0xffffff`.

`verify` requires the manifest SHA-256 pinned before submission. It checks all
COLOR bits and the low 24 depth bits, reports mismatch counts and exits with
failure on disagreement. Only the undefined X8 depth byte is ignored. Pixel
agreement alone never grants physical qualification. In particular, the
actual syscall-model outputs fail this pixel gate at both resolutions.

Prepared expectations, native/UBSan/i486 control exports from the existing
validators, and source/guard/lifecycle requirements are pinned under
`/home/gama/sgx535-offline/gears-pixel-oracles-20261102/v1/PRELAUNCH_ORACLES.json`.
No new rasterizer or encoder was added to the driver. These calculations are
LAB expectations and are never uploaded to the GPU or used for presentation.

The physical boundary for this selected scene subsequently passed in both
sizes. See [bound-gears-physical.md](bound-gears-physical.md) for the evidence,
recovery status and remaining API/continuous-execution limitations.
