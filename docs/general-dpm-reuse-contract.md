# Carrying qualified DPM reuse into the general executor

DPM pool reuse is already present in the restricted physical FAST driver.
The missing work is its integration with general scene jobs, not a new DPM
encoder or another first OpenGL draw. The earlier physical
[`glDrawArrays`](phase9/OPENGL-FIRST-TRIANGLE-20261021.md), GLXGears and native
OpenGL results remain valid within their recorded scopes.

## Existing physical donor

The [600-frame original GLXGears run](phase9/PHYSICAL-GLX-STATUS-20261027.md)
used the retained private module from
`/home/gama/sgx535-offline/gears-color-readonly-fd-mayshare-20261026T232500Z/module`.
The original build record pins its module SHA256 to
`98e7f823acf50b7af4707d964a27c54f6267af8a825408b60d27911dd8c2abc8`;
the runtime module note is
`d5279b212cf7d7da4477261996f8b15e02dad534aa3a37614dc7b2772b2b83c2`.
Its completed production record is
`/home/gama/sgx535-offline/gears-fast-gem-fd-original-glx-20261027T013000Z/hardware-glx/PHYSICAL_PRODUCTION_GLX_RESULTS.json`.

In that module, `remesa_stream_trusted.c` calls
`remesa_gem_job_ta_policy(..., sequence > 3)` for FAST_NOHASH and skips the
two pool `memset`s for sequences greater than three. REGION is still cleared;
the absence of pool clears must not be generalized to REGION or attachments.
`remesa_gem_memory.c` provides the associated contract:

- The two pools have fixed addresses `0x407ef000` and `0x30100000`, each32MiB.
  Stream admission seals them against user writes and writable CPU leases.
- Every prepared job still validates the real eleven objects, generations,
  mappings, exclusive holds and absence of conflicting leases.
- CPU-cached, CPU-dirty and GPU-dirty state are separate. An actual CPU read
  returns ownership through `ta_for_cpu`; warm reuse does not pretend that
  GPU-written memory is pristine zero memory.
- Prefire flushes remain required when the CPU has touched a pool. Warm GPU
  ownership avoids redundant pool flushes. Barriers, completion, source
  attribution, retirement and uncertain-failure retention remain in place.

The physical record reports600 GPU submissions and a clean close, not600
archived individual receipts. Its separate LAB pixel qualification is preserved.
This is a qualified donor for that restricted workload, not a proof of arbitrary
pool contents, capacity, scenes, OOM recovery or general GL conformance.

## Verified construction equivalence

`tools/remesa/test_dpm_retained.py` compiles the actual archived
`remesa_stream_contract.c` with its original private namespace, headers and
depth profile. A separate translation unit calls the current
`remesa_lab_dpm_actions_encode`. It compares all29 complete actions for16
placements, including the original physical addresses. Native, UBSan and
static i486/QEMU runs agree byte for byte. The four-load completion mask15,
INIT kick and INITEND wait/acknowledgement/clear are included.

Evidence:
`/home/gama/sgx535-offline/general-dpm-retained-20261102/v1/compare-v2/RESULT.json`.
The runner creates a new output directory, leaves the archive untouched and
opens no device. Its first i486 invocation failed because a768MiB virtual
address limit prevented QEMU from reserving its guest address space. The
corrected runner uses the existing6GiB QEMU limit; the failed log remains in
`compare/i486-run.log`. This was not an SGX execution or encoder failure.

This comparison establishes that the general constructor needs no replacement.
It does not transfer the donor's ownership/cache rules into the new executor.

## Actual integration differences

| Contract | Restricted physical FAST | Current general scene executor |
| --- | --- | --- |
|29-action DPM load | Physically used, including warm frames | Same CPU construction; finite cold physical API cases passed |
| Pool identity | Sealed fixed eleven-role stream | Explicit packet IDs and narrowed private mappings |
| Between-frame writes | Pool write exclusion for stream lifetime | Explicit general-ID pool seal implemented; kernel-only, not physically qualified |
| Warm admission | Typed `ta_reuse` policy after initial three frames | Separate prior-retirement claim in trusted session; ordinary cold preparation still refuses reused pools |
| Cache ownership | Tracks CPU cached/dirty and GPU dirty separately | Same tracking integrated for sealed pool IDs; offline checked, not physically qualified |
| REGION preparation | Separate per-frame clear | Actual page overwrite under exclusive holds in trusted session; ordinary cold requirement preserved |
| Source lifetime | Retained stream | Existing source-session adapter, not a new user endpoint |

The [general pool write exclusion](general-dpm-write-exclusion.md) now carries
the first part of the donor contract using explicit IDs and generations.
Its [cache ownership](general-dpm-cache-ownership.md) is now integrated too.
The [warm claim](general-scene-warm-session.md) is now a separate capability backed by successful
prior retirement under that same lease. It must not set `zero_pristine=true`,
accept arbitrary modified pools, reinterpret the eleven-role ABI or clear64MiB
per frame. CPU access, source/file/device loss and ambiguous execution must
invalidate or terminate the warm capability. REGION initialization remains a
separate operation; its reserved size does not identify the bytes consumed.

The first bounded physical qualification should preserve the donor's three
cold submissions followed by a warm fourth submission, with no change to the
existing one-shot gates. Validate both1280×800 and640×640, identity, attributable
completion/retirement, COLOR and defined Z24 readback, inputs and guards, and
stock recovery. The new general session needs its own explicit admission;
rearming a consumed probe or clearing source lifetime flags is not that
admission. Until implementation and offline checks pass, the existing cold
refusal remains effective and no new physical run is authorized by this test.

No new GPU operation, FPS measurement, driver installation or OpenGL2.1/GLES2
qualification is claimed here.
