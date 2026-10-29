# Synchronous private GEM job composition

`remesa_gem_private_execute()` composes the retained GEM loan, private MMU root,
guarded fixed backend and existing stream source/service lifecycle in one kernel
call. It is compiled only in the opt-in private staging profile. There is no new
ioctl, production registration, user pointer, arbitrary executor callback or
permission to bypass the existing LAB admission path.

Its caller has already independently validated the final GPU images, relocations
and command/resource closure, prepared the GEM loan, initialized a borrowed scene,
and acquired the existing stream source lease. Owner, backend, observation and
loan storage remain stable through successful release or retained failure. This
entry executes one cold job; it does not make an unvalidated general GL command
stream acceptable.

The function checks exact copied commands and authenticated held GEM views/default
mappings before acquiring the writer semaphore. After existing source bind it
holds endpoint mutex then one MMU writer interval through private bind, guarded
backend begin, service execution, completion/CPU visibility, backend retirement,
checked root restoration and source unbind. The original service callbacks,
exclusive IRQ attribution, event ledger and backend end gates remain authoritative.
The existing nohash source finish requires actual service/sequence/terminal/event
receipts; the new function does not construct those receipts itself.

The job is marked possibly ISSUED with its exact operation cookie before entering
the service callback, matching GEM's original start convention. The upload helper
now accepts that state only when the retained same-call execution flags and cookie
match; arbitrary ISSUED or cookied jobs still fail. This resolves the mismatch
between preparation-only upload tests and actual GEM start ordering.

After the service reports retirement, the function requires the exact sequence,
RETIRED phase, complete event mask and original may-release gate. It transfers CPU
ownership by flushing GPU-writable backing and a read barrier. Read-only inputs
and CPU-only sidecars are excluded by private PTE policy. There is no pixel
readback, framebuffer comparison, cryptographic hash or evidence dump. The cold
job still conservatively flushes writable pools: this must not replace the FAST
streaming cache/reuse protocol or be presented as an FPS optimization.

Only after existing source finish, backend end, checked restore and source unbind
succeed does the loan receive its final execution receipt and the job become
ACKNOWLEDGED. Loan release then frees the unbound private tree and returns its
guard, retaining the original GEM holds until explicit job retirement. Borrowed
observer pointers are detached after the source no longer references the backend.

A failed source bind leaves the original prepared loan unconsumed. A successful
source bind consumes this attempt. Any subsequent error quarantines the actual
GEM job and marks an active private root uncertain without reset or guessed
restore. Loan release is refused until the complete final receipt exists, even
if an error happened after root restoration. Backend/source/owner storage and all
holds must remain alive. This is conservative retention, not a recovery protocol;
no caller may retry or free the objects on such an error.

## Qualification scope

`/home/gama/sgx535-offline/private-gem-execute-20261029/v2/RESULT.json` records 275
parent checks plus eight joined fault models in each native, UBSan and i386 run.
The GEM service, loan and new execution function are actual source. Scene roles
are a THREE-resource model; source, service, backend and MMU callbacks are modeled.
Tests cover parameter/command/view refusals before source bind, exact possible-
issue ordering, CPU-only/read-only cache exclusions, one writer interval, success
release/explicit retirement, and retained bind/begin/service/event/finish/end/
restore/unbind failures. These tests do not prove a full 11-role scene, real GPU
completion, hardware retirement or readback.

`v1/GUARD_RESULT.json` records the existing adapter/IRQ/native regressions and six
real antiX kernel translation units/partial link, with all new driver symbols
resolved. The new execution implementation matches the staged compiled source.
Subsequent loan header/source edits were documentation only. Non-private generated
source profiles remain identical to bdf3807 (`v1/LEGACY_STAGING.json`). This is not
a complete kernel module build or installation.

`v1/gem-regression/RESULT.json` records the existing 786-check loan/publication
suite per variant. `v1/mmu/RESULT.json` exercises the actual root quarantine helper
and previous root/maintenance/interval suites; it includes modeled MMIO and is
explicitly UNISSUED. The initial v1 execution model had 259 parent checks and is
superseded by v2's additional invalid-input tests.

No Mini12 write, GPU operation, physical readback or FPS measurement occurred in
this work. Next qualification must compose the full actual source/service/backend
and borrowed scene, then provide a controlled physical entry with all original
identity, submission, completion, retirement, readback, buffer integrity and boot
rollback gates. General GL/GLES2, desktop 2D, VA-API and an installable driver are
still incomplete.

Follow-up qualification with the actual eleven-role source/service found a
missing final source-settling check. The executor now invokes the existing
`remesa_stream_source_settled()` after unbind and before clearing observers or
granting ACKNOWLEDGED/execution_retired. Its failure preserves all GEM holds
and refuses loan release. The additional executor regression is retained at
`/home/gama/sgx535-offline/private-gem-settled-20261029/v1/RESULT.json`; the prior
eight-fault results above remain historical evidence for the earlier version.
