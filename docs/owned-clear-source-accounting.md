# Source accounting for the ordinary clear predecessor

The last physical ordinary OpenGL attempt completed the native 2D clear, then
refused the 3D matrix submission with `-EBUSY`. The first-owner source guard
correctly retained `INTERFERENCE` after the 2D producer ended. A retirement
record by itself cannot authorize the following draw, and claiming the old
source lease before that producer does not fix the problem.

This change implements the source-side contract needed for that particular
`glClear` → `glDrawArrays` transition. It is **not activated by a production
caller yet**. It does not make a new OpenGL call work on hardware, advertise a
GL version, qualify desktop acceleration or provide a downloadable beta.

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
`/home/gama/sgx535-offline/owned-fill-source-20261103/v1`.

- `tests-final/RESULT.json`: 110 scenarios, native, UBSan and i486. Executes the
  actual source fragment, original claim/producer/release/bind implementations
  and frozen guard. Locks, tasks and retirement receipts are modeled.
- `legacy-source-final/RESULT.json`: original first-owner producer refusal.
- `scene-final/RESULT.json`: existing general and legacy routing, capsule,
  no-hash closure and refusal scenarios with the full generated source.
- `mmio-final/RESULT.json`: existing pthread authority/write linearization
  model. The old `fixed_write` comparison is read explicitly from its frozen
  baseline; the current source authority uses the inactive predecessor helpers.
- `final/kbuild/RESULT.json`: full antiX i486 module compilation; not installed.

All of these are offline tests with **zero GPU operations**. Earlier failed
harness runs and the intermediate module remain in the evidence directory.
Only paths marked final are candidates for further work.

## Next integration and physical experiment

Wire the stable ordinary fill owner to the internal claim before publication.
Carry its kernel-only identity through the authenticated GEM launch. Call the
source hooks around both native producer intervals, including the IRQ-cookie
installation boundary. Finish only with the exact successful native and GEM
retirement record, then reuse the held source at matrix admission. Handle known
unissued cancellation, foreign events and file/device loss without rearming or
freeing uncertain backing. Keep CPU resource construction separate from MMIO
authority. Test those joined paths offline before installing anything.

The next physical test remains one ordinary 1280×800 clear and transformed draw,
with independent COLOR/Z and guard checks after retirement, exact device/module
identity and stock boot recovery. A successful bounded draw would still leave
continuous execution, multiple resources/states, normal Mesa runtime loading,
X11 desktop integration, installation/removal and broader GL coverage to qualify.
