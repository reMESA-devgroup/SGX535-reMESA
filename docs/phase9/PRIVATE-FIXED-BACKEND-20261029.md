# Borrowed private GEM connection to the fixed backend

The opt-in private staging profile now connects the retained fixed backend's
three pre-issue stages to the private GEM loan. Normal profiles keep their
original generated source replacements, checked against local commit e660b8d.

`sgx535_gma500_fixed_backend_begin_private()` requires an already bound private
root, the endpoint mutex followed by the MMU write semaphore, and retained device,
producer, power and lifetime exclusion. It authenticates the borrowed owner's
resource order, descriptor IDs, sizes, page arrays, CPU view identities and GPU
addresses against the actual GEM job. LOCAL descriptors must have CPU-only rules
and no GPU reservation; GPU descriptors must remain GPU-visible. A device/guard
mismatch, malformed owner, stale views or previously published loan is refused.

Admission then uses the existing guarded begin, including the original source
history and HOST_IMAGE_FINAL gates. The backend copies the view descriptors; the
loan retains the backing and mappings. Caller view-array edits cannot redirect
later publication. No allocation or user pointer is introduced.

The original `fixed_run` stage sequence and observation handling remain in place:

* PUBLISH_CPU calls the retained GEM cold-upload publisher; it preserves borrowed
  mappings and skips CPU-only sidecars.
* PUBLISH_TRANSLATIONS checks the private-bound job closure and requires the CPU
  publication receipt, without the original default-directory getter.
* DEVICE_MAINTAIN requires that receipt and invokes the existing already-locked
  private maintenance function, avoiding recursive acquisition of the MMU writer
  semaphore.

Errors propagate through the original stage result. No fallback to the legacy
publisher, default PD, or public locking flush is attempted on a private error.
The ordinary branch keeps those original calls. The existing successful backend
end returns IRQ ownership using the existing retirement gate, then clears the
backend's private loan/view metadata. It neither releases the GEM loan nor
restores/frees its private root. Failed end retains that metadata.

This is not an executable private-context driver yet. No registration, ioctl,
root bind, command admission or retirement bypass was added. The caller still
needs a qualified whole-interval MMU writer/exclusion protocol, checked restore,
and safe retention/deferred default flush behavior after uncertain completion.
The existing exclusive root helpers release their own semaphore; blindly wrapping
the old executor or those helpers in an outer write lock would recurse. The new
entry must not be exposed before that integration is complete. General GL/GLES2,
full desktop 2D, VA-API and installation remain unfinished.

## Validation

`/home/gama/sgx535-offline/private-fixed-backend-20261029/v3/GUARD_RESULT.json`
records native, UBSan and i386 runs. The new adapter suite has 225 checks per
variant, with the actual adapter include and verbatim extracted pre-issue switch
arms; MMU/publication/IRQ callbacks and scene containers are modeled. It verifies
metadata refusals before publication, propagation of validator/begin/publication/
maintenance errors, required publication ordering, copied views, invalid payload
refusal, and that the private branch never calls legacy publishers/getters/flush.
The normal branch's original calls remain exercised. These are CPU tests, not
physical cache, submission or retirement evidence.

Existing IRQ handoff, reservation, event-fault and native 2D suites pass in all
three variants. Inherited reservation checks are not independent extra coverage.
Six real antiX kernel translation units compile with private/copy/vendor/fill
flags and partially link; all new driver symbols resolve. This is not a complete
module link or installation. `LEGACY_STAGING.json` compares five non-private
source replacement profiles byte for byte against e660b8d; it does not assert
whole module binary identity.

The unchanged GEM publication suite passed 786 checks per variant in v1's
RESULT.json. v1's initial adapter compile failure was a test-only indentation
warning and is preserved. v2's 204-check preliminary adapter run is superseded by
v3's null-device and additional metadata/error cases. No Mini12 writes or GPU
operations occurred during this qualification.
