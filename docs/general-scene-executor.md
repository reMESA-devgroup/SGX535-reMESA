# Ordered execution of a retained general scene

`remesa_gem_scene_job_execute()` connects the prepared general GEM job, private
root, plan-consuming service, native backend and source observation. It reuses
the canonical executor's ordering without its eleven-role owner/wire checks.
The canonical executor and all selected ISP/DATA/PDS/USSE encoders stay intact.
This kernel-only entry is not registered as a live ioctl or capture provider.

The caller retains a live outer DRM entry, serialized private backing session,
device/power/producer reservation, source lease and stable context/backend/source
storage. It must authenticate the actual immutable input construction before
calling. Admission must also verify the actual SGX535 rev121 identity and
selected hardware PDS window through the existing owned-entry gates; passing
the selected revision constant to the service is not an identity observation.
The executor takes the endpoint mutex, authenticates the selected
snapshot and default mappings, and binds the source before entering one MMU
writer interval. A successfully bound source consumes the attempt.

The selected snapshot check uses the existing `verify_job_bindings` and actual
job IDs, generations, pages, ranges, command identity and endpoint ownership.
It requires job-owned cached CPU views for READ-only inputs and refuses CPU
aliases for all other roles. The original all-view checker is unchanged.
Default mapping checks happen before the MMU writer, never through the bound
private root. This adds no full-buffer hash, pixel comparison or scan.

Within the writer interval the executor binds the private root, runs the
existing full scene closure/service binder and enters the native backend. It
records the actual GEM job as ISSUED with the operation cookie and marks the
loan started before the first service publication callback. The real general
service runs the real native ops and status source. Successful phase/ledger
checks are necessary but insufficient: the executor transfers CPU visibility
for GPU-writable backing, closes the nohash source capsule, ends the backend,
restores the root, unbinds and settles the source. Only then does it acknowledge
the real job/loan and context.

Any failure after source bind quarantines the complete job/loan/context. It
never supplies substitute completion, root restoration, source close, retry or
cleanup. A pre-bind error leaves the original refusal and backing holds.
`remesa_gem_scene_job_retire()` separately releases an acknowledged loan, retires
the actual job and zeroes the context only on complete success. Preparation
cancel refuses executed jobs; a retirement error leaves HELD storage retained.

## Evidence and limits

Evidence root: `/home/gama/sgx535-offline/general-scene-executor-20261101/v1`.
The scene model now runs the actual general executor, GEM publisher, selected
snapshot check, service and explicit job retirement at all four dimensions.
Source, backend and hardware-MMU callbacks are modeled; the preceding native
backend/source suites provide their separate offline coverage. This is not a
joint hardware qualification.

The executor model requires the real job's issued state/cookie before service
callbacks, ordered source/root/visibility/ack transitions, all 19 service stages,
22 expected cold upload/visibility flush calls, and a restored root before
retirement. A default-PFN lookup hook refuses any walk under the writer.
Missing READ views, writable CPU aliases, ownership receipt and rule ID changes
are refused. Ten isolated failure processes cover source bind, root bind,
backend admission, publication, MMU-fault status, source close, backend end,
root restore, unbind and settlement. Each asserts the actual retained holds;
post-consumption failures refuse cancellation/release and do not claim ACK.
The CPU process then exits with modeled holds intact: no production recovery
or cleanup is fabricated for the failure tests.

The first model compile failed because its include order puts the scene helpers
before the private executor definition. The harness now provides a forward
declaration of that same private executor object; production order is unchanged.
The failed artifact is retained. No safety check was relaxed.

The next missing entry is capture serialization/provider dispatch into this
transaction, followed by fresh bounded recoverable physical qualification.
Existing one-shot scripts must not be rearmed against a new boot/candidate.
Normal OpenGL 2.1/GLES 2 dispatch and full resource/API behavior, desktop
integration, supported installation and hardware validation remain incomplete.
