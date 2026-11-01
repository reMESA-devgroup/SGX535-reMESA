# Bound state-upload PDS and USE auxiliaries

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
