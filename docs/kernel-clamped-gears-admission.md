# Kernel validation of the native COLOR clamp variant

The native vertex checker now recognizes the separately encoded CODE344
variant described in [vertex-color-clamp.md](vertex-color-clamp.md). The old
CODE320 predicate and its literal word table remain unchanged.

The new body predicate checks the three existing COLOR0 clamp records and
validates the original 312-byte lighting body plus the relocated 8-byte tail
through the old predicate. Its temporary 320-byte array exists only for CPU
validation. It never emits or uploads shader instructions. The helper has an
explicit noinline boundary for the Mini 12's 8KiB kernel stack.

The vertex checker accepts a 344-byte USE entry only when the already checked
TA/state plans identify that draw's homogeneous state15 upload. It resolves
the upload by PDS identity and offset, rather than assuming a positional
triple. The authenticated effective source length must be 60 bytes. The
64-byte reserved TA capacity alone does not establish state15: state14 also
fits that capacity and continues to refuse CODE344.

| Quantity | Meaning |
| --- | --- |
| 344 bytes | Committed vertex CODE, including the real clamp and typed tail |
| 352 bytes | Reserved USE storage in the userspace producer |
| 60 bytes | Effective state15 source consumption |
| 64 bytes | Aligned state source capacity |

A USE entry declaring 352 bytes as CODE is refused. The body predicate can
inspect a larger available buffer without treating its spare bytes as extra
instructions. Existing address bounds, READ/cache permissions, source37 /
transfer40 parameter validation, index bounds, ownership, publication and
retirement checks are preserved.

## Offline evidence

Evidence is retained under
`/home/gama/sgx535-offline/kernel-clamp-admission-20261102/v1`.
The current kernel core is rebuilt against the current staged body header;
an initial diagnostic accidentally selected the older flattened header and
its compiler refusal is preserved.

Native, UBSan and i486 runs cover the existing legacy graphs and both actual
OpenGL CODE344 captures at 1280×800 and 640×640. Each variant also refuses all
2,752 single-bit changes in the committed CODE. Full kernel-model faults
refuse altered body/clamp/tail bytes, declared CODE sizes 320/343/352,
state14 with CODE344 and an inconsistent PDS count before backend stage0.

Changing the eight spare USE bytes is accepted by the internal structural
checker because those bytes are not instructions. This control does not
exercise the separate closed LAB catalogue: its full READ-image hashes still
apply, and the new captures are not registered there.

The model's execution/retirement callbacks are simulated. No GPU execution,
pixel readback or new physical approval follows from these tests. The
registered public profiles remain the four previous profiles, and the
userspace pending-raster guard remains blocking. Physical testing needs a
separate exact input catalogue and complete raster admission first.
