# Bound gears scene: physical integration, 2026-11-02

The bound Gallium/NIR gears vertex path now has physical evidence through the
common scene executor on the Dell Mini 12, SGX535 rev121. These are two single
LAB scenes in separate fresh N/N boots, not an OpenGL version or FPS claim.

| Target | Profile | COLOR mismatches | Z24 mismatches | Retirement |
| --- | --- | --- | --- | --- |
| 1280×800 | version 2 / 1 | 0 | 0 | phase 9, ledger 7 |
| 640×640 | version 2 / 2 | 0 | 0 | phase 9, ledger 7 |

Each scene used actual bound NIR, raw position/normal input, a constant-buffer
matrix and lighting parameters, COLOR/Z attachments and the Linux/GEM
submission callback. The GPU produced the expected transformed triangle:
118 white pixels. No CPU-transformed vertices or CPU-rendered image were
uploaded. The independent pixel expectations were sealed before execution.

Both runs confirmed core ID `0x01130000`, revision `0x00010201`, completion
status samples, retirement, all 66 control words, 13 immutable READ buffers,
three CPU-only/source buffers, four guards and complete resource cleanup.
The source lifecycle closed and all seven controls returned to N.

The 1280 run returned to a verified stock kernel/module boot. After the 640
run, a local thunderstorm prompted closure without another reboot: collection
and audit finished, the fallback reboot timer was cancelled, and the stock
boot configuration was verified restored. The test module remained loaded
with controls N; a subsequent stock module boot has not yet been observed.
No GPU operation remains pending.

Evidence and hashes:

- `/home/gama/sgx535-offline/gears-pixel-oracles-20261102/FINAL_PAIR_RESULT.json`
- `v1/FINAL_PHYSICAL_RESULT.json`, boot `545d694f-c594-42c6-ac4c-ef04779b05b4`
- `v2-640/PHYSICAL_CLOSURE_RESULT.json`, boot `86e5accb-fc50-46fc-9a62-bdd8034b7781`
- Each directory retains the raw capture, prelaunch seals, auditor invocation
  and `PHYSICAL_FILES.json` with hashes of the collected files.

The initial image transfer failed during a network outage and was retried only
after read-only confirmation that no complete or partial image existed. A
640 boot-plan directory collision was refused before changing GRUB; its failed
scripts/results are retained separately. The corrected plan used a distinct
root and boot entry. Neither refusal consumed a GPU attempt.

This closes the physical boundary for the selected bound gears scene. Normal
OpenGL draw routing, general shader/texture/state coverage, continuous native
sessions and the installable complete driver still need work. Existing
GLXGears/FAST paths and proven encoders were preserved. LAB hashes, snapshots
and oracle comparisons were not introduced into FAST.
