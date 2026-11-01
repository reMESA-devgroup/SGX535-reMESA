# Frozen scenes through the normal userspace fence lifecycle

The portable provider originally accepted16 resources per job, while the
physically qualified general scene has17. The retained GEM/MMU consumer already
supports32. The host provider now holds up to32 consolidated resources;
this is internal storage capacity, not an ioctl or graphics capability change.
The preserved16-header test reproduces refusal of17 before dispatch. Tests
exercise1/16/17/32, reject33, validate late slots, unwind failed hold acquisition,
keep writable publication separate from retirement and retain all failed jobs.
Measured i486 provider-submit stack usage is1392 bytes in userspace; this is
not a kernel IRQ-stack qualification.

`remesa_persistent_set_native_scene()` installs an explicit synchronous native
callback only on a fresh endpoint. `scene_capture_submit()` snapshots the real
frozen graph, resolves each captured ID/generation to its actual owner and
submits all resources through the existing registry/provider. The normal device
fence is returned. The callback must authenticate retirement, CPU publication
and native acknowledgement, separately; context/host-operation identity must
match. Only then does the provider complete, publish, acknowledge its holds,
make writable owners readable and consume the capture. The fence remains
published until normal device fence destruction. Frontend owners can have been
deleted before submission; the capture retains their complete graph.

NOT_ISSUED rolls back host transitions and leaves the capture cancellable.
Ambiguous issuance, incomplete receipts or missing acknowledgement return a
failed fence and retain/quarantine the whole endpoint, including CPU-only
owners. No guessed cancellation, resubmission, force retirement or freeing of
uncertain backing is introduced. The setter and submitter do not enable ordinary
resource-only draw, clear NEEDS bits or advertise complete GL/GLES.

`remesa_linux_memory_scene_lab_dispatch()` connects this seam to the existing
closed LAB5/6 transport. Exact kernel identity, immutable inputs, bounds,
permissions, publication and retirement still pass the established kernel
checks. Only that authenticated native result becomes the host callback receipt.
The raw144-byte response remains available by metadata-only snapshot. There is
no extra INFO/hash/readback in dispatch, and no kernel/UAPI change.

The native C client now calls `scene_capture_submit`, waits using `device.ops`,
destroys the published fence and verifies its capture token was consumed.
It retains the same per-file GEM endpoint for independent parent collection.
The previous direct C transport client and its unexecuted payload are preserved.

Evidence root: `/home/gama/sgx535-offline/provider-scene-capacity-20261101/v1`.
`tests` compares original16 with current32 across native/UBSan/i486;
`repro-provider` uses the checked-in reproduction tool. `scene-provider-v2`
covers14 scene/fence cases per variant, including eight simultaneous published
fences, refusal of a ninth until slot reuse, deleted public owners, absent
native callback, local metadata refusal and uncertain receipt retention.
`native-client` compares the actual Linux C provider, constructor bytes and
provider/fence path at1280x800 and640x640 in three syscall-model variants, then
builds the actual static i486 client without interposition. `producer-final`
retains the original12 constructor regressions. These are CPU models/GPU0.
The first scene-provider compile failure used an incorrect test callback name;
it is preserved, and the test now uses the actual existing `device.ops->wait`.

The actual i486 provider/fence client completed physical1280x800 and640x640
submissions in independent fresh recoverable boots on the unchanged qualified
kernel. Both `v1/PHYSICAL_RESULT.json` and `v2-640/PHYSICAL_RESULT.json` verify
SGX535 core01130000/rev00010201, source-attributed completion/retirement,
whole capture namespace and executed controls,23 INFO records, unchanged12
GPU READ images and2 CPU-only buffers,4 guards, source closure, seven controls N,
complete memory cleanup and subsequent stock recovery with the original GRUB.
The unchanged analytical oracle finds zero COLOR/Z24 mismatches at both sizes.
The640 oracle permits320 exact edge ties only with coherent COLOR/Z coverage;
Z24 allows one quantization unit and undefined X8 is ignored. Reference images
were compared after execution, never uploaded by the collector. The real native
client returned only after the ordinary device fence wait/destruction and
verification that submission consumed its capture. Same binary and kernel,
closed selectors5/6; no simulated native backend in those physical receipts.

| Size | CPU publication flush | Observed raster completion | Full cold submit/fence |
| --- | --- | --- | --- |
|1280x800|55.850119 ms|16.400409 ms|185.528271 ms|
|640x640|52.094893 ms|6.183189 ms|163.320210 ms|

These isolated cold LAB times include host work and observation; they are not
GPU-only hardware counters, continuous rendering or GLXGEARS FPS. Existing
physical GLXGEARS remains preserved and was not remeasured.

The subsequent [driver scene builder](driver-scene-builder.md) composes these
existing owners from frontend attachments inside the library, with39 offline
ownership/failure cases and nine exact input-equivalence captures. That new
wrapper was not part of the physically executed binary from3bc7b59; do not
attribute a physical wrapper/API qualification to byte equivalence alone.

Normal Gallium dispatch/resource state,
full OpenGL/GLES behavior, desktop integration and installation are still open.
