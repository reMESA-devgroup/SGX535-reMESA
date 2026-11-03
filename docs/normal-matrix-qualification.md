# Qualifying the ordinary OpenGL path

`normal_matrix_native.py` supervises the ordinary OpenGL client introduced in
`normal-matrix-submit.md`. It is separate from the production driver and from
the existing closed LAB catalogues. Its default mode only checks inputs and
writes a plan; `--execute` requires the outer physical boot/source/recovery
controller to have qualified the exact module and enabled the intended gates.
This tool never enables gates, installs, reboots, retries or presents pixels.

The bounded test calls real Mesa `glClear` and `glDrawArrays`, using the
existing native 2D fill for COLOR and matrix executor/Z-clear prefix for 3D.
It records the fill receipt, the native matrix receipt and the original host
ticket separately. Host operation numbers must not be mistaken for native
GPU operations. Both COLOR/Z readbacks, GL readback origin, unchanged READ
images/CPU owners, four enclosing guards, idle memory and remaining ownership
are checked before the supervisor manually releases the six remaining BOs.
Failures or uncertain outcomes suppress manual cleanup. Cleanup errors stop
at the failed operation; no retry or guessed completion occurs.

The existing matrix/lighting shader and ISP encoders are unchanged. The test
supplies translation Y=1/256, within the already accepted source domain, to
avoid ambiguous sample-on-edge cases at both 1280x800 and 640x640. The first
offline candidate Y=1/128 was correctly refused by the expectation generator
for an edge tie at 1280x800. Its artifact is retained. These are test inputs,
not a change to drawing semantics or an additional rendering restriction.

`normal_matrix_oracle.py` reuses the established rational sample-centre and
Z24 rules for this one explicit input. It rejects unknown inputs and edge
ties. Expected images exist only for comparison; they are never uploaded to
GPU memory or used for presentation. Its 31,920 and 12,720 covered pixels
have been checked against an independent scanline calculation. This does not
establish physical correctness of those images; that remains the experiment.

The archive verifier deliberately reports `physical_qualification=false`.
Its result must be combined with exact PCI/core/revision, boot/module notes,
first-load evidence, fresh source/guard state, actual submissions/retirement,
eight runtime controls and stock rollback evidence from the outer controller.
Use `verify_firstload_module.py --with-matrix-submit` for this candidate's
eight registered 0600/default-zero bool controls; the default still checks
the historical seven-control profile. Static ELF checks cannot attest runtime
N values. Only memory, native fill and matrix submission may be enabled for
this experiment; the old LAB scene/stream/probe controls stay N.

Offline artifacts:
`/home/gama/sgx535-offline/normal-matrix-qualification-20261103/v1/`.
`normal-gl-v2/RESULT.json` records twelve actual Mesa frontend cases and the
fresh producer exports. `client-final/RESULT.json` pins the compiled real
client; `client-model/RESULT.json` exercises it with interposed ioctls.
`archive-model-tests-final/RESULT.json` records envelope, pixel, ownership and
supervisor fault tests. Original zero-framebuffer ioctl-model captures must
fail the pixel verifier. Separate clearly marked synthetic copies contain
expectation bytes solely to test the verifier's positive branch. They never
constitute hardware evidence. `ORACLE_INDEPENDENT_TESTS.json` records the
scanline check and all 240 source-byte mutations.

`boot/INITRD_BUILD.json` and `boot/INIT_CHAIN_TEST_RESULTS.json` describe the
offline image and parent/script/module identity checks. They record the
image's original undeployed state; the later physical outcome is below.

## First physical attempt: source claim refused, stock recovered

`PHYSICAL_FAILURE_AUDIT.json` records the actual 1280x800 attempt. Experimental
boot `702fbca9-9391-457d-9fbe-1157dc795e6f` passed module/observer identity,
first-load, fresh source and all eight N controls. The stock GRUB configuration
and saved default were restored before enabling only memory, fill and matrix.

The actual OpenGL client reached native COLOR fill, operation/cookie 1/1,
stages31, core01130000/revision00010201 and retirement. This attempt did not
independently read back the fill's pixels. Matrix submission then returned
`error=-EBUSY`, operation/cookie0, phase/ledger0 and stages8 (clean closure).
**No 3D job was issued**, no COLOR/Z oracle pass occurred and no FPS was
measured. The frontend reported ENOTSUP and the collector correctly failed
without retry or manual GEM cleanup. The original failure evidence is intact.

The source guard changed from fresh reasons0 to INTERFERENCE32/UNPROVEN after
the native 2D producer interval. The actual source route explicitly poisons
the first-owner lineage on any producer after startup. Ending that interval
does not restore it. The ordinary matrix source claim therefore cannot
follow this fill. `source-predecessor-model/RESULT.json` reproduces the
refusal with the actual extracted claim/producer code and frozen guard, on
native/UBSan/i486. It also proves that merely claiming the source before the
fill still poisons it. This is an integration gap, not an encoder, shader,
geometry or GPU retirement failure.

All controls returned to N. Stock boot
`272f9114-00d8-4ced-bad4-ddd2cf717409`, its exact module note, absent experimental
endpoints, restored GRUB and no pending operations were verified. No new
collected kernel fault lines appeared relative to the pre-test dmesg; the
existing backlight and ACPI boot warnings are preserved in the audit. All 31
protected artifacts remain unchanged. The 640x640 physical attempt was not
run after this refusal.

The next component is an explicit owned 2D-to-3D source/lifetime contract
using the existing GEM/private owner, reservation, IRQ cookie, retirement,
MMU restoration and coherency machinery. It must account for the native fill
before any FIFO/base publication and carry its attributable completion into
the subsequent 3D admission. It must refuse foreign producers, wrong owners,
late events, incomplete retirement and uncertain restoration. Preserve the
old frozen LAB guard and its rejection behavior; do not clear its reasons,
invent a fresh lineage, skip COLOR clear or suppress producer reporting to
make this test pass. Qualify that contract offline before the next fresh
physical boot. Full OpenGL 2.1/GLES 2.0, desktop coexistence, continuous normal
submissions, performance and installability remain unfinished.
