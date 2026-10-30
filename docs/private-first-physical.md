# First controlled private-PD GPU execution on the Mini 12

The candidate built from `2748885` completed one physical fixed-profile operation
using its private MMU directory, returned a validated completion/retirement
receipt, restored the provider mappings and released the operation. This is a
bounded bring-up result. General OpenGL/GLES, continuous GLXGears, removal/S3
qualification and a production installable driver are not established by it.

## Recorded result

Device: Dell Mini 12, kernel `5.10.240-antix.1-486-smp`.
Boot: `80e0a8c9-2051-4227-a626-a9438e2c072c`.
The actual controller checked and returned core `0x01130000`, revision
`0x00010201` (SGX535 rev121), before accepting the operation.

| Field | Observation |
| --- | --- |
| Probe ioctl calls | 1 |
| Operation / cookie | 1 / 1 |
| Retirement phase / event ledger | 9 / 7 |
| Controller stages | 15: prepared, isolated, started, retired |
| Controller error / retained flag | 0 / 0 |
| COLOR and Z readbacks | 4,096,000 bytes each, 1280x800 |
| Nonzero COLOR / changed Z24 pixels | 418,394 / 418,394 |
| Readonly/LOCAL inputs compared | All five full images byte-identical |
| Guard pages | Both full 4 KiB canaries unchanged |
| Explicit memory cleanup | Complete |

The client checks restored provider mappings and absence of retained jobs,
leases and quarantine before allowing cleanup. The controller's success stages
are reached only after the existing execution, retirement, source-close,
default-root restoration and release gates. This is not a manually fabricated
phase, CPU rasterization or X11 presentation test. The retained profile contains
twelve gear draws; it is not a running original GLXGears process.

COLOR SHA-256:
`4fade472746990885cc4e40054b5d1592d01c5dcb5711af35e9a69d48c062757`.
Raw Z SHA-256:
`cb09951726264cf2a6a3483ea44fa854e7c83e2198f0b07f6e751b067bfde9e0`.
No whole-frame pixel oracle is approved yet. A visual preview is decoded from
the actual GPU readback; it is not independent mathematical validation. Earlier
pose-dependent/diagnostic references must not be relabeled as a golden oracle.

Measured walls were 0.346097 s for preparation/upload, 0.413999 s for the probe
ioctl and 0.493499 s for readback. The ioctl includes CPU preparation, cache and
MMU work, synchronization and GPU execution. These are neither isolated GPU
execution times nor sustained FPS. Diagnostic hashes/readback remain LAB costs.

## Evidence and limits

Root: `/home/gama/sgx535-offline/private-first-physical-20261029/v1/`.

- `physical/capture/REQUEST.bin`, `RESPONSE.bin`, `RESULT.json`, full input,
  COLOR/Z and guard readbacks; original collector status is
  `RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING` and
  `hardware_driver_qualified=false`.
- `INDEPENDENT_RECEIPT_READBACK_REVIEW.json`: local receipt reparse, full input
  and guard comparisons and readback size/hash verification, separate from the
  client's report. Pixel approval remains false.
- `physical-evidence.tar`: exact retrieved archive, SHA-256
  `79fa119129e7a68b994182f45d5215e452b3aa6aea96a2501b33d036d8ec3a84`.
- `PHYSICAL-COLOR-RGB-1280x800.png`, `PREVIEW_PROVENANCE.json`: decoded RGB
  channels, alpha omitted for viewing only; original BGRA bytes preserved.
- `PREEXECUTION_REVIEW.json`, `PAYLOAD_MANIFEST.json`, actual controller tests
  and client test logs: bounded normal execution under the new retention design,
  not destructive remove/unbind/S3 tests. Historical audit remains preserved.
- `FINAL_REMOTE_STATE.json`: collector terminal, six controls N, explicit cleanup
  complete, stock saved default and empty next entry, original custom.cfg hash
  restored. No GPU operation is left pending by this test.

The module SHA-256 is
`527cbbf84e59b1878c4bccea10ba889d191fd2878cdf72d1b3c2d1c17dfdf4a1`.
Its loaded build-id note matched the earlier N-only physical boot. Only memory
and private-probe admission were temporarily enabled; other execution paths
remained N. All six controls were returned to N after the collector terminated.
The OS remains on the candidate, with the stock boot configuration restored.

This boot is consumed for the one-shot private probe. The legacy
`/proc/sgx535_source_guard` can still display its independent startup evidence;
that does not authorize another operation or prove a fresh private source lease.
Do not retry the probe on this boot. This result does not upgrade the original
LAB capsule contract or demonstrate all negative MMU permissions, complete
driver safety, S3, surprise removal, general rendering or performance targets.
