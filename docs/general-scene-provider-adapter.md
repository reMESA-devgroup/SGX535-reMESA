# One native session operation per provider submission

The Linux memory provider can now bind a retained scene capture to the existing
general session. `remesa_linux_memory_general_scene_open` freezes the exact
capture packet and opens that session after checking its owned backing. OPEN
does not submit a GPU job. The adapter is explicitly restricted to the existing
bounded kernel LAB profiles; it grants no new shader or graphics admission.

Install `remesa_linux_memory_general_scene_dispatch` as the persistent provider's
native callback before allocating resources. Prepare a capture, snapshot its
packet, then open the adapter. Each call to
`remesa_persistent_scene_capture_submit_keep` now performs exactly one native
SUBMIT through that callback. The Linux transport verifies the real kernel
operation, retirement, publication and acknowledgement before translating the
result into the provider's own context and operation. Host operations must
increase and the captured packet must stay identical. The packet comparison
checks small ownership metadata, never GPU buffer contents.

Destroy each published fence normally to recover its bounded job slot. COLOR
can use the existing retired READ lease between jobs. Close every READ lease,
obtain acknowledged `remesa_linux_memory_general_close`, then release the
capture and its transitive owners. A consuming scene submission is incompatible
with keeping this native session open. All operations require external
serialization. The adapter does not select a gate or close sessions implicitly.

A changed packet, repeated host operation, exhausted LAB budget or live CPU
lease refuses submission locally. An entered ioctl failure or malformed native
receipt poisons the endpoint; subsequent submissions report ambiguity and no
resource may be freed through it. No automatic retry, replay loop, per-job INFO,
hash, readback, dump or presentation is added.
The kernel's existing per-SUBMIT LAB input authentication remains in place.
The adapter adds no such work itself; this is not a hash-free production path
or a measured performance improvement.

The general transport also rejects host input/output aliases with its endpoint,
CPU leases and each other before issuing an ioctl. An output inside the opaque
endpoint could otherwise overwrite lifetime state when a raw response was
copied back. Runtime kernel address, permission, ownership, coherence and
retirement validation remain independent and mandatory.

This joins existing session, capture and fence lifetimes. Dynamic authenticated
input updates, arbitrary native program admission, ordinary GL dispatch,
complete GL2.1/GLES2, desktop integration and installation remain unfinished.
The original physical client replaying two jobs inside one callback remains a
separate diagnostic collector; it is not evidence of two provider submissions.

## Verification

Evidence: `/home/gama/sgx535-offline/general-scene-provider-adapter-20261102/v1`.
The new C test exercises the actual Linux adapter with interposed syscalls:
three host operations produce three native SUBMIT calls, with no per-job memory
queries; packet changes, aliases, repeated operations and capacity exhaustion
are refused; acknowledged close permits cleanup; a later malformed receipt
retains and poisons every allocation. The existing transport suite additionally
checks every native response byte and errno failure for OPEN/SUBMIT/CLOSE.
These tests model GPU completion and do not render pixels or measure FPS.

Run `tools/remesa/test_private_session_transport.py --output NEW_DIRECTORY`.
Use its paired `--i486-cc` and `--qemu-i386` options for the target variant.
