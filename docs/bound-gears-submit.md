# Bound gears submission with the existing shared fence lifecycle

`remesa_context_submit_gears_attribute_scene` now submits the explicit bound
gears capture through the same ownership and render-pass implementation as
the existing linked scene API. The ordinary draw callback, capabilities,
kernel gates and working GLXGEARS/FAST route are unchanged.

Both public submission wrappers select one shared implementation. The gears
wrapper uses the validated bound gears preparation and additionally refuses
output aliases with bound constant resources. Before dispatch, the code
allocates the single fence and reserves COLOR/Z pass transitions on local
copies. A conclusively unissued draw releases its capture and leaves both
attachments unchanged. Uncertainty marks both attachments and the context
failed and suppresses retries and stale flush fences.

Successful backend authentication publishes one fence shared by COLOR, Z,
the context and the caller, and marks contents available only at that point.
Normal resource waits still require authenticated backend retirement and
publication. The new wrapper does not fabricate a native receipt or bypass
the matching kernel transport/catalogue requirements.

Evidence is under
`/home/gama/sgx535-offline/bound-gears-submit-20261101/v1/`.
The coherent current Mesa/NIR build passes 17 gears submission scenarios,
including four target sizes, user/resource indices, bind-time constants,
aliases, mapped resources, ordinary and uncertain allocation failure,
conclusive nonissue followed by retry, ambiguous issuance and an unacknowledged
receipt. The actual shared implementation and fixture also pass UBSan.

Tests verify COLOR/Z waits, the common context flush fence, survival across
context and attachment teardown, and final caller ownership. All 26 prior
context cases and 14 bound preparation cases pass. Four old and six bound
capture trees remain byte-identical; all 31 protected artifacts are preserved.
Native callbacks are modeled and produce no pixels. These results do not
constitute a new physical draw or FPS measurement.

The immediate remaining physical work is the native client and collector:
actual gears NIR/constants/vertex bindings, the separately registered transport,
readbacks inside the authenticated callback before temporary capture owners
are freed, complete trace decoding, independent oracles and the recoverable
boot image. The existing module has not been installed by this change.
Full OpenGL/GLES admission, continuous sessions and complete driver installation
remain unfinished.
