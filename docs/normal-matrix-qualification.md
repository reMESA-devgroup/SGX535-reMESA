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
offline image and parent/script/module identity checks. The new image has
not been installed or booted as part of this preparation. No new physical
OpenGL pass, FPS gain, full OpenGL 2.1/GLES 2.0 support, complete desktop or
installability is established by these tools. First qualify 1280x800 in a
fresh recoverable boot; qualify 640x640 separately only after a valid result.
