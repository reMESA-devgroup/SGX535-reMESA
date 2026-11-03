# Ordinary matrix draw input checks

The ordinary frontend already builds the selected matrix/lighting draw through
Mesa `glDrawArrays` and Gallium `draw_vbo`. The next Linux admission boundary
must check actual held input bytes independently of that frontend.

`matrix_draw_domain.h` shares the previous frontend bounds as integer IEEE754
comparisons. Gallium now uses this shared predicate; the kernel applies it to
the resolved PARAM coefficients and packed vertex window after the existing
PDS/TA/USE closure checks. No floating point runs in the kernel, no vertex is
transformed, and no shader or encoder is replaced.

The domain remains explicit: finite bounded coefficients, diagonal scale at
most one, translation at most one half, W=2, bounded POSITION/NORMAL records
and a nonzero bounded normal/light. Float3 NORMAL padding W=1 and explicit
W=0, including negative zero, retain the earlier acceptance. The packed
48-byte GPU record must also retain the unused zero float4 emitted by the
existing owner. This is a construction check, not extra shader CODE.

The kernel prerequisite additionally requires the existing clamped344 vertex
body, initial state, one draw, Z24 and the existing depth-finalization contract.
It checks the non-reference fields of the emitted opaque LESS/write/full-RGBA
state15, full MATRIX_WINDOW viewport with near0/far1, scalar groups and final
zero word. Fragment references remain authenticated by the existing closure.
Expected half-dimension values are compared using integer IEEE754 bits;
existing viewport emission stays unchanged.

The existing prefix clear state and consumed three vertices/three indices
must match the unchanged single-triangle Z-clear constructor. Initial and
close source checks are reused from the earlier validator. Neither a padded
sixteenth state word nor the unconsumed fourth vertex/last three indices is
read as effective input.

`remesa_gem_scene_job_matrix_values_check_locked()` is separate from existing
LAB callers. It requires the same locks, private root and immutable held views
as the PDS prerequisite and changes scratch plans only. A positive result is
not a source lease, COLOR initialization receipt, GPU submission or retirement
certificate. It enables no ioctl or module parameter and does not change a
LAB catalogue. Future mutable PARAM updates must preserve this value-domain
contract before publication; an old LAB update API is not normal admission.

`remesa_gem_scene_job_execute_matrix()` uses the existing ordered native
transaction and makes this input check mandatory after private-root binding,
before service binding, publication or possible issue. The original trusted
entry retains its existing behavior. Failure at the new boundary follows the
existing quarantine/retention path; a source/root attempt is not re-armed or
released based on a guessed completion. No userspace execution gate is enabled
by adding this kernel entry.

Offline evidence is kept under
`/home/gama/sgx535-offline/native-matrix-values-20261103/v1/`:

- 12,601 cases per CPU variant compare the shared integer predicate with the
  frozen previous float predicate at both host and GPU strides, including
  signed zero, subnormals, boundaries, infinities and NaNs.
- The existing ISP/viewport/scalar/clear encoders are checked at 2,052 dimension
  pairs per variant, with 4,288 payload/geometry/index bit mutations. Spare
  vertex/index capacity is deliberately changed without affecting approval.
- Current ordinary Mesa API cases run at1280x800/640x640. Their exported bytes
  enter the real transformed retained core, the new source checks and the
  matrix executor/retirement with hardware callbacks modeled. Numeric and
  ISP/prefix failures are tested separately from PDS failures, without added
  maps, clears or flushes. An invalid matrix is also supplied directly to the
  matrix executor: it is refused before publication/service issue, and actual
  modeled job/loan/backing ownership remains retained.

These are native/UBSan/i486 CPU tests, not new physical rendering or FPS
results. The next integration is an independently admitted normal Linux
submission path using existing memory, initialization and source/executor
ownership. Existing closed LAB interfaces cannot carry that dynamic graph.
A concrete normal packet transport is therefore needed for `glDrawArrays`;
it must not expose raw registers, arbitrary CODE or unchecked GPU addresses.
The first physical API test still requires correct COLOR/Z output, attributed
completion/retirement, buffer integrity and stock recovery. General API,
desktop, continuous operation and installation remain separate requirements.
