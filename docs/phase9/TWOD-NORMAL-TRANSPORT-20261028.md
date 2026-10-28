# Native copy without LAB diagnostics, through the Linux userspace transport

The separate native-copy service and `remesa_linux_memory_copy_2d()` now pass
physical repeated-copy tests on the Mini12 SGX535 rev121. At each of1280×800,
640×640 and641×479, the actual userspace transport created/pinned/mapped two
allocations once and completed16 synchronous native copies before readback.
All48 operations passed retirement and all three final guarded readback pairs
match the preserved LAB oracles byte for byte.

This is a qualified privileged copy primitive. Gallium's resource-copy callback
is not connected to it yet; the existing CPU byte-transfer callback remains
explicitly a CPU path. This does not qualify desktop acceleration, general
OpenGL/GLES, presentation FPS or an installable driver.

## Normal interface and safety boundary

`include/remesa/twod_copy_uapi.h` defines a separate120-byte version1 request at
private ioctl7. IDs and a rectangle refer to already owned allocations in the
exact file/context. No user VA, PFN, raw packet, code, injected receipt or
diagnostic phase timestamps are accepted. The kernel reconstructs the existing
vendor nine-word copy from authenticated backing and runs the existing retained
GEM/native completion, visibility and retirement lifecycle.

The service is explicitly compiled with `--with-native-copy`, defaults to N,
and retains DRM_ROOT_ONLY, CAP_SYS_ADMIN, the memory-enable gate and per-file
context checks. It is experimental and privileged, not an unprivileged render
ABI. Mixed normal/LAB stager profiles are refused. The shared vendor-stream
macro preserves the old LAB alias and its requirement for the LAB build.
The original ten-word encoder and152-byte LAB ABI remain unchanged.

The normal service has no LAB per-phase clocks or output, pixel checks,
readbacks, hashes or detailed logs. The native adapter's per-word fault
observation and pre-FIFO diagnostic sleep remain in LAB only. Necessary
pre/post hardware/root/MMU/fault checks, IRQ ownership, polling deadline,
coherency, completion, ALPHA/request-base restoration, retirement and uncertain
backing retention remain active. The normal module contains no2D LAB ioctl,
parameter or failure-log marker. The ordinary build without these optional
profiles retains byte-identical accel_2d text and the compiled gears asset.

The userspace function accepts IDs tracked by its own fd/context, rejects
active CPU leases and poisoned/missing mappings, and performs one copy ioctl.
It checks immutable control fields, exact core/revision, complete retirement
and a new monotonic operation. There is no retry, INFO, buffer comparison,
CPU mapping/readback, clock or allocation in that function. Its small control-
receipt comparison is necessary protocol validation, not a framebuffer memcmp.
Ambiguous or malformed outcomes poison both objects; forced unpin/destroy is
refused. Known unissued failures retain their error contract. Memory-only
recorders are refused because they cannot record the separate copy ABI.

## Offline evidence

Root: `/home/gama/sgx535-offline/twod-normal-copy-20261028/v1/`.

`CLOSEOUT.json` records the offline prerequisite checkpoint: normal native,
UBSan and i386 packet/GEM/lifecycle/endpoint models,32 vendor fault children
plus21 legacy children per architecture,239 endpoint parent checks and seven
retention/cancellation fault models. `lab-regression/FOCUSED_RESULT.json`
records54 passing legacy/LAB commands, preserving the49-child diagnostic
suite. Normal positive models explicitly require zero pre-FIFO diagnostic
sleeps; LAB retains its checks.

`TRANSPORT_RESULT.json` covers24 interposed protocol/error cases, including
stale receipts, monotonic same-object reuse, malformed responses, INT_MIN,
EINTR without retry, lease/ID/alignment refusal and poisoned-object retention.
`CLIENT_RESULT.json` binds the three architecture builds and safe describe/
/dev/null cases. `NORMAL_PATH_AUDIT.json`, `MODULE_PROFILE_VERIFICATION.json`,
full module/image build records and identity-chain tests bind the candidate.
All31 protected artifacts and the original kernel inputs remain unchanged;
the21 build warnings are in unchanged original files.

The actual Mesa compilation model passes in
`/home/gama/sgx535-offline/general-userspace-driver-20261027T040000Z/private-mesa-native-copy-v2/RESULT.json`.
Three regressions against that private archive (old memory transport, retained
scene captures and legacy gears geometry) pass in `native-copy-regressions-v2/`.
The original cached archive/objects remain unchanged. The101-file staged Mesa
overlay includes the new header; it still refuses general GL admission.

Initial harness failures are preserved: an unused LAB-only log helper in the
non-LAB test, copied old client size assertion, missing extracted model headers,
and a stricter actual-Mesa wrapper prototype/irrelevant validator dependency.
They were fixed in the harness or test prototype; no runtime gate was waived.
The recorder check was moved after local input validation in the library.

## Physical repeated-copy result

Candidate module SHA256:
`9967c6f2e409e4a9e4341dc183dabe854fecf0a6a2685272f9a5be8fdd1cf22c`.
Private image SHA256:
`22eb901478c24351abb0d152103844f8993d8f84af5289340edc59400bbd944f`.
Boot: `5182c2c3-3917-4028-929e-7e489fb8faea`.
Client SHA256: `6a22fa36e9dec423e861cf32ece8dc15f39568ff9bc83437456fcfaaab65672b`.
Core/revision: `0x01130000/0x00010201`. Original GRUB/default and empty next entry
were restored before any GPU operation. The boot-envelope filenames retain
“LAB” from the original first-load harness; the selected kernel profile has
no2D LAB ioctl or observations.

| Copy | Operations | Mean call ms | Minimum ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: |
|1280×800|16|37.810731|34.577302|43.384218|
|640×640|16|14.590870|12.854987|15.965648|
|641×479|16|11.238523|9.511615|12.833475|

These are external probe CPU wall times around the normal userspace call,
including necessary validation/cache/synchronization work. The samples span
about0.605/0.233/0.180 seconds, not a sustained presentation benchmark. They
are neither isolated GPU time nor desktop/GLXGears FPS. The earlier LAB phase
sums have different timing boundaries and cannot establish a percentage gain.
CPU DVFS/load was not controlled.

The library accepted16 new monotonic operations per context, ending at16;
each response passed complete/visible/retired status and had error0/retained0.
The buffers and mappings remained the same throughout each sequence. Source
upload and final readback/oracles belong only to the qualification tool. No
intermediate pixel result was read back; this does not count as LAB validation
of every intermediate frame or long-duration stability.

Final readback coverage is14,041,088 bytes across the three pairs. Source bytes
are unchanged, only the requested destination rectangles changed, and all
borders/padding/tails are intact. Every final file is byte-exact to the earlier
[LAB qualification](TWOD-PHYSICAL-COPY-20261028.md). All runtime controls returned
to N, producers_active0, context cleanup0, and no new native fault/BUG/WARN/Oops
appeared. `hardware/PHYSICAL_RESULTS.json`, per-size fetched archives/raw files
and independent-verification reports retain the proof.

Next: connect this primitive to authenticated Gallium resource ownership and
its synchronization rules. Keep permissions/isolation closed until qualified;
do not use the normal profile to approve LAB tests or general API support.


One return-to-stock boot occurred only after all48 retirements, final readbacks
and context closes passed. `hardware/FINAL_REMOTE_STATE.json` verifies stock
boot `639e968a-5a0a-4c7c-97d3-fbc495f69ad0`, exact original module, no observer or
experimental controls/proc endpoint, original GRUB/default and empty next entry.
Reset was not used to satisfy retirement. All probe/fetch/SSH/reboot observation
handles are terminal. No user libraries or default driver were replaced.

The remaining immediate integration is an authenticated Gallium resource-copy
callback with correct writer/resource lifetime handling, followed by physical
frontend tests. Unprivileged admission requires its separate memory-isolation
qualification; CAP_SYS_ADMIN must not simply be removed. General3D still needs
its complete event PDS, attachment/DPM initialization, raster control and typed
native submission. Installation and later video acceleration remain separate
unfinished requirements.
