# Vendor 2D submission boundary after the physical FENCE fault

The physical native adapter rejects immediately after its first FENCE word,
even after the LAB pre-FIFO wait/check passes. This investigation follows the
old shipped 2D implementation to distinguish a packet assumption from the
actual queue/state contract. It does not modify the driver or execute a new
Mini12 workload. The full general3D/2D/userspace/installable objective remains
unfinished.

Private artifacts: `/home/gama/sgx535-offline/twod-fence-contract-20261028/`.
Start with TARGET.json, FINDINGS.json and the seven complete *-REA.json
records. Recovered pseudocode is analysis output, not original source.
No proprietary code or firmware is incorporated into the driver.

## Host call chain observed statically

The exact ELF `libEMGD2d.so.1.5.15.3226`,31,280bytes, SHA256
`d0f7c06c94f3a5c8e122727d60dfd6a87e020849838b257b8fc8307cf057bf59`
imports `libemgdsrv_um.so` and SGXQueueTransfer. Its observed chain is:

1. PVR2DBlt calls PVR2DBltClipped, which calls implementation entry0x13060
   in Ghidra's address space. The imported PLT and exported implementation of
   PVR2DBltClipped share a name; the rejected ambiguous query was preserved
   and corrected to the explicit implementation address0x14910.
2. The implementation builds a bounded local command array and sets transfer
   selector2, count, pointer and alpha-related field before SGXQueueTransfer.
   No conclusion about the original C calling convention is taken from the
   decompiler's inferred register arguments.
3. PVR2DCreateDeviceContext creates a device-memory context, selects heaps
   and calls SGXCreateTransferContext; it does not merely open a raw FIFO.

REA Evidence IDs:

| Observation | Evidence |
| --- | --- |
| PVR2DBlt | ev_f8af7271e8915be787048ab5ac129a9eb7674a1ad09fc8fa410625f9aa64618f |
| Explicit PVR2DBltClipped entry | ev_cf262330325fab542b381432375910a42140a2912b36c8638ff1fe1a4509d1ad |
| Blit command builder | ev_666317ffedd990e7dbc890ae77ff5a3c9720cfc9c6c3b184827af31488fc5605 |
| Device-context creation | ev_a2c82751c5b17d29f3d10645982038162706593cc6e59f466e7644c776dec482 |

The matching UM ELF SHA256 is
`5f3bbea4ea98907fd44be0f84fe8832d395b41c07fbc408b2c19f6c61c974e7c`.
In SGXQueueTransfer's selector2 branch, the recovered host operations are:

- Reserve a0x100byte queue record.
- Store record/header fields, including caller count and an alpha-related
  field; copy caller command DWORDs beginning at record+16.
- Append0xf0000000 after that copied payload and increment the command count.
- Collect synchronization references and invoke the bridge submission helper.

That branch does not prepend0x70000000. This bounded observation does not prove
that no other producer, firmware path or kernel code can add a FENCE.
SGXCreateTransferContext also allocates queue/control/device-memory and sync
objects. Their addresses and dependencies must not be replaced by guessed
scratch memory in the native driver.

| UM observation | Evidence |
| --- | --- |
| Queue branch | ev_bca2bbe5bf7a13f130a79efd0e85f955719faf0a08fc90bd0dad33a2ee691f3d |
| Transfer-context creation | ev_b75aed177e180ec26df1e411c9de554c0d78c3e0ccbfbf6d5e457b30fe1599e3 |
| Submission helper | ev_fd357aea37e5893539ef2eeb6705c221f3e5be6a999dbbdd9c188427e49f484c |

## Original SDK decoder applied only to retained 2D firmware

The existing unchanged SDK3.5 SGX535/rev121 decoder and canary-checking shim
were reused. Source init ELF SHAd1926b99af680332f0c22a5b65c8895caa4522902acc3dbeab65720f853035db.
The exact pbuKernel2DProgram range starts at file0x18ba0, has14,744 declared
bytes and SHA256
`2a580c31e5a3be689fc14f0f669b2d37563e545fd5e26c279a68bec2803a40d5`.
The extra copied tail/metadata is excluded. All1,843 aligned eight-byte records
were supplied to the original decoder in four chunks, each at most4,096bytes,
under the original15second per-process timeout. All inputs remain unchanged;
canary/termination checks pass and stderr is empty. This executes the decoder
on the CPU, not the firmware on a GPU.

FIRMWARE2D_DECODE.json, firmware2d.usse.txt and decode_2d_firmware.py retain
all source bindings, commands and individual observations. The decoder uses
its original descriptor `[4,121,0x40128f80,0,0x082d2800]`. No encoder, ISA
interpretation or gears shader was changed for this inspection.

The selected region at declared-payload offsets0x12c0--0x1320:

- Loads a count from header DWORD3 and constructs a pointer at header+16.
- Loads command data, writes the alpha-related value through `str #903`,
  decrements the count, and writes the loaded command through `str #4096`.
- Performs a conditional next load and loops back to the decrement/store.

The decoded indices903 and4096, multiplied by4, numerically match SGX535
2D_ALPHA0xe1c and the slave port0x4000 in the retained register definitions.
That address-unit mapping is corroborating inference, not an independent
physical observation. Decoded load/post-increment and branch spellings are
retained verbatim; this inspection is not a full instruction interpreter.

The loop is a useful separate consumer boundary for the host queue payload.
Its observed sequence does not prepend a FENCE inside the selected loop.
Other firmware paths, patch state, context contents and dynamic reachability
remain unknown. Absence of a literal in a decompilation is not proof of global
absence or permission to delete synchronization.

## Consequence and next minimum

The old framebuffer packet and the vendor queue path have different
synchronization/context boundaries. Matching the former is insufficient to
establish the latter's complete footprint. The current native adapter's FENCE
fault remains unresolved; neither this static chain nor SDK output approves a
new packet, framebuffer result or hardware capability.

Next, corroborate a narrow native-copy packet against captured output of the
old command builder with explicit CPU-only stubs, or establish the complete
preceding dependency/context contract from primary evidence. Keep an alternate
LAB profile separate if justified by that evidence. Preserve the old encoder,
exclusive admission, address/range/PFN checks, coherency, fences, retirement,
quarantine and paired physical validation. Do not silently omit FENCE, create
an unowned base mapping or turn a diagnostic experiment into production approval.

The latest physical recovery remains unverified: two read-only probes this
turn still reported NoRouteToHost192.168.18.90. There was no new reboot/control
change/GPU submission. Reconnect and verify the exact stock boot before more
physical work. Native analysis sessions are closed; REA_CLEANUP_FINAL.json
records a transient close-cleanup warning, launcher exit0/PID absent, closed
session and no associated process remaining. No unrelated process was killed.


## Controlled original command-builder capture

The next minimum above has now been executed. Evidence is at
`/home/gama/sgx535-offline/twod-fence-contract-20261028/oracle-v2/`:
RUN.py, oracle.c, stub_um.c, STUB_IMPORTS.json, RESULT.json, CLOSEOUT.json and
build/run stdout/stderr. The earlier oracle-v1 is retained, including its
initial local compiler indentation failure and corrected successful run.

The original pinned libEMGD2d is loaded under QEMU i386. Only PVR2DBlt is
called with guarded fabricated CPU context/memory metadata; no original
context constructor or device service runs. A private library with the exact
UM SONAME substitutes its dependency. MemSet/MemCopy are ordinary CPU helpers;
SGXQueueTransfer validates and captures the descriptor/command array instead
of submitting. Every other strong device-service import is a terminating
trap. dl_iterate_phdr verifies exactly one UM dependency is loaded and its
path is the private stub. Original constructors/JCR metadata were inspected;
input structures have canaries. LD_PRELOAD/LD_LIBRARY_PATH are removed from
the harness environment before invoking the private loader path.

Observed results:

| Cases | Result |
| --- | --- |
| Separate BOs1280x800,640x640,641x479 | Exactly one type2 callback each; eight payload DWORDs |
| Same BO, four overlapping copy directions | Eight payload DWORDs, including the expected direction/coordinates |
| Forced queue return13 | Original PVR2DBlt returns-2; the submitted body is still captured |
| Null request | Original PVR2DBlt rejects without a queue callback |

Every captured body is byte-exact with DWORDs1--8 of the unchanged existing
legacy encoder. The old encoder still produces ten words: initial FENCE,
eight-word body and final FLUSH. The original caller produces eight words;
the separately observed original UM type2 queue appends FLUSH, yielding nine
stream words at that boundary. The alpha-related descriptor field is0xff00.

The stub, harness and current encoder compile with warnings treated as errors;
all three build/run commands exit0 and final stderr is empty. All input
canaries pass. The original library and encoder source hashes are unchanged;
31 protected historical artifacts remain intact. This is independent original
host-binary emission evidence, not CPU pixel rendering, a simulated hardware
completion or a physical copy approval.

The next implementation can represent a separate explicit LAB stream profile
using this captured producer boundary, preserving the old encoder/profile.
Reserved storage, effective DWORD count and consumed bytes must remain distinct;
no padding opcode may replace a word. Before hardware, authenticate the full
exclusive/context/dependency contract and alpha-related state, preserve all
memory/coherency/fence/retirement/quarantine checks and run the offline gates.
The captured body alone does not justify a silent FENCE removal in production.

## Explicit vendor-stream LAB implementation and offline qualification

The separate candidate now lives at
`/home/gama/sgx535-offline/twod-vendor-stream-20261028/v1/`.
`REMESA_TWOD_LAB_VENDOR_STREAM` requires the LAB build flag and is absent from
normal production. Its internal typed GEM schema is `0x31443256`; the existing
`0x31443252` schema and ten-word encoder remain supported unchanged. There is
no new raw-packet userspace ABI. The root-only LAB endpoint uses the new schema
only in this explicit build.

The vendor encoder reserves the existing 40-byte command capacity, returns
nine effective DWORDs (36 bytes), and zeros the unused four-byte tail. The
submitted stream is the captured eight-word payload followed by the existing
FLUSH word. The unused tail is never emitted. Legacy FENCE behavior is retained
in the old profile. This experiment does not establish equivalence to every
vendor transfer-context synchronization dependency.

A conservative new-profile prerequisite checks the read-only ALPHA register
against the captured `0x0000ff00` field. An incompatible initial value causes
rejection before the first FIFO write; the adapter does not change shared ALPHA
state. Failure snapshots include that value. Address/PTE/PFN/permission checks,
producer exclusion, coherency, owned IRQ cookies, completion, visibility,
retirement and uncertainty retention remain active.

`FOCUSED_RESULT.json` records 54 successful build/run commands: legacy and
vendor GEM, lifecycle, LAB and native models in native, UBSan and i386 variants,
plus eight independent captured vendor packet fixtures. The native vendor
suite preserves all legacy cases and adds three dimensions with three copies
each, 38 fault children, and ALPHA mismatch rejection. Word-ten fault injection
is explicitly excluded for the nine-word profile. The GEM lifecycle test in
the vendor build still exercises the legacy schema; vendor lifecycle paths are
covered through the LAB endpoint's seven fault children.

`NATIVE_RESULT.json` records 14 successful commands including real antiX kernel
objects and partial linking. `FULL_MODULE_BUILD.json` records the complete
candidate module. `MODULE_PROFILE_VERIFICATION.json` proves the old compiled
gears asset remains byte-exact. `PRODUCTION_TEXT_COMPARISON.json` proves the
complete normal `accel_2d` text matches v4. All 31 protected artifacts and pinned
input files remain unchanged. The 21 full-build warnings are in byte-unchanged
original files. `CLOSEOUT.json` is explicitly CPU/offline qualification with
zero GPU operations, not a physical 2D approval.

The first focused harness compile used the already integrated staged GEM file
and included preflight twice. The runner's include order was corrected to the
original GEM source plus current separate preflight, as used by the earlier
qualified harness. `RUN_FOCUSED.first-failed.py` retains the initial runner.
No driver safety check was removed to address that harness error.

## Physical initial-state rejection (v1)

The candidate was booted once, with exact module/observer identity and a virgin
source guard, on boot `8897d270-7f0d-491e-8cd8-c82b4fe2109d`. The original GRUB
configuration, saved stock default and empty next entry were restored before
attempting the copy. ConnMan also returned online automatically under runsv
(PID1652, parent1473), qualifying its startup after a full reboot.

The 1280×800 attempt identified core/revision `0x01130000/0x00010201` and was
rejected with `-EOPNOTSUPP` at phase2, **zero FIFO words**, cookie0. The new
ALPHA prerequisite observed `0x00000000` rather than `0x0000ff00`. BIF status
and fault address were zero. The request-base register still held the saved
`0xd0000000`; no new base publication or producer interval had begun. This is
an initial-state rejection, not evidence of a nine-word stream executing or
failing on the GPU, and not a successful fix of the old FENCE fault.

Completion, visibility, retirement and readback did not pass. The client
retained the uncertain job and checked zero source/destination bytes. The two
zero-length readback files are not readback evidence. Its 8.186027ms issue wall
time and 335.678999ms client wall time are rejected-attempt timings, not GPU
execution time or FPS. Four controls returned to N; producers_active and
source reasons remained zero. No smaller-size retry occurred.

Evidence: `hardware/PHYSICAL_RESULT.json`, `hardware/artifacts-1280x800/`, and
`hardware/1280x800-evidence.tar.gz` with SHA256
`48180e1df8b6464932b96973e828861042bd1ca5ecfd074b2e19aa99487ae1ea`.
A default recovery reboot was requested only after evidence was fetched.
Recovery must be established by `hardware/FINAL_REMOTE_STATE.json`; reset is
not GPU retirement.

Next minimum implementation: explicitly own the vendor ALPHA state during the
exclusive native transaction. The retained SGX535 header names register0x0e1c
and the ONE/ZERO component masks0xff00/0xff; the captured request and examined
firmware output support the intended value. Save its initial value, validate
quiet engine/power/core/root, publish the required state under both existing
locks and the producer interval, verify readback, retain state/backing on any
uncertainty, and restore/verify the saved value only after proven completion
and visibility before reservation release. Add failure models for publication,
readback, interference and restoration before a new isolated hardware test.
Do not simply delete the prerequisite or write ALPHA outside engine ownership.

Recovery is now verified on stock boot
`59284e53-2648-4da4-b40b-96f6a467d6b4`. FINAL_REMOTE_STATE.json proves the
original stock module, absent experimental controls/proc endpoints, original
GRUB configuration, saved default and empty next entry. The retained kernel
lifetime ended by reset; no GPU retirement is claimed. All local test, build,
SSH and reboot observation handles for this candidate are terminal.
