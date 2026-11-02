# Retained captures through Gallium's writer lifecycle

`remesa_context_submit_captured_scene` submits one existing frozen capture and
keeps it alive. It publishes the same shared pipe fence to COLOR, DEPTH and
the context as the existing prepare-and-submit entry points. Both routes use
the same pass reservation, native result handling and writer publication code;
the original entry points still consume a successful capture.

The new entry point verifies the current attachments against actual retained
objects through `remesa_persistent_scene_capture_validate_attachments`. A caller
cannot substitute a matching numeric handle or dimensions: membership in the
held graph, exact COLOR/DEPTH roles, generations, mapped addresses and allocation
extents must agree. It checks the frontend output against both context/resource
state and provider metadata before reading or writing the output pointer.
It refuses mapped attachments, a queued native clear or a changed framebuffer.
It does not reinterpret a frozen capture using current shader or vertex state.

After synchronizing any preceding attachment writers, it reserves both passes
on local copies. Conclusive NOT_ISSUED leaves the capture and prior pass state
intact. An uncertain submission poisons both attachments and the context and
retains backend ownership. A poisoned provider discovered before dispatch also
marks this context failed, so `flush` cannot return an older successful fence
in place of the failed attempt.

For the Linux [one-job session adapter](general-scene-provider-adapter.md), the
serialized caller prepares and retains the capture, opens that native session,
then calls this frontend entry point once per operation. Destroy returned pipe
fences normally. Texture reads and presentation snapshots use the existing
[retired COLOR READ lease](scene-color-frontend.md). Before releasing the capture,
the caller must close every READ lease and obtain acknowledged native session
CLOSE. The frontend does not infer close, release the capture on rejection or
silently reopen a consumed native source.

No buffer hash, oracle comparison, upload, clear, input rebuild or diagnostic
readback is added to submission. This does not prove arbitrary changing inputs
safe, replace kernel admission, enable ordinary draw dispatch or advertise a
new GL/GLES version. The current Linux general endpoint remains bounded LAB,
including its existing kernel input authentication. Production admission and
authenticated updates for changing application data are still needed.

## Verification

Evidence root: `/home/gama/sgx535-offline/captured-scene-frontend-20261102/v2`.
All driver objects compile together for i486 against the existing generic
Mesa/NIR libraries. The new frontend cases cover 1280x800, 640x640, 641x479 and
1600x768; repeated submission with no extra allocations/uploads; later
NOT_ISSUED and ambiguity; attachment mismatch; a live COLOR lease; output
aliases; stale captures; and prior ownership loss in the shared provider.
The previous COLOR transfer, context and vertex regressions run against the
same rebuilt objects. Focused COLOR lease regressions additionally compile
and execute the actual provider for native, UBSan and static i486/QEMU.

These tests use modeled memory, completion receipts and initial allocation
bytes. They do not render pixels or qualify physical execution, sustained FPS,
complete GL2.1/GLES2 or an installable driver. The previous physical results
continue to qualify only their recorded kernel, client and execution paths.

The new unwrapped `captured_scene_native_client` subsequently passed one
physical profile3 job at 1280x800. It prepares the same homogeneous Gallium/NIR
scene as before, submits it through the new captured-scene frontend, waits on
the shared pipe fence, and snapshots COLOR while the capture remains held.
That snapshot exactly matches the independent COLOR oracle. Separate COLOR/Z24,
all 13 READ inputs, three CPU/source owners, four guards, native control words,
retirement, publication and cleanup passed the outer audit. Seven controls
returned to N and stock recovery was verified.

Evidence:
`/home/gama/sgx535-offline/captured-scene-frontend-physical-20261102/v1-1280/FINAL_PHYSICAL_RESULT.json`.
This qualifies this single captured frontend/retained READ path. It does not
qualify the Linux general-session adapter, repeated physical frontend jobs,
ordinary GL or sustained FPS.

The same client subsequently passed a fresh profile4 job at 640x640 with the
same full retained-COLOR oracle comparison, independent COLOR/Z24/input/guard
checks, complete cleanup and verified stock recovery. The consolidated result
is `/home/gama/sgx535-offline/captured-scene-frontend-physical-20261102/FINAL_PAIR_RESULT.json`.
The new [general-session homogeneous extension](general-session-homogeneous.md)
remains a separate candidate requiring physical cold/warm qualification.

The optional build without Mesa headers also compiles after including the
complete by-value capture token for its refusal stub. The qualified full-Mesa
i486 screen object remains byte-identical, as recorded in
`/home/gama/sgx535-offline/captured-scene-frontend-20261102/v2/FALLBACK_FIX_PROOF.json`.
