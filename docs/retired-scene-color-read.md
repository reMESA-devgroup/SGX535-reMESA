# Reading retired COLOR while keeping the scene alive

`remesa_persistent_scene_color_map` opens an explicit CPU READ lease for the
exact COLOR allocation of a retained scene capture. The capture must have a
successful `submit_keep` operation whose native receipt and provider lifecycle
already acknowledged retirement and CPU publication. A newly prepared capture
has no such credential and cannot use this API.

The ordinary held-resource map guard remains unchanged. This separate route
validates the frozen COLOR role, write access, generation, address and extent;
all captures contributing a hold must identify the same allocation as idle
COLOR. Mixed input, DEPTH, DPM or other roles are rejected. Registry validation
still requires mapped resources with no pending GPU operation. A live CPU map
or unacknowledged job prevents entry. Capture counts are checked before scans.

The data comes from the existing provider's synchronized
`cpu_map(..., false)`. It is not a raw GPU address, an unsynchronized alias or a
claim that mapped memory is coherent without the backend protocol. The backend
may copy/read COLOR to provide the lease; this API adds no pixel comparison,
hash, input/DPM readback, upload or presentation. There is no FPS claim.
The caller uses the pre-map immutable footprint for layout and extent.

While the lease exists, submissions and capture release are refused. An ordinary
`device.ops->unmap` call cannot close it or publish it as a write. Only
`remesa_persistent_scene_color_unmap` with the same capture closes the provider's
READ lease, without a write synchronization. After successful close, the same
capture can submit again. The lease retains an additional backing reference,
including when application owner references have been dropped.

An atomic provider refusal leaves the capture usable. An uncertain mapping,
invalid returned pointer, output alias or any unmap error quarantines the whole
endpoint and retains the possible CPU lease and references. No retry, guessed
unmap or disposal is issued. This cannot manufacture GPU retirement or native
source-session close; those remain independent requirements.

## Evidence

Root: `/home/gama/sgx535-offline/retired-scene-color-read-20261102/v1`.

The `test_scene_color_read.c` fixture uses the actual persistent provider,
registry, captures and owner graph, with modeled native receipts and CPU
backing. Twelve cases cover the pre-publication refusal, successful reading
between submissions, exact unmap, shared captures, dropped application owners,
invalid tokens/output aliases, atomic allocation refusal, uncertain/invalid
mapping and two unmap failures. All unmap calls are asserted read-only; a
writable ordinary unmap cannot terminate the lease. Failure cases retain their
modeled resources until process exit.

The keep-capture and original one-shot suites run alongside it against the
same current source in native, UBSan and static i486/QEMU variants. These are
host ownership/coherence-contract tests, without GPU execution or rendered
pixels. They do not qualify the new kernel candidate, sustained OpenGL, X11
presentation, GL2.1/GLES2 or the complete driver.

The [Gallium transfer integration](scene-color-frontend.md) now routes eligible
texture READs and presentation snapshots through this lease. Automatic retained
draw dispatch, general native session admission and changing inputs remain
separate work. Broader API coverage, complete desktop 2D/3D and installation
remain unfinished. The reported first-load HOLD must also be diagnosed before
qualifying the new kernel candidate physically.
