# Private/native full-output regression on the SGX535 rev121

The unchanged module built from `2748885` passed a physical native/private
regression on the Dell Mini 12 at 1280x800. With the archived native pose1 inputs,
all 4,096,000 COLOR bytes and all 1,024,000 defined Z24 values match the archived
native-PD output exactly. This establishes one private-PD fixed-profile case;
it does not establish a reusable context or general OpenGL/GLES conformance.

## Input and acceptance contract

Evidence root:
`/home/gama/sgx535-offline/private-native-pose-regression-20261029/v1/`.
The baseline is the physical capture under
`/home/gama/sgx535-offline/gears-depth-less-single-clear-20261026T160400Z/hardware/replay-ended6/capture/frame-000001-*`.
Its receipt records phase 9, ledger 7 and sequence/operation/generation 1.
The previous native diagnostic's conformance limitations remain unchanged.

`BINDING_PREPARATION.json` and `EQUALITY_CONTRACT.json` pin the baseline,
inputs and acceptance rules before execution. Roles 1 through 4 are identical
to the first private probe. The PDS differs by 156 data bytes; the actual existing
C `remesa_stream_mask_sources` implementation proves that the whole normalized
128 KiB PDS is identical. The profile changes only the raw role0 input digest.
Shader CODE, geometry, bindings, commands and the kernel module are unchanged.

Acceptance requires exact full COLOR and defined Z24 equality, with no edge mask
or added tolerance. The existing Z24X8 contract ignores X8; 200,000 X8 bytes do
differ and the original raw Z files and hashes are preserved. This is not full
raw-Z equality. Offline controls accept the exact baseline, reject a single
changed COLOR byte and a single changed Z24 byte, and ignore only X8 changes.
The comparison does not use the CPU analytical image as expected pixels.

## Physical result

Boot: `dfa483ea-bde5-45e4-8eaf-a09828971a5e`.
Kernel: `5.10.240-antix.1-486-smp`.
Module SHA-256:
`527cbbf84e59b1878c4bccea10ba889d191fd2878cdf72d1b3c2d1c17dfdf4a1`.

| Observation | Result |
| --- | --- |
| Actual SGX core / revision | `0x01130000` / `0x00010201` (SGX535 rev121) |
| Probe calls / operation / cookie | 1 / 1 / 1 |
| Retirement phase / ledger / stages | 9 / 7 / 15 |
| Error / retained | 0 / 0 |
| COLOR differing bytes / pixels | 0 / 0 |
| Defined Z24 differing pixels | 0 |
| Undefined X8 differing bytes | 200,000 |
| Five full input readbacks | Exact |
| Two 4 KiB guard pages | Intact |
| Explicit memory cleanup | Complete |

COLOR SHA-256 (also the native baseline):
`807b5466e2fc57363030e91b99820c8b6c33845420271be0ddc5102f13abd311`.
Raw private Z SHA-256:
`ac389faad63810b7a61cce4fa129b4bdd0bf362f93df5fb4c800d540cb747cb0`.

Preparation/upload took 0.354439 s, the probe ioctl 0.344051 s and readback
0.535118 s. These separate LAB wall times include CPU/kernel/cache/MMU work;
they are not isolated GPU execution times or continuous rendering FPS. No hashes,
readbacks or comparisons were added to production FAST.

`physical/capture/REQUEST.bin` and `RESPONSE.bin` were independently reparsed.
`NATIVE_COMPARISON.json` records the full-output comparison and negative controls.
The original collector remains `ORACLE_PENDING` and
`hardware_driver_qualified=false`; the separate regression result does not
retroactively promote it into complete LAB or GL approval.

Before the physical operation, the fresh client tests passed, all 36 actual
controller scenarios passed with UBSan and all 31 protected files were unchanged.
`hardware/BOOT_SMOKE_RESULT.json` binds the fresh boot to both loaded module
notes and the first-load PASS. `FINAL_REMOTE_STATE.json` confirms the collector
is terminal, cleanup complete, the module live and all six controls N. The stock
saved default, empty next entry and original custom.cfg hash were restored before
execution and remained restored afterward. No remove/unbind/S3/reset test ran.
The captured log has no kernel fault markers; the BL message and two ACPI
PowerButton warnings also occur in the previous boot and remain recorded in
`KERNEL_LOG_REVIEW.json` and the full raw log.

This new boot is consumed too. Do not clear either one-shot guard or repeat the
probe on it. Keep the first probe's [analytical discrepancies](private-first-physical.md)
and all historical evidence intact.

## Next integration boundary

The production path needs a persistent file-owned render session with reusable
resources and fresh per-operation receipts. The existing source protocol permits
one lease lifetime with distinct bound observations; it does not permit
reclaiming a released lease. Keep the one-shot LAB ioctl unchanged. A reusable
path must retain its source ownership across jobs, establish a settled boundary
between jobs and retain/quarantine resources after uncertain execution. Merely
clearing `private_probe_consumed` or the source's `ever_claimed` flag would violate
that contract. This regression is the input/output baseline for that next work,
not an implementation or qualification of the reusable path.
