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
continuous GLXGears evidence remains valid. The new CODE344 recipe requires
its own physical qualification.
