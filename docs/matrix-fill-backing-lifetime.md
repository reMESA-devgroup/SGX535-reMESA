# Backing continuity between ordinary clear and draw

The ordinary OpenGL frontend clears COLOR before `draw_vbo` constructs its
GPU scene resources. The physical attempt showed that the old first-owner
source guard cannot accept a native 2D fill before 3D. Retaining a private
backing owner early is necessary for the eventual owned source transition,
but the existing backing freeze would then reject those later allocations.

The normal matrix path now keeps one stable backing owner from native fill
through CPU scene construction to matrix submission. This is guarded by the
existing default-N matrix control and remains root-only. No new UAPI, module
control, scheduler or rendering encoder is added. The kernel-only token is
never supplied by userspace.

`remesa_matrix_fill.inc` validates the concrete full black clear, acquires
the existing private backing lease and retains the actual GEM/native 2D
retirement record. Successful retirement opens an idle CPU construction
phase. Repeated matching clears reuse the owner and require a newer GEM
operation; native cookies retain their existing, separate identity domain.
Other copy operations cannot run through that private owner. Known unissued
errors release a new owner or restore the earlier completed clear's phase;
uncertain or malformed completion retains the context. No guessed cleanup
or source approval occurs.

`remesa_matrix_construct.inc` permits bounded CPU allocation, pin/map, leases
and transfers for the other resources during that phase. The normal memory
service still applies all ID/generation, size, VA/PTE, permission, hold and
lease checks. COLOR's exact ID stays protected from writes, writable leases,
unmap, unpin and destruction. An active job/executor, frozen phase, lost file
or poisoned session cannot use construction admission. File loss revokes
the phase without freeing retained ownership.

Matrix submission authenticates the existing owner and completed clear,
ends construction before inspecting the actual graph, and uses the same
existing matrix executor, source claim and cleanup. Successful backing release
clears the phase and protected ID. The **source authority has not changed**:
this intermediate version still refuses the old 2D-before-3D lineage. Backing
ownership and CPU allocation permission are not execution permission.

Evidence is under
`/home/gama/sgx535-offline/matrix-fill-backing-20261103/v1/`:

- `paired-controller-final/RESULT.json`: 23 actual owner/controller scenarios
  per native/UBSan/i486 variant, including incomplete and mismatched receipts,
  lost identities, repeated clear preparation, known unissued cleanup and a
  draw source refusal. Fill/source/retirement dependencies are modeled.
- `joined-fill-endpoint/RESULT.json`: the actual typed fill/copy endpoints and
  owner together, using the real packet constructor and modeled job execution.
  Covers success, pre-job allocation failure, prepared cancellation, uncertain
  issue, absent/mismatched proof and the disabled matrix profile.
- `retained-construction/RESULT.json`: actual retained memory operations with
  modeled pages/MMU. Allocate/pin/map/write/read succeeds; COLOR writes and
  destruction, wrong generation, out-of-bounds access, live-job mutation and
  allocation after freezing are refused. No GPU or source evidence is inferred.
- The construction-policy, cold-controller, backing/source-session, file-loss
  and closed-controller results preserve their existing refusals. The full
  module build is under `final/kbuild/RESULT.json`; it is not installed.

This advances the concrete `glClear` -> resource construction -> `glDrawArrays`
lifetime, but does not yet unlock a successful physical ordinary draw. The
next source hook belongs before the fill's first publication and must cover
its restore interval and attributable retirement, using this same owner.
The old frozen guard must keep rejecting unrelated producer history.

These intermediate modules are not public beta binaries. A downloadable beta
still needs the physical source transition, continuous correct COLOR/Z across
draws and frames, the normal Mesa/X11 runtime, and tested installation/removal
on the stated hardware/kernel combinations. No OpenGL 2.1/GLES 2.0 conformance,
desktop-wide acceleration, release deadline or FPS gain is established here.
