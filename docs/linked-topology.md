# Linked vertex primitive assembly

The linked vertex input snapshot accepts triangle lists, triangle strips and
triangle fans. Strips and fans are expanded into triangle-list indices before
the existing geometry owner and TA encoder. Attributes remain byte-exact;
this performs no vertex transformation, lighting, clipping or rasterization.

The context snapshot obtains first/last provoking convention from the live
bound rasterizer. Strip parity is relative to the submitted draw, including
nonzero starts. A fan's first provoking vertex is the next rim vertex, not
its anchor. Degenerate triangles are retained. The original standalone
triangle-list constructor remains triangle-only.

Input bounds use the consumed input count; index storage and TA count use the
expanded output count. The existing limits remain 65,535 output indices and
a source span of 4,095 vertex records. Indexed restart and instancing remain
unsupported. Overflow, stale resources, writer/lease conflicts and invalid
source slices still fail before producing an owned snapshot. No capability
or ordinary draw admission gate is enabled by this change.

## Validation

`tools/remesa/prepare_topology_oracle.py` generates six unchanged translators
from the pinned public Mesa 25.2.0 `u_indices_gen.py` (SHA-256
`9798646139782c2b28bdb37aedf4de4171a69a13739d3200d4d11eaf6afdffba`).
The test-only oracle is not a driver dependency.

Run the source suite with a fresh output directory:

```sh
python3 mesa/sgx535/tests/qualify_shader_program.py \
  --cache /home/gama/sgx535-offline/phase9-real-mesa-20261008 \
  --test test_linked_topology.c --source-only --output /absolute/new/test-dir
```

Evidence is under `/home/gama/sgx535-offline/linked-topology-20261030/v1`:

- `source-tests-final-v2/result.json`: native, UBSan and static i386 pass;
  288 topology cases plus existing linked-input and staging regressions.
- `actual-mesa-context-v2/RESULT.json`: real Gallium/NIR context, simulated
  resources/fences; map deduplication, writer conflicts, rollback and cleanup.
- `actual-mesa-topology-v3/RESULT.json`: private Mesa-model archive, linked
  geometry/PDS ownership and the existing TA encoder with simulated GEM.

The private archive changes only the draw and screen objects; its parent
archive is preserved. Earlier harness compile/link failures remain in the
same evidence root. These tests perform zero GPU operations and do not
qualify hardware rendering, performance or an installable general GL driver.
The authenticated generated-scene closure and native admission remain open.
