# Ordinary OpenGL matrix submission

The ordinary `glClear` / `glDrawArrays(GL_TRIANGLES)` frontend now has a
concrete Linux transport to the existing matrix executor. This is a narrow,
experimental integration, not OpenGL 2.1 conformance or an installable desktop
driver. No physical pass or FPS improvement is claimed for this change.

`matrix_submit_uapi.h` carries the existing typed scene packet and owned BO
IDs. It adds command 17 specifically because the closed LAB session API
cannot accept this ordinary packet. There is no new scheduler, raw GPU
address, register stream, shader upload interface or LAB catalogue selector.

The controller requires root, memory service enable and the separate
`remesa_matrix_submit_enabled` control, which defaults to N. The previous
seven controls retain their behavior; a future boot inventory must check
this additional control too. It shares the existing source-attempt mutex,
stable retained owner, private session, MMU confinement, guard, job holds and
retirement cleanup. It always calls `remesa_gem_scene_job_execute_matrix`,
including the already validated PDS, source-value and ISP checks before
publication. An uncertain or consumed failure retains ownership. There is
one consumed source attempt per boot; continuous source lifetime is still
unfinished. Unsupported states remain explicit refusals.

The userspace adapter issues exactly one submit ioctl. It checks the complete
input echo, native operation ordering after 2D fill, identity, retirement,
ledger and closure before translating the native receipt to its host ticket.
Malformed or ambiguous outcomes poison the owned objects and cannot be
retried or freed as if retired. The adapter contains no framebuffer hashes,
pixel comparisons, diagnostic readbacks, INFO loop or diagnostic waits.

`tools/remesa/opengl_matrix_native_client.c` connects the existing ordinary
screen registration, native COLOR fill and matrix adapter to actual Mesa
state-tracker API calls. It borrows a supervisor-owned fd and accepts only
`--execute`; it neither enables controls nor boots the machine. Captured
inputs, readbacks, four guards and receipt files belong exclusively to this
one-shot qualification executable. It reuses the earlier client collector
without enabling its LAB frontend or invoking its LAB submission callback.
The old client keeps its existing registration and command-line modes.

Evidence is under
`/home/gama/sgx535-offline/normal-matrix-transport-20261103/v1/`:

- `transport-tests-v2/RESULT.json`: ten transport suites on native, UBSan and
  i486; the new suite changes every response byte, tests errno/retention,
  mapped aliases, construction limits and host/native ticket separation.
- `controller-final/RESULT.json`: 41 controller cases per variant, including
  cleanup failures, pre-publication refusal and retained ambiguous ownership.
  The execution dependencies are modeled; this is not a GPU result.
- `closed-controller-regression/RESULT.json`: existing closed clamped API
  controller regression, without changing catalogue contents or admission.
- `normal-gl-final/RESULT.json`: twelve actual Mesa API cases across
  1280x800 and 640x640, with native callbacks modeled.
- `client-final/RESULT.json` and `client-model-final/RESULT.json`: compiled
  physical client and its actual API-to-Linux-transport offline integration.
  The latter interposes all ioctls and explicitly models receipts; it does
  not rasterize, prove pixels or access hardware.
- `build-v2/kbuild/RESULT.json`: complete antiX i486 module build, no new
  warnings, not installed. `MATRIX_DEFAULT_N.txt` records the new gate's
  zero-initialized BSS symbol. `PROTECTED_CHECK.json` checks all 31 pins.

The previous physical changing-matrix result remains FAILED for stale COLOR,
with valid retirement and Z. Nothing here relabels it as passing. The next
physical step requires an updated eight-control boot/rollback inventory and
a bounded ordinary-client supervisor with presealed independent COLOR/Z
expectations, input/guard checks, exact receipts and conservative cleanup.
Run one job in a fresh recoverable boot, starting at 1280x800; only proceed
to a separate 640x640 qualification after the first result is valid. Do not
substitute the ioctl model or X11 presentation for that hardware evidence.
