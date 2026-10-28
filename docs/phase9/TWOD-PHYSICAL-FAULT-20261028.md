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
that the LAB failure-log string is absent in the production object. At the time of that offline result the candidate had not been booted.
The subsequent physical diagnostic below remains rejected.

Use a fresh image/controller bound to the new module and a new boot inventory;
the recovered stock driver lacks the custom /proc interfaces. Preserve gates,
holds and rollback. Only a correct1280x800 result permits the paired640x640
and occasional641x479 cases. General3D/2D/userspace installation remains active
and unfinished.


## Second physical attempt: diagnostic phase8, raw BIF fault

The diagnostic module from commit838a6cb was booted exactly once with the new
image SHA256 `8d82e368ac19315002460dac7b6cf7d8c415fbd3b2a91b02f5b496a4c3a41107`.
Evidence: `/home/gama/sgx535-offline/twod-fault-stage-20261028/v2/hardware/`,
including PHYSICAL_RESULTS.json, CLOSEOUT.json, FINAL_REMOTE_STATE.json and
artifacts-1280x800/{RESULT.json,dmesg.before,dmesg.after,stdout,stderr}.
The archive1280x800-evidence.tar.gz has SHA256
`9f4662b4f82637ce5b54815d012f88b2bdc06be3fd1c7ed110863e89707b4874`.

Fresh stock inventory, five offline init identity tests, generated-controller
syntax checks, staged-image hash and grub-script-check passed before boot.
The stock-compatible controller requires the exact stock note, absence of the
observer/custom proc interfaces and empty experimental controls. It does not
call missing custom interfaces or mistake absent controls for N. The new
module and observer notes, virgin source prerequisites and four N controls
were checked on boot `9665ecff-c945-49b4-aed3-f4b6c946816d`. The original GRUB
configuration and empty next_entry were restored before the copy.

One1280x800 request reached core `0x01130000`, revision `0x00010201`,
operation1/cookie1. It failed with error-5/stages1/retained1/uncertain1.
The new failure line, captured before the legacy IRQ logger, is:

```
phase=8 words=10 valid=1 event=80000000/00000010 bif=00004004
addr=40000000 root=349ec000 req=40000000
vfifo=00000000/00000000 fifo=00000079 blit=01000000
```

This proves that the post-base/pre-FIFO hardware check returned success and
ten FIFO write calls followed before the final hardware check rejected the
operation. BIF bit14 reports a page fault; the original kernel names bit2
TA. The raw word is now preserved independently of that name. Both virtual
FIFO registers were zero at capture, including enable bit0. The faulting
address equals the request base and remains outside both held BOs. The
blitter was busy and FIFO was not empty. A single free-space sample does not
identify the instruction being processed or establish which DWORD faulted.
An asynchronous effect from base publication is still not ruled out.

Issue wall time to rejection was7.509615ms and client wall time230.415265ms;
these are not GPU execution time or FPS. Completion, visibility, retirement
and readback integrity remain unapproved. Both raw readback files are empty
because uncertainty refused reads. No640x640 or641x479 retry occurred.
All controls returned to N, producers_active0 and source reasons33. The
complete evidence was fetched before one default recovery reboot. That reset
ended the quarantined kernel lifetime; it is not retirement approval.

Recovery boot `cbb8f28e-ceca-4552-961c-a890edfa81e7` has the exact stock module
note, no observer or experimental controls/proc endpoint, original custom.cfg
SHA256 and original saved default/empty next_entry. No experimental job or
queued boot remains. Both failed experiment images remain unselected artifacts.
All31 protected historical artifacts remain unchanged.

The earlier "next bounded diagnostic" was executed; do not repeat this
consumed root `/root/sgx535-twod-lab-boot-8d82e368/copy-1280x800`.
The next minimum experiment should locate the earliest observed fault within
the unchanged packet, using bounded LAB-only per-word status observation and
stopping further writes on a detected fault. It needs offline fault injection
at every possible word boundary and a fresh isolated image/boot. The sample
must still be described as an observation boundary, not proof of the exact
causal DWORD. Preserve the complete packet, FENCE, protected address space,
retained ownership, production behavior and all existing gates. Do not map
the failing base page or change pattern/ROP state speculatively. No successful
2D acceleration, desktop performance gain or completed general driver is claimed.


## Next candidate: bounded LAB observation between packet words

The per-word observation is now implemented and qualified offline in
`/home/gama/sgx535-offline/twod-fault-stage-20261028/v3/`. After each unchanged
FIFO word, the LAB build checks power, raw BIF status and event-status2 without
ACK, reset or wait. Phase9 records the number of words written when a fault is
observed, immediately stops subsequent writes and retains the partial job.
A power loss prevents all subsequent MMIO including the failure snapshot.
It does not identify the causal word of an asynchronous fault.

Native/UBSan/i386 each pass3543 parent checks plus33 joined fault children.
New injections cover BIF faults after each of the ten words, event2 after
word3 and power loss after word3; tests require exact write cutoff, balanced
producer notification, retained ownership, no completion/visibility/reset and
logging after both locks are released. Power-loss checks also prove no later
MMIO reads. All14 focused compile/run/link commands pass with empty stderr.

The complete antiX module builds:736432bytes, SHA256
`9874b98b6fbd8be0633d02e29e2bf95f712fedf92ada00e85ee60968eba0ed76`.
Its21 warnings belong to byte-unchanged original files. The compiled5844byte
gears profile is byte-exact against the previous module. All31 protected
artifacts are intact. The complete non-LAB accel_2d .text is byte-identical to
v2 (SHA256 eb0eba9050870f0686de2c23a880dc2fb63e9ec9c9d55d4eb8ccca58ab0beeec).
No additional diagnostic cost is compiled into that production code.

This candidate is not installed, booted or physically approved. No v3 initrd
has been built. Build a fresh identity-bound image/controller if proceeding;
the v2 image/consumed hardware root contains the earlier phase8 candidate.
Current physical outcome is still the rejected v2 test and verified stock
recovery. General3D/2D/userspace installation remains unfinished.


## v3 physical result: rejection after only the FENCE word

The v3 image SHA256
`9dfb32c25fbc9e34ac6da070c75bc0af8a569a2353392d7c863365e4a31905a9`
(51,105,735 bytes) booted once as
`d9baa70e-b6fc-4e8a-a9b5-48dbcc8dae41`. Its module note matched the offline
build, all four controls were N and source/capsule prerequisites were virgin.
The original default/configuration and empty next_entry were restored before
one 1280x800 operation. No 640x640 or odd-size retry followed the fault.

Evidence: `/home/gama/sgx535-offline/twod-fault-stage-20261028/v3/hardware/`,
including PHYSICAL_RESULTS.json, CLOSEOUT.json and artifacts-1280x800/.
The fetched archive SHA256 is
`1d236edebd99e5654700d880cad865fd8b4dedcaa7aeb2f2dd381f690b86be2e`.
The consumed root is `/root/sgx535-twod-lab-boot-9dfb32c2/copy-1280x800`.

```
phase=9 words=1 valid=1 event=80000000/00000010 bif=00004004
addr=40000000 root=34a84000 req=40000000
vfifo=00000000/00000000 fifo=00000080 blit=01000000
```

Only word0 (`0x70000000`, the original FENCE header) was written. The driver
then observed the fault and stopped: no new source, destination, rectangle,
ROP/BLIT or FLUSH word was sent. Consequently the immediate fault does not
require this invocation's new surface or BLIT state. This still does not prove
that FENCE caused the fault: a delayed base-publication effect or preexisting
state remains possible. Do not remove FENCE or map the failing base page.

Core/revision were `0x01130000/0x00010201`, operation/cookie1/1;
error-5/stages1/retained1/uncertain1. No completion, visibility or retirement
was approved; readbacks are deliberately empty. Issue wall time to rejection
was7.466313ms, client wall241.523730ms; neither is GPU execution time or FPS.
Controls returned to N, producers_active0, source reasons33. The probe was
terminal before complete evidence was fetched. All31 protected artifacts remain
intact.

After fetching, exactly one default recovery reboot was requested. The first
read-only wait exhausted30 probes; SSH independently reported "No route to
host". No reboot was repeated and no next experiment was selected. Recovery
is not yet verified; do not claim the quarantined kernel lifetime ended.
The original stock saved default and empty next_entry were verified before
that request. The user subsequently reported Mini12 powered on; only read-only
recovery observation resumed. A successful exact-stock verification must
supersede this pending state before another hardware experiment.

## v4 offline candidate: separate base observation from the first FIFO word

A LAB-only requested1,000--1,500us wait now occurs after cookie association and
before the first FIFO write. Both spinlocks are released during the wait;
backing, reservation, source-producer interval and IRQ cookie stay live.
After reacquiring, phase10 rechecks power/root/base/quiet hardware and rejects
ANY raw or owned-ledger event before FIFO. An event already ACKed by IRQ must
still reject through the ledger. Nothing clears faults, creates a completion,
changes the packet or releases uncertain ownership. The physical duration of
this wait is not measured yet.

Offline evidence: `/home/gama/sgx535-offline/twod-fault-stage-20261028/v4/`.
Native/UBSan/i386 each pass3,900 parent checks plus38 joined fault children.
New cases cover a delayed BIF fault, power loss without subsequent MMIO,
pre-FIFO completion/fault already ACKed by IRQ, and root change during wait.
The model checks both locks are free, producer and IRQ reservation remain live
and another reservation is refused during sleep. All14 focused commands pass
with empty stderr. These CPU models submit no GPU work.

Complete module735,496bytes, SHA256
`d3903fcd5bc79784b6e595a573d167c6a3df900554629769bb9ea9aa42d3b1f6`.
Twenty-one warnings belong to unchanged original files. The compiled gears
profile remains byte-exact and all31 protected artifacts are unchanged.
Production accel_2d .text is byte-identical to both v2 and v3, SHA256
`eb0eba9050870f0686de2c23a880dc2fb63e9ec9c9d55d4eb8ccca58ab0beeec`:
the additional sleep, reads and ledger checks are compiled out of production.

The offline image is51,105,535bytes, SHA256
`62adb13237333f0b165097dc6f9b19279a6312c8c924dbf28d4c82bbc2e07f38`;
only the four identity/module members changed, with five identity-chain tests
passing. READY_OFFLINE.json records the prerequisite of fresh stock recovery
and inventory. At that offline checkpoint it had not been staged or booted; the subsequent
physical result below supersedes that deployment status. No physical success,
2D performance gain or completed installable driver is claimed.


The v3 recovery subsequently verified after the user restored connectivity:
stock boot `058f1193-bb56-460e-9cd2-c29362832169`, exact stock module note,
no observer/experimental controls/proc endpoint, original saved default and
custom.cfg hash, empty next_entry. FINAL_REMOTE_STATE.json and the updated
hardware/CLOSEOUT.json supersede the pending recovery state above; the initial
unverified snapshot is preserved separately. The read-only wait was resumed
after the user's new powered-on report; no additional reboot or GPU operation
was requested. This ends the quarantined kernel lifetime by reset, not by
retirement. Both observation controllers are terminal.


## v4 physical result: pre-FIFO observation passes, FENCE still faults

One v4 boot `127cbff1-ae98-4c7f-ac7e-da11239ab072` matched the bound driver
note `9badbb08ff359cb1a919d8199219a22260344fb712ca583aa70873b3297b1989`
and observer, all four N controls and virgin source/capsule prerequisites.
Fresh stock inventory, identity-chain tests, controller syntax, staged-image
hash and grub-script-check passed before this boot. The original default,
custom.cfg and empty next_entry were restored before the single1280x800 copy.

Evidence: `/home/gama/sgx535-offline/twod-fault-stage-20261028/v4/hardware/`,
particularly PHYSICAL_RESULTS.json, artifacts-1280x800/ and the complete
archive SHA256
`8d32298f3c239c4038857b69a20d5c015bfe6f1da8af8652d673b50fc2ce8f10`.
Consumed root: `/root/sgx535-twod-lab-boot-62adb132/copy-1280x800`.

```
phase=9 words=1 valid=1 event=80000000/00000010 bif=00004004
addr=40000000 root=349ef000 req=40000000
vfifo=00000000/00000000 fifo=00000080 blit=01000000
```

The sleep returned and phase10 hardware/root/quiet/raw-event/owned-ledger checks
passed before the first word. The fault was again observed immediately after
FENCE alone; source/destination/BLIT were not sent. This strengthens the
FENCE/initial-state hypothesis and narrows the earlier asynchronous-base
hypothesis, without proving the precise memory operation, its read/write
semantics or complete fetch footprint. The requested sleep is1--1.5ms;
its actual hardware duration was not independently measured. Stop repeating
the unchanged failing operation or merely lengthening this wait.

Core/revision `0x01130000/0x00010201`, op/cookie1/1;
error-5/stages1/retained1/uncertain1. Issue wall9.137754ms until rejection,
client wall241.646791ms are not GPU execution time or FPS. Completion,
visibility, retirement and readback integrity remain unapproved. Readbacks
are empty because uncertainty correctly prevented reads. No smaller-size
retry, CPU fallback, FENCE removal, fault clear or extra base mapping occurred.

Evidence was fetched after the probe terminated and all four controls returned
to N; producers_active0/reasons33. One default recovery reboot was requested.
The same read-only recovery observation is still awaiting network response;
no extra reboot or GPU operation was requested. Recovery is NOT yet verified
and the retained kernel lifetime must not be called ended. The original saved
default and empty next_entry were verified before the recovery request.
All31 protected artifacts remain intact.

Next work: resolve the FENCE memory/state contract using permitted primary
initialization/command evidence, including how its implicit accesses are owned
and bounded. The original header says its bits0--27 are ignored; matching
that packet is not a complete fetch contract. Do not infer that deleting FENCE
or silently mapping the faulting base page is a valid driver fix. Preserve
isolation, backing holds, error handling and the original 3D implementation.
General desktop GL/GLES2, textures/state, reusable submission and an installable
userspace driver still require their own implementation and physical matrix.
