# Existing homogeneous captures in the general session

The general session now explicitly accepts the existing homogeneous catalogues
3/4 as well as the existing OpenGL CODE344 catalogues 5/6. It selects the same
profile objects used by the one-shot executor. No shader, geometry, encoder,
command image, placement or instruction padding is changed.

This joins two independently tested components: the captured frontend can hold
its resources and read retired COLOR at both primary sizes, and the general
session can retain native backing between jobs. The Linux one-job adapter can
now open a packet from the homogeneous frontend instead of requiring a separate
OpenGL-generated packet solely to enter profiles 5/6.

This is an explicit bounded LAB extension. It still requires the matching
module, root permission, default-off gates, exact profile image authentication,
owned address/permission checks, immutable READ seals, DPM/source ownership,
completion, publication and retirement. Profiles 1/2 remain outside this ABI.
The old 11-ID private session is not reinterpreted. The existing 64-job limit,
sequence checks, close acknowledgement and uncertainty quarantine remain.
Kernel per-SUBMIT LAB input authentication has not been removed or advertised
as a production optimization.

An older module may refuse 3/4. Callers must qualify a matching module before
execution; they must not probe and retry another path after an entered ioctl
fails. The Linux adapter retains its conservative endpoint poisoning rule.

Evidence: `/home/gama/sgx535-offline/general-session-homogeneous-integration-20261102/v1`.
The controller test matrix exercises all four selectors across native, UBSan
and static i486/QEMU, preserving permission, disabled-gate, generation, input
seal, sequencing, failure/poisoning and close tests. The transport matrix checks
one native SUBMIT per provider call for each selector, unchanged packet/host
operation rules, exact receipts, aliases, capacity, close and uncertainty, along
with the existing transport regressions.

All 180 controller cases and seven transport targets in each of three variants
pass. The complete antiX 5.10.240 i486 module builds with no new warnings. The
seven registered controls have mode 0600 and static zero initializers; five
initrd identity tests and four failure-console tests pass. The new image is
local only at `boot/depth1280-private-initrd.img` under the evidence root.
Comparing against the preceding qualified full-profile staging shows only
`remesa_general_session.inc` and `remesa_general_session_uapi.h` changed; every
other staged source and existing catalogue byte remains identical.

This extension needs new physical cold/warm tests. The preceding physical
frontend tests used the one-shot executor; they do not qualify repeated general
execution. No new module or image is installed by these offline tests. Dynamic
authenticated inputs, production native admission, complete GL2.1/GLES2,
desktop integration and installation remain separate unfinished work.
