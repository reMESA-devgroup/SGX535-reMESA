# Restricted physical GLX status — October 27, 2026

The original GLXGears completed a bounded **600-frame / 600-GPU-submission**
run at **1280×800** on the Dell Mini 12, SGX535 rev121. The six steady
five-second stdout windows averaged **16.1345 FPS**, ranging from **15.915
to 16.376 FPS**. The reciprocal of that mean is **61.978989 ms/frame**.
The initial 5.1-second warmup window reported 14.232 FPS and is excluded
from the steady mean.

The prior qualified G snapshot run averaged 14.4982857 FPS. The observed
aggregate change is **+11.28557% FPS / −6.99468 ms per frame**. This comparison
combines the cached-Z/fault64 kernel, direct read-only GEM FD/XSHM presentation
and paired GC work. CPU DVFS and workload were uncontrolled; these numbers
do not establish per-feature causality or pure GPU cycle time.

## Application measurement and final state

The completed run reported `frames=600`, `GPU_submissions=600`, `queued=0`,
`legacy_submits=0`, `error=0`, `offline=0`, application exit 0 and
`presentation_error=0`. Capture was empty: no per-frame GPU receipts, raw
frames, hashes, pixel-oracle checks, diagnostic checkpoints or dumps were
archived by the normal FAST run. Frame counts and successful teardown are
aggregate evidence; pixel qualification comes from the separate physical tests.

The final audit for this completed run recorded all three controls N/N/N,
stock future boot restored, source guard quiet, and owned client and supervisor
absent. This is a statement about that run's recorded terminal state. It does
not report a subsequently requested continuous onscreen session as running.

The original client SHA-256 is
`159aa90de37785038490ff51bb5d7de86f37d81c129fc8090e13f6687713dc37`.
The tested private libGL/bootstrap pair is recorded in the build evidence:

| Artifact | SHA-256 |
| --- | --- |
| libGL | `7228fdb00b9d858784b433222a03cd241c2cf60652be1818c90a69df109cc37b` |
| Bootstrap | `08eda4f10995a21c086d1a6d371259d4d028900432196e957140768f7ccdd5d8` |

These identities refer to the isolated qualified build. This documentation
does not imply every implementation in the integration tree is that build.

## Separate production-transport physical smoke test

Before the original application run, six physical GPU operations exercised
the actual new production transport/helper/publisher through a separate LAB
harness. Every pose matched full S-golden COLOR, all defined Z24 bits and all
five input images, and X11 matched all 1,024,000 RGB pixels. Z's upper X8 bits
are VOID; full raw Z byte equality is not claimed.

The same cached read-only FD and X attachment survived six distinct kernel
READ leases, with full revocation/remapping between them. Matching X completion
preceded UNLEASE; checked X detach preceded FD/GEM context close. Final GEM
INFO was zero. Retirement phase 9, ledger 7 and proof 111 remained intact.
Input/Z/COLOR readbacks and XGetImage belonged to the separate LAB harness,
outside the normal production loop. Smoke timings include those reads and
cold setup; they are not application FPS measurements.

The prior nine negative kernel gates remain passed in the MAYSHARE physical
qualification. The production smoke is not a replacement full negative-journal
audit and does not confer LAB approval. The latest direct-FD integration made
no kernel, shader, geometry or GPU asset changes relative to its qualified
parent. Existing address/PFN, permission, generation, ownership, coherence,
fence, retirement and error guards remain part of the interface contract.

## Orientation and copy scope

The qualified G snapshot path already presents GPU raw COLOR in the correct
order: `frame_into(bottom_left=false)` copies raw COLOR without row reversal,
and snapshot presentation adds no flip. The earlier handoff assumption that
this path needed a bottom-up conversion was incorrect. The source chain and
subsequent retired-GPU-FD-to-X11 physical RGB test establish this correction;
inverted-order expectations were rejected. No shader or geometry correction
was needed.

Direct FD presentation removes the userspace COLOR readback/snapshot copy in
the normal qualified path. X server presentation still copies pixels; this
is not an end-to-end zero-copy claim. GPU reuse remains blocked by an active
presentation lease. Restricted FAST may reject an old HOST read with `ESTALE`
after a different GPU target overwrites shared COLOR, rather than return stale
HOST bytes. That restriction is incompatible with claiming general Mesa/GL
conformance from this workload alone.

## Evidence and limits

All source evidence below is retained locally under
`/home/gama/sgx535-offline/`; existing raw evidence, failed attempts, checkpoints,
oracles and hashes remain separate and unchanged.

| Evidence directory | Authoritative records |
| --- | --- |
| `gears-fast-gem-fd-20261027T001500Z/` | `BUILD_RESULTS.json`, `OFFLINE_SUITE_RESULTS.json`, `PHYSICAL_SMOKE_QUALIFICATION.json` |
| `gears-fast-gem-fd-original-glx-20261027T013000Z/` | `PERFORMANCE_RESULTS.json`, `hardware-glx/PHYSICAL_PRODUCTION_GLX_RESULTS.json`, `hardware-glx/FINAL_REMOTE_STATE.json`, `hardware-glx/ended/RESULT.json` |
| `gears-retired-gpu-fd-x11-20261027T000000Z/` | `QUALIFIED_ORIENTATION_CHAIN.json`, `PHYSICAL_QUALIFICATION.json` |
| `gears-color-readonly-fd-mayshare-20261026T232500Z/` | `PHYSICAL_QUALIFICATION.json` |

The build and ten-report offline suite performed zero GPU operations; their
original pre-physical status fields are historical and do not supersede the
later physical records. CPU mocks and Xvfb results are not GPU rendering proof.

The **30–60 FPS goal remains open**. This bounded run does not qualify unbounded
continuity, full GL shading/precision, a supported OpenGL version, general
application compatibility, every NIR/VS/PDS candidate, or LAB approval. Printed
GL version/extension strings do not establish conformance. The older October 14
and structural-pipeline notes remain historical implementation records; the
October 26 checkpoint preserves its own earlier measured result and sources.
