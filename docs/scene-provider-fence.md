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

Physical validation of this new provider/fence route is pending. Existing
kernel COLOR/Z24 passes remain valid. Normal Gallium dispatch/resource state,
full OpenGL/GLES behavior, desktop integration and installation are still open.
