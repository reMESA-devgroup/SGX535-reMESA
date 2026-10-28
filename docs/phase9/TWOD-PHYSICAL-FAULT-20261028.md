# Native 2D physical fault and recovered boot

The first1280x800 copy reached the physical SGX535 core/revision, but failed
the post-write hardware check. Completion, retirement, readback and buffer
integrity were not approved. No640x640/odd-size retry followed this failure.

Evidence: `/home/gama/sgx535-offline/twod-lab-20261027/v2/hardware/`, especially
PHYSICAL_RESULTS.json, 1280x800-evidence.tar.gz and artifacts-1280x800/.

| Observation | Result |
| --- | --- |
| LAB boot | `33af8664-d802-48e2-8fcc-0633453bdb2d` |
| PCI / observed core / revision | 8086:8108 / `0x01130000` / `0x00010201` |
| Module SHA256 | `3fd9f60568a797e7b01f031423cab067150c93c9d9149a8073795c128f5ed5fa` |
| Operation / cookie | 1 / 1 |
| Outcome | error-5, stages1, retained1, uncertain1 |
| Bound code path | possible-issue cookie, ten FIFO write calls, post-write EIO |
| IRQ log | MMU page fault, label `TA requestor`, address `0x40000000` |
| Source BO | `[0x40001000,0x403ef000)` |
| Destination BO | `[0x41000000,0x413ee000)` |
| Readbacks | zero bytes; refused on uncertainty |

The fault is outside both held BOs. The requester name is from the generic
gma500 IRQ decoder; raw BIF status was not exported. It does not establish an
instruction-level cause or prove a different requester-bit interpretation.
Issue took7.649576ms until failure; client wall time233.473265ms included
allocation/upload. Neither is GPU execution time or an FPS result.

## Preserved evidence and recovery

The driver retained backing/context. All four controls returned toN, producer
count returned to0 and source reasons33 recorded pending/interference. The
original boot configuration was restored before the GPU attempt; next_entry
was empty. After fetching the complete failure artifacts, one default recovery
reboot ended the failed driver lifetime. This reset is not GPU retirement.

FINAL_REMOTE_STATE.json records boot `2ffb7421-98a0-4730-bfff-ce2964a77b05`,
stock module note, no experimental controls/observer, original custom.cfg hash
and empty next_entry. A fresh read-only check after resume confirms the same
state in `twod-fault-stage-20261028/RECOVERY_REVALIDATED.json`. The temporary
menu entry was removed; the image is an unselected artifact. No experimental
job or one-shot boot remains queued. Historical gears evidence remains intact.

The permitted, pinned [SGX535 register header](https://github.com/EMGD-Community/intel-binaries-linux/blob/e6884ec2eaaf1afe88d5ff9dd44d70403525be5b/drm/pvr/services4/srvkm/hwdefs/sgx535defs.h)
agrees with the base/FIFO/blitter offsets. Local provenance is retained at
`/home/gama/sgx535-offline/phase8-independent-root-evidence-20261006T023140Z/independent-primary-manifest.json`.
This corroborates offsets, not the complete physical fetch contract. No
excluded PSP2 source was used. Matching the public kernel packet does not
prove that the GPU accesses only the declared rectangles.

Do not map an extra base page, clear faults, change requester masks or use a
CPU copy to silence the error. Uninitialized surface state, virtual-FIFO/base
publication and packet interpretation remain hypotheses. Locate the fault
first rather than changing several of these at once.

## Next bounded diagnostic

The native start now checks hardware immediately after base publication and
before FIFO issuance. Source-producer notification begins before that base
write and is balanced on every later error, preserving legacy provenance even
when base publication fails before any packet word.

Only the LAB build captures one failed-start snapshot of raw event/BIF/fault,
directory/request bases, virtual-FIFO controls and FIFO/blitter status. Capture
uses the IRQ interval; printing happens after both spinlocks are released.
Rejected core/revision or lost power causes no extra snapshot MMIO reads.
Nothing clears the fault or changes completion authority.

| Phase | Observation point |
| --- | --- |
| 1 | identity/power |
| 2 | initial hardware check |
| 3 | stale-event boundary |
| 4 | base write/readback |
| 5 | post-base check, before FIFO |
| 6 | cookie/IRQ association |
| 7 | FIFO writes and posted read |
| 8 | final hardware check |

Phase5 with zero words proves this start sent no FIFO packet. Phase8 only says
the earlier check passed; an asynchronous fault still cannot be attributed to
an individual DWORD without further evidence.

Current offline candidate: `/home/gama/sgx535-offline/twod-fault-stage-20261028/v2/`.
Native/UBSan/i386 each pass3147 parent checks plus21 fault children, including
fault-on-base-write, zero FIFO words, raw fault capture and logging after
unlock. Existing root/IRQ/endpoint results remain preserved. Native objects,
partial link and full antiX module build pass. PRODUCTION_CHECK.json verifies
that the LAB failure-log string is absent in the production object. This new
candidate has not been booted or physically approved.

Use a fresh image/controller bound to the new module and a new boot inventory;
the recovered stock driver lacks the custom /proc interfaces. Preserve gates,
holds and rollback. Only a correct1280x800 result permits the paired640x640
and occasional641x479 cases. General3D/2D/userspace installation remains active
and unfinished.
