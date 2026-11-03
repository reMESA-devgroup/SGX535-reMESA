# Source accounting for the ordinary clear predecessor

The last physical ordinary OpenGL attempt completed the native 2D clear, then
refused the 3D matrix submission with `-EBUSY`. The first-owner source guard
correctly retained `INTERFERENCE` after the 2D producer ended. A retirement
record by itself cannot authorize the following draw, and claiming the old
source lease before that producer does not fix the problem.

The source contract is now connected to the ordinary full-clear owner and the
authenticated GEM/native executor for this particular `glClear` → `glDrawArrays`
transition. The existing matrix control still defaults to N. Offline integration
is separate from hardware qualification; no new GL version, desktop acceleration
or installable beta is advertised.

`remesa_owned_fill_source.inc` lives under the real source lock. Its internal
claim copies the already valid boot lineage once, before any clear publication.
The stable backing owner must remain exclusive. The subsequent calls identify
one native launch by device, PD, complete job ticket, destination span, request
base, saved base and IRQ cookie. They distinguish publication, IRQ-cookie
installation before FIFO writes, running, restoration, native retirement and
GEM retirement. Only the last successful transition makes the source ready for
the draw. Uncertain or out-of-order transitions poison and retain the lease.

Both native producer intervals still update the original lifetime. Its
`INTERFERENCE` reason is never cleared, and the original claim and generic
producer implementations are unchanged. Generic foreign producers still poison
the source. The new accounting also rejects any other original guard reason,
changed boot facts, startup replay, unexpected pending status, wrong task or
identity, and completion events before issue or after native retirement. It
neither acknowledges events nor infers hardware completion. The native adapter
and GEM must still perform their existing checks.

The existing source bind, write authority, settled check and release select the
new lineage only when that internal claim is active and ready. With no owned
clear claim, they keep the existing first-owner behavior. No UAPI, parameter,
scheduler, shader, encoder, MMU operation, wait, hash or pixel comparison was
added. Tests keep synthetic receipts separate from physical evidence.

## Validation

Evidence directory:
`/home/gama/sgx535-offline/owned-clear-integration-20261103/v1`.
The preceding source-only commit remains recorded under
`/home/gama/sgx535-offline/owned-fill-source-20261103/v1`.

- `source-final/RESULT.json`: 115 scenarios, native, UBSan and i486. Executes
  the actual source fragment, original claim/producer/release/bind and frozen
  guard. Locks, tasks and retirement receipts are modeled.
- `native-final/RESULT.json`: 24 scenarios in those three variants. Actual
  native/source/reservation/IRQ/packet code with modeled MMIO. Covers three
  dimensions, repeated clear, foreign work, pre-cookie events, faults, timeout,
  restoration, malformed receipts and late events. Final GEM receipt modeled.
- `gem-final/RESULT.json`: 17 scenarios in those three variants. Actual GEM
  issue derives its source owner from held endpoint metadata; invalid owners,
  phases and destinations are refused before flush/attach/native start.
- `owner-final/RESULT.json`: 31 controller scenarios in three variants, including
  clear-to-draw source reuse, known unissued cancellation and uncertain cleanup.
- `endpoint-final/RESULT.json`: nine joined fill endpoint cases in three variants;
  source/native execution modeled. `controller-final` retains cold draw coverage,
  and `closed-final` retains the old closed LAB controller coverage.
- `tests-v2/RESULT.json`: the existing original/vendor/fill/LAB native/GEM/copy/fill
  retirement regressions, optional receipts, three variants. Source owner NULL.
- `file-loss/RESULT.json`: native/UBSan plus ordinary-profile UBSan; source loss
  poisons the stable normal owner without releasing jobs, mappings or backing.
- `final/kbuild/RESULT.json`: full antiX i486 module build; not installed.

All of these are offline tests with **zero GPU operations**. Earlier failed
harness runs and the intermediate module remain in the evidence directory.
Only paths marked final are candidates for further work.

## Joined path and next physical experiment

The existing matrix fill owner claims source while endpoint and memory are
locked, before returning to the fill endpoint. Ownership and idle checks are
reused. GEM adds the kernel-only owner to its immutable launch only for that
exact protected COLOR/full-fill phase. Native source accounting covers base and
ALPHA publication, IRQ-cookie installation before FIFO, and restoration. A new
capture before cookie installation retains any event caused by publication in
its original source phase, before the existing ACK. The exact saved request
base is restored; the new packet's 256 MiB window alignment is not imposed on
borrowed state such as the previously tested `0x28000000` value.

Only actual successful native retirement followed by GEM hold release permits
`fill_finish` to enter the CPU construction phase. Matrix admission reuses this
held source instead of reclaiming the first-owner source. A known unissued
failure closes source before backing, or restores a previous retired clear only
if source is still ready. An uncertain failure retains ownership. File/device
loss poisons source and revokes construction without freeing backing. No closed
LAB profile imports this authorization.

The next physical test remains one ordinary 1280×800 clear and transformed draw,
with independent COLOR/Z and guard checks after retirement, exact device/module
identity and stock boot recovery. A successful bounded draw would still leave
continuous execution, multiple resources/states, normal Mesa runtime loading,
X11 desktop integration, installation/removal and broader GL coverage to qualify.
