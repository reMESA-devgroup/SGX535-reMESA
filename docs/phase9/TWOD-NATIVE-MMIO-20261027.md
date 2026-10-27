# Native 2D FIFO adapter candidate

`remesa_twod_native.h/.inc` implements `remesa_twod_native_executor` for the
authenticated retained GEM 2D lifecycle. It performs real register operations
when called in a native kernel build; no pixel rendering/copy is implemented
on the CPU. The adapter is compiled, not registered with a userspace endpoint
or installed on Mini12. It remains a candidate without physical qualification.

Start authenticates the default PD through `psb_mmu_default_root`, requiring
the matching device/default directory, context0, valid directory mask, CLFLUSH
support and no pending table publication. The physical directory frame is
checked against the 20-bit bound before shifting, avoiding the original
getter's truncation. This read-only helper neither binds nor flushes a PD.

With producer and IRQ ownership held, the adapter requires the retained
SGX535 core/revision values, matching hardware root, zero bank selectors,
no MMU bypass/flush/reset/fault state, no enabled autonomous timer/kick, empty
FIFO and idle/bus-quiet engine. It captures and clears only a stale 2D event,
checks that status is actually clear, borrows BIF_TWOD_REQ_BASE, assigns a
nonzero monotonic cookie and records possible issuance before writing the ten
DWORDs supplied by the shared GEM-authenticated encoder. A posted read follows
the final FIFO write. It does not construct arbitrary instructions or accept
raw userspace packets.

Poll samples through the same pre-ACK capture path as IRQ, preserves all
unexpected events, and accepts only the exact owned 2D event with empty FIFO,
nonbusy blitter and no outstanding BIF reads/fault. Its budget is milliseconds,
bounded to10000; it sleeps50–100 microseconds between samples outside spinlocks.
Errors and timeout retain the native reservation and mark the context failed.
Power loss is checked before MMIO; error paths do not continue sampling an
unpowered device. There is no `psb_spank`, reset, forced drop or retry/rearm.

Visibility relies on the existing packet's final 2D FLUSH, owned completion and
quiet engine before the GEM service invalidates CPU destination cache lines.
This writeback interpretation must be checked by physical LAB readback; an
offline receipt does not establish GPU cache visibility.

Retirement requires successful visibility, rechecks faults/quiet/root, restores
the saved request base, checks the restored state, marks the owned ledger and
drops producer/IRQ ownership in one interval under producer then IRQ locks.
The new locked release helper avoids an unlock/relock gap. Only success clears
the native context, preserving its cookie serial for reuse; all uncertainty
retains the context and buffers. The caller must retain job/context/device and
module lifetime; never keep these objects on a returning stack after failure.

The new code contains no hashes, framebuffer comparisons/readbacks, buffer
clears, data allocations, dumps or per-frame logs. It retains essential cache,
address/mapping, event, fence and retirement work. Existing 3D code, gates and
the qualified GLXGears profile remain separate.

## Offline evidence

Current evidence:
`/home/gama/sgx535-offline/twod-native-mmio-20261027/v2/`.

`RUN.py` executes the actual adapter with explicit CPU MMIO/time shims in native,
UBSan and i386 modes. Each reports2928 parent checks, including the510-check
producer regression, plus20 joined fault models. Three operations reuse each
1280x800/640x640/641x479 fixture. Tests check exact FIFO words, old-event ACK,
possible-issue ordering, cookie matching, restored base and ownership release.
Faults cover core/revision/root/bank/bypass/reset/timer, stale event not clearing,
base write/restore rejection, timeout/busy/missing event, MMU/status faults,
visibility/retirement faults and power loss with no further MMIO reads.
No simulated GPU pixels are generated or treated as physical output.

The actual default-root helper passes42 checks per mode: mismatched device/PD,
context/mask, missing metadata, pending TLB work, absent CLFLUSH and zero/high
PFNs all refuse unchanged output. The IRQ regression also passes838 parent
checks including510 producer checks, plus62 joined event-fault cases per mode.

Six complete native translation units compile with retained actual antiX i686
commands and link into one relocatable object with all new symbols resolved:
accel_2d, psb_drv, gma500_fixed_backend, psb_irq, mmu and remesa_gem_memory.
The canonical stager now pins nine input files and stages all shared 2D/GEM/MMU
components together. It checks unique anchors and refuses existing outputs.

`BUILD_FULL_MODULE.py` builds the complete candidate
`module/gma500_gfx.ko`; `FULL_MODULE_BUILD.json` records its hash and command.
The module has vermagic `5.10.240-antix.1-486-smp SMP mod_unload modversions 486`.
`MODULE_AUDIT.json` and `MODULE_PROFILE_VERIFICATION.json` confirm that the
existing compiled GLXGears profile matches its retained assets byte for byte
and all new 2D symbols resolve. The full build has21 warnings, all in unchanged
original source files; focused builds/tests have empty stderr. This is neither
a physical regression approval of the new module nor an installed driver.

Earlier evidence remains in the parent directory. Its first fixture was
rejected because its declared source allocation overlapped the destination;
only fixture length was corrected. `FIRST_FIXTURE_FAILURE.txt` preserves that
failure. The v2 adapter additionally checks power loss before MMIO and is the
current candidate. Original module/source artifacts remain unchanged.

## Next physical path

A LAB entry point still must resolve the file-bound GEM endpoint, prepare the
typed request and retain a heap-owned native/job context on any failure. The
existing postclose quarantine sets `endpoint->file=NULL` and retains module,
device and power; the new context must follow that lifetime rather than become
a dangling pointer. Do not enable an arbitrary completion-injection interface.

Offline-test the complete endpoint and userspace harness, build its exact
module, then preserve N/N and rollback while selecting a recoverable boot.
Execute paired1280x800/640x640 and occasional641x479 real copies, checking
core/revision, submission, completion, retirement, readback and guards. The
physical FLUSH/writeback and request-window contracts remain unproven until
those results. No GPU operation, new FPS, API capability or native2D approval
is claimed here. General desktop GL/GLES2 and the installable userspace driver
remain unfinished.
