# OpenGL client of the native Linux/GEM scene executor

`tools/remesa/opengl_homogeneous_native_client.c` creates a real Mesa
state-tracker context on the resource frontend. It sets matrix, material,
lighting, vertex/normal arrays and viewport through OpenGL, then calls
`glClear`, `glDrawArrays`, `glFlush` and, after authenticated retirement,
`glReadPixels`. It does not create authored NIR or replace vertex/raster work
with CPU rendering. Mesa reports its actual computed OpenGL version1.3 and
GLSL0; this is not an OpenGL2.1 or GLES2 completeness claim.

The client borrows its supervisor's descriptor and reuses the existing native
collector with explicit LAB selectors5/6. It cannot open a DRM node, enable a
gate, install a module, reboot, present pixels or retry a submission. The
default direct Gallium clients retain selectors1/2 and3/4. The only shared
collector parameter is the exact source-owner count: three for the old
clients and two for the API route, whose inputs use bounded host storage.
The expected inventories remain strict:20 owners,24 with guards and6 after
capture release for the API, versus21/25/7 for the old clients.

All18 owned packet bindings, addresses, limits, cache/permission policy,
thirteen immutable READ images and four guards are checked as before. The
collector saves the raw response and trace, verifies physical identity and
retirement fields, and reads COLOR/Z before consuming the temporary scene.
The API subsequently reads the still-owned published COLOR attachment. A
bottom-up BGRA `glReadPixels` image must equal the vertically flipped retired
COLOR backing. The supervisor still requires a separate presealed pixel/depth
oracle before physical approval.

An entered ambiguous or malformed receipt retains all24 owners and suppresses
readback, teardown and retry. The Mesa frontend/drawable bundle has heap
lifetime so retained failures cannot leave references to expired helper stack
objects. Definite nonissuance unwinds the temporary graph. Successful receipt
and readback leave six borrowed frontend/guard owners for supervised cleanup.

The separate `--opengl` supervisor mode requires the actual-API catalogue and
matching client/kernel. It refuses the old catalogue modes and verifies the
API report, GL readback, complete owner/integrity evidence and cleanup. Without
`--execute`, the Python supervisor only writes a plan and opens no device.
The C client's `--prepare-only` may allocate through its already borrowed
descriptor but never enters the scene execution ioctl.

Offline evidence is under
`/home/gama/sgx535-offline/opengl-native-client-20261102/v1`.
The unwrapped i486 executable is built alongside syscall models. Tests cover
both sizes, definite preparation, successful modeled retirement/readback,
bad receipts and interrupted ioctls, plus the older clients and supervisor
fault suites. Optional synthetic row tags exist only in the linked syscall
model to verify readback origin/copying; they execute no vertex or raster
program. Blank modeled frames fail the independent COLOR/Z oracles. Neither
model receipts nor row tags establish GPU execution or rendered pixels.

Execution on the Mini12 requires a fresh, separately supervised boot, exact
module/client/image identity and the existing recoverable stock default and
seven default-off controls. Prior physical OpenGL, GLX and restricted
continuous GLXGears evidence remains valid. Physical qualification of the new
finite CODE344 recipe is recorded below.

## Physical API qualification at both target sizes

The same unwrapped client and kernel module completed one GPU operation per
fresh boot on the SGX535 rev121. The API calls were real Mesa calls; the
physical executable had no syscall model linked and received no expected
framebuffer image as an input.

| Target | Selector | Expected white pixels | COLOR / defined Z24 mismatches | `glReadPixels` bytes |
| --- | ---: | ---: | --- | ---: |
| 1280×800 | 5 | 135 | 0 / 0 | 4,096,000 |
| 640×640 | 6 | 55 | 0 / 0 | 1,638,400 |

Each receipt reported core `0x01130000`, revision `0x00010201`, PDS base
`0x20000000`, operation/cookie1, phase9, ledger7, stages15, error0 and retained0.
Both cases matched all66 presealed control words, preserved13 READ inputs,
two additional CPU/source owners and four guards, and completed supervised
memory cleanup. Actual API readback matched both the presealed expected BGRA
bytes and the retired COLOR backing with the required vertical flip. Z24
comparison excludes only the existing undefined X8 bits; raw Z is retained.
The driver reported zero CPU clears.

Module SHA256:
`eb495240ab075735ff73398a7ee4bbc960adad440db0bf785a5fd056e209d411`.
Boots were `bf6beecd-4714-407a-89e3-ef964d075882` at1280 and
`9aea28e2-d380-4b9f-b73d-48fb46ca575f` at640. Each test closed the source,
returned all seven controls to N and recovered the stock module/default boot.
Final stock boot: `aa1297c4-5779-4024-abda-bf145f012a97`. No operation remains
pending. A transient SSH failure before the640 verification is preserved;
only identity/restore verification was retried after observing the new boot.

Evidence:
`/home/gama/sgx535-offline/opengl-native-client-20261102/FINAL_PAIR_RESULT.json`,
with raw reports/readbacks and independent audits in `v1` and `v2-640`.
This qualifies the new finite API-to-native-executor route, preserving the
earlier OpenGL triangle, GLX swap and restricted GLXGears proofs. Inputs remain
in range, so it does not qualify arbitrary clamp corner cases. These isolated
cold LAB operations establish neither sustained FPS nor general OpenGL2.1,
GLES2, desktop2D or a complete installed driver.

The next executor integration should reuse the already physically qualified
[private multi-job session](private-session-lab.md) and its
[C transport](private-session-transport.md). Its old eleven-ID ABI and the
one-shot gates must remain intact while adapting the eighteen-binding scene
and attachment lifetime/clear/load contracts. Clearing a consumed flag is not
a continuous renderer.
