# Cache ownership of sealed general DPM pools

The general executor now uses the qualified FAST donor's separate CPU-cached,
CPU-dirty and GPU-dirty tracking for its two
[sealed DPM pool generations](general-dpm-write-exclusion.md). It uses the
existing backing flags and CPU lease transfer; it adds no shader, DPM encoder,
register sequence, diagnostic hash, readback or memory clear.

The private staging profile extends `ta_domain` to recognize these explicit
IDs, independent of the retained fixed eleven-role stream. The classification
requires the live generation and complete pinned32MiB storage. This is cache
ownership bookkeeping, not memory/job validation or warm initialization.

## Transfers

The existing private publisher still authenticates the complete loan, narrowed
mappings, permissions, views and job state before the first cache side effect.
For a sealed pool, it flushes when CPU-cached or CPU-dirty, clears those flags
and marks possible GPU writes before device use. It preserves `dma_wmb` and
consumes `zero_pristine` as before. Other bindings retain their unconditional
publication maintenance.

After the general executor authenticates retirement and the complete event
ledger, it keeps the pools in GPU ownership. COLOR, Z, REGION and other GPU
write roles retain their previous CPU visibility maintenance. All source,
root restoration, fence, retirement and error checks remain in place.

An actual read-only CPU lease calls the retained `ta_for_cpu` before accessing
the data. GPU-dirty data is flushed with the existing read barrier; the lease
then records CPU-cached state. Its next GPU consumer flushes that pool again.
Two simultaneous read-only pool leases work independently. Writable leases,
WRITE and explicit user SYNC remain blocked by the pool seal.

Final session release first validates complete idle metadata, source closure,
absence of jobs/leases/quarantine and both pool identities. It returns any
GPU-dirty pools to CPU ownership **before** clearing the IDs. Without that
transfer, a later unsealed CPU lease would lose the domain classification that
performs its invalidation. Failed release leaves the seal and cache state
intact. Failed execution retains the job/root and GPU-dirty ownership; no CPU
read, unseal, retry or cleanup is synthesized.

Only the staged private memory implementation defines
`REMESA_PRIVATE_DPM_CACHE`. Builds without this implementation retain the
previous unconditional publisher/completion behavior. The normal unsealed
general and retained eleven-role paths keep their existing behavior as well.

## Verification and limits

Evidence root: `/home/gama/sgx535-offline/general-dpm-cache-20261102/v1`.

- `tests-final/RESULT.json`:1118 checks per native/UBSan/statici486 variant,
  including787 existing private-loan checks. The actual private validator and
  publisher run against full modeled backing. Bad views and injected MMU
  validation failure produce no cache side effect; duplicate publication is
  refused. Pool publication flush counts are2 initially,1 after reading one,
  0 when untouched, and2 after reading both. Final unseal transfers both.
  Other bindings still flush; no extra page clears occur.
- `tests-seals/RESULT.json`:330 existing actual memory/seal/loss checks per
  variant still pass with the new cache implementation compiled in.
- `scene-replay-v2/RESULT.json`: the actual exported OpenGL/native1280×800 and
  640×640 input bytes pass current memory/closure/service/executor code in all
  three CPU variants. The sealed cold job performs21 cache-flush calls versus
  the conservative23, omitting only the two post-retirement pool transfers.
  A second scene preparation still returns `-ENODATA` with no second backend
  stage or extra flush. Final unseal transfers the two pools.
- `scene-cache-failures/RESULT.json`:36 modeled postpublication failures at
  both sizes cover unknown event, source finish, backend end, root restore,
  source unbind and source settled. GPU-dirty flags, real modeled job holds,
  quarantine and root retention survive. Read leases and all releases refuse;
  the test process exits with deliberately retained model resources.
- `kbuild/RESULT.json`: full antiX5.10.240 i486 module builds with no new
  warning messages. Candidate `module/gma500_gfx.ko` SHA256
  `d056d3b42ce32ae424c827404cd9ed05181a27b7c2fc88acb851f2751db5b037`.
  It has not been installed or physically qualified.

The first focused test compile lacked the Mesa2D include path; its failed log
is retained in `tests/`. The first full scene replay omitted the existing scene
validator footer from its generated memory fixture; its failed log remains in
`scene-replay/`. The corrected fixtures use the existing footer and constructors;
no production workaround or encoder change was made.

These are operation counts with injected cache/MMU/backend primitives, not
physical timing, GPU output or FPS measurements. The generic publication jobs
used to exercise ownership do not acquire warm scene admission. Cache-resident
data and a GPU-dirty bit are not retirement credentials: publication sets that
bit before possible use, including failing attempts.

The remaining continuous-rendering contract is a separate warm DPM claim tied
to successful prior retirement under the same protected source/backing lease,
plus separate REGION preparation and bounded user transport. It must not
relabel `zero_pristine`, treat reserved bytes as consumed data, omit the load
sequence or rearm a consumed LAB/source gate. A separately admitted physical
sequence at both sizes is still required before enabling or claiming this
optimization in the installable driver.

Focused reproduction opens no device:

```sh
python3 tools/remesa/test_private_dpm_cache.py \
  --native RETAINED_NATIVE_DIRECTORY --shim INDEXED_TEST_GEM_SHIM \
  --output NEW_DIRECTORY
```

Pair `--i486-cc` and `--qemu-i386` for the i486 model. The actual general-scene
replay and failure scripts are retained in the evidence root. Full GL2.1/GLES2,
desktop2D and complete installation remain unfinished.
