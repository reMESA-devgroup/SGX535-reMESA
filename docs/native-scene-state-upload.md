# Bound state-upload PDS and USE auxiliaries

The original sparse checker below remains unchanged. The November 3 ordinary
frontend follow-up adds a separate complete constructor-image prerequisite;
it does not replace the LAB checker or enable a normal submission ioctl.

The general native validator now follows the TA state references through their
selected state-upload PDS and into the source DATA and count-specific USE
auxiliary. This is one part of indirect closure validation, not a complete
scene admission certificate.

`include/remesa/state_upload.h` checks the unchanged CPU producer's committed
PDS60 layout: DATA0 source address, DATA1 single-descriptor control, DATA2
task lower19/background, and all three CODE words. Available backing may be
larger than60, but extra capacity is not read or interpreted as CODE.
DATA3..11 retain their original unknown-mask status; nonzero values are not
silently declared invalid or treated as known zero. Matching these known
fields is not proof of the architectural meaning of all DATA cells.

The corresponding auxiliary checker validates exactly the existing16-byte
ordinary-MOVE/three-source CPU record pair, including the source-DWORD count
in both records. Its expected fields use the original encoders' arithmetic;
the encoders themselves are unchanged. No instruction is generated or added.
This is a selected numeric construction contract, not a general USSE decoder
or proof of runtime copy/termination semantics.

`remesa_gem_scene_states_check_locked()` requires the existing endpoint/MMU
locks and bound private root. It first runs the complete direct-TA and
action/control/USE checks. Caller-provided stable TA scratch receives that
validation result; the separate STATE output stays unchanged on any failure.
For each prefix/draw/close state reference, it checks:

- Actual PDS bytes with the expected11/14/2-DWORD profile.
- Complete read-only cached source backing and the selected source header.
- A matching immutable USE-window entry with vertex data master, exactly16
  committed bytes, and the actual count-specific auxiliary contents.
- The close DATA extent derived from the same control-bound dimensions.

The result records PDS, source and auxiliary held identities and offsets. It
does not allocate/map, walk the default page directory, hash/read back pixels,
flush caches, synchronize or issue MMIO. Required publication and retirement
stay in the existing executor. The helper neither initializes surfaces nor
clears incomplete/capability gates.

Evidence:
`/home/gama/sgx535-offline/native-scene-state-upload-20261101/v1`.
Joined kernel models exercise four dimensions, state/DATA/USE binding,
destination and unbound-task rejection, malformed CODE/auxiliary and close
extent, unknown DATA handling, transactional results and post-publication
stability. Separate native/UBSan/static-i486 tests compare the new checks with
unchanged original PDS and USE encoders for counts1..16 and four lower19 values,
plus three independent preserved physical carrier PDS objects under their
known-field contract. Reading those old files is not a fresh physical test.

The first standalone negative test changed a valid lower19 address bit and
incorrectly expected syntax rejection. It now corrupts an unsupported task
control bit instead. The kernel resolver separately checks address ownership;
no production check was tightened or weakened to accommodate that test.

Remaining work includes vertex PDS/index/vertex-source limits, fragment,
background and event closure, fuller ISP/state semantics and unknown-field
qualification, then ordered general native execution and physical validation.
This helper alone must not be used to admit arbitrary unknown DATA or advertise
full GPU/API support. The old canonical11 executor and LAB gates remain intact.

## Complete constructor images for the ordinary draw route

`remesa_state_upload_pds_image()` and `remesa_parameter_upload_pds_image()`
reuse the existing field/CODE decoders and additionally require the untouched
DATA cells to match the existing producer's zero initialization. This is an
exact construction constraint, not a new interpretation of unknown fields.
An object with different values can still pass the original sparse decoder;
it does not pass this additional prerequisite.

Both checks inspect only the committed 60 bytes. They neither read nor
require zero in remaining allocation capacity, add CODE padding, hash buffers,
transform vertices or execute the GPU. Decoded output is unchanged on failure.

`remesa_gem_scene_state_images_check_locked()` first revalidates the existing
held TA/state/source/USE graph, then checks every actual ISP/PARAM PDS upload
with these complete-image helpers. It allocates no additional storage and
uses the existing locks and views. Publication and retirement stay with the
existing executor. Its TA/STATE arguments are scratch and can change on
failure. Return zero covers these upload images only: source ISP semantics,
other PDS images and normal runtime admission remain separate requirements.
This function is not yet a live normal submitter.

The OpenGL call benefiting from this prerequisite is ordinary `glDrawArrays`
with the existing matrix/lighting vertex program. The frontend already builds
that draw; independently admitting its complete uploads is necessary before
connecting the normal Linux execution callback. No shader or encoder was
reimplemented, and no LAB catalogue was broadened.

The focused CPU test uses the actual unchanged state encoder:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Imesa/sgx535 \
  mesa/sgx535/tests/test_state_upload_images.c \
  mesa/sgx535/remesa_pds_structure.c -o /tmp/remesa-state-images
/tmp/remesa-state-images
```

The joined retained-core model runner also executes `--state-images`, including
held-graph and reserved-byte refusal cases at 1280x800, 640x640, 641x479 and
1600x768. November 3 evidence is under
`/home/gama/sgx535-offline/native-state-images-20261103/v1/`: 68 producer cases
and 25,984 reserved-DATA/CODE bit mutations per native/UBSan/i486 variant,
comparison with preserved ISP15/PARAM carriers, and the held-graph checks.
These are offline results, with zero GPU operations and no performance claim.

Rollback leaves the additional image prerequisite unused until the normal
runtime has the remaining complete-input authority. The original sparse
checker and consumers keep their contracts. No runtime gate is enabled.
