# Distinct frontend operations on one native session

`captured_session_native_client` joins the existing captured frontend with the
Linux general-session adapter. It prepares the same homogeneous scene once,
opens profile 3 or 4 explicitly, and calls
`remesa_context_submit_captured_scene` twice. Each call obtains its own provider
operation, native SUBMIT and pipe fence. Each operation has a separate retained
COLOR snapshot. The client acknowledges native CLOSE before releasing the
capture; it never repeats both GPU jobs inside one provider callback.

This client is a bounded Gallium/NIR qualification tool. It does not claim two
OpenGL API draws, arbitrary changing inputs, presentation or sustained FPS.
Its existing encoders, shader/geometry inputs and catalogues remain unchanged.
The matching general-session homogeneous module extension is required.

The Linux transport has an explicit fresh-endpoint diagnostic opt-in for this
LAB collector. When enabled, it saves the actual request and returned response
around an entered ioctl, including uncertain responses. Pure host getters copy
those saved envelopes without another ioctl or retry. Recording is disabled by
default; ordinary adapter calls do not copy diagnostic records. This does not
alter any admission, native validation or error handling, and adds no frame
hash, pixel readback or diagnostic log to production FAST.

The separate Python supervisor checks the two native envelopes, distinct host
operations, exact profile plans/control words, immutable READ/CPU owners and
guards, independent COLOR/Z24 expectations, and both retained frontend COLOR
reads. It accounts for the source vertex buffer retained by the frontend in
addition to COLOR/Z and guards. Cleanup requires complete evidence and idle
kernel INFO for every remaining object; failure suppresses cleanup and retry.
Boot/module identity, gates, source ownership and stock recovery still require
the outer bounded physical experiment controller.

Default invocation only creates a plan. `--execute` is explicit, borrows one
fd for the native child and never enables a gate, installs a module, reboots or
presents pixels. The actual hardware binary contains no ioctl interposition;
the separate model binary uses synthetic syscalls and never renders pixels.

Evidence root:
`/home/gama/sgx535-offline/captured-general-session-client-20261102/v2`.
The native client has 12 i486 syscall-model cases across both sizes, covering
preparation, success, OPEN, first/second SUBMIT and CLOSE failures. Eight old
one-shot client cases pass. Seven transport targets pass across native, UBSan
and static i486/QEMU. Archive validation has 16 consistency/tamper cases, and
the supervisor has 12 cleanup/failure cases. Both default plan commands refuse
to access a nonexistent device. These are offline tests, not hardware approval.

The archive verifier's positive unit fixture is explicitly synthetic: modeled
envelopes, rebased prior plan metadata and existing CPU expectation bytes.
It always reports zero proven GPU operations and no physical qualification.
These fixtures must never be uploaded as hardware evidence or a rendering
payload. The unchanged pixel oracles and prior physical artifacts are retained.

Physical cold/warm qualification of this joined path is still pending. Complete
GL2.1/GLES2, dynamic authenticated inputs, production submission, desktop
integration and an installable driver remain unfinished.
