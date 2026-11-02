# Owned gears parameter commands in the ordered TA stream

The common owners now build the same ordering used by the existing physical
gears producer: draw state, parameter upload, indexed draw. The previous
program/PDS integration is described in [gears-common-pds.md](gears-common-pds.md).
This change does not qualify a new hardware submission or expose general GL.

`remesa_persistent_gears_attribute_state_prepare` reuses the existing ISP,
fragment, viewport and scalar encoders. Its distinct state kind retains the
gears PDS closure. `remesa_persistent_gears_attribute_body_prepare` emits
state8/PARAM8/index20 per draw and the existing close12. All addresses derive
from held owners. The legacy state/body constructors continue to refuse this
profile. No instruction or artificial CODE padding is introduced.

The parameter block keeps 36 coefficient words, a self-address prefix, and
three initialized padding words: 37 effective source words and 40 transferred
words, or 160 bytes. The PARAM PDS has 60 effective bytes, including 12 bytes
of CODE, and its EE auxiliary has 8 effective bytes. These sizes remain
separate from allocation capacity. The body checks parameter, vertex,
fragment, state and close USE bank compatibility before publication.

The TA owner retains prefix and body, and therefore every state, program,
raw geometry and parameter dependency. Its READ inventory includes the extra
parameter allocation. Scene TA/USE snapshots include the parameter PDS and
the EE entry. Releasing public parent references does not release storage
still retained by the TA owner. Snapshot inspection performs no provider IO.

## Independent kernel checks

The original TA walker is unchanged. A separate strict walker accepts only
the ordered parameter grammar, including the existing optional initial state,
one through sixteen draws, and the exact source37/transfer40 command. It
validates the complete syntax before invoking a resolver. The kernel selects
the grammar before resolving any address; a permission failure cannot cause
a retry under a different grammar.

The state checker validates the actual retained PARAM PDS, its source address
and 160-byte READ/cache1 extent, the self-address prefix, zero transfer
padding, and the actual held vertex USE entry and EE8 bytes. These checks use
the existing private MMU and retained-job prerequisites. They do not add an
arbitrary address, shader, memory permission or execution grant.

The subsequent [complete capture integration](gears-scene-capture.md) replaces
the temporary vertex-stage refusal with kind/draw reference lookup, the exact
320-byte program predicate and the retained 48-byte fetch contract. Fragment
validation similarly selects the authenticated draw-state upload instead of
relying on upload ordinals. Legacy triple indexing is not reused for PARAM
streams. The closed physical profile catalogue still needs a separate exact
extension before this graph can be submitted on the Mini 12.

## Verification

Artifacts are retained at
`/home/gama/sgx535-offline/gears-ordered-ta-20261101/v1/`.

The real Mesa/NIR ownership fixture covers 1280x800, 640x640 and 641x479,
with and without an initial state, and one or sixteen ordered draws. It
checks encoder output against independent decoders, READ closure, malformed
commands, source/PDS/EE corruption, no-IO snapshots, and dependency lifetime.
Native, UBSan and i486 fixtures accompany the previous linked-state tests.
Kernel tests use the actual transformed retained core, private loan and MMU
auditor with modeled primitives; they do not simulate GPU rendering.
Four previous capture trees and all 31 protected artifacts remain unchanged.

No Mini 12 operation, new FPS result or physical approval is implied by these
offline results. The working GLXGEARS path and FAST loop are unchanged.
