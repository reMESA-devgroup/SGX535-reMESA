# Explicit native COLOR initialization

`remesa_persistent_linked_targets_color_initialize()` initializes the complete
committed COLOR plane through the registered native fill callback before scene
capture. Target identity, COLOR handle, pitch, height and bounds come from the
live retained target graph. The caller supplies only the target owner and packed
BGRA bytes. Padding columns up to the pitch are included; page tail is excluded.

This is an explicit operation. Each successful call issues a fill, even when the
color matches the previous request. There is no automatic per-frame clear,
clear/preserve decision, initialization cache or certificate. A previous partial
writer or readable flag is not used as proof of full initialization. The fill
callback's existing zero-return contract attests synchronous authenticated GPU
completion, publication and retirement. No CPU fallback or extra readback/hash
is added. That callback contract is not itself new physical qualification.

CPU leases and frozen scene references refuse the operation. The typed target
can retain its COLOR child after the child's public owner is deleted. Only this
validated internal operation may use that retained child: ordinary public
fill/copy still require public ownership. Source/destination type, address,
window, extent, lifetime and failure checks remain unchanged. Ordinary callback
errors mean not issued; uncertainty retains/quarantines the destination using
the existing path. No scene admission bit or graphics capability is enabled.

Tests cover1280x800,640x640,641x479 and1600x768; complete pitch/height/stride and
handle binding, explicit repeated calls, missing callback, active leases,
ordinary failures, retained children, unchanged public access and uncertain
returns. The callback model records requests without writing CPU pixels. A
scene-capture regression verifies initialization returns EBUSY while frozen.
The1600x768 case is host/callback-model coverage, not physical Vaio qualification.

Evidence root: `/home/gama/sgx535-offline/linked-color-initialize-20261031/v1`.
`source-tests-v2/result.json` records native/UBSan/statici486 results.
`actual-mesa-color-initialize-v2/RESULT.json` records a private persistent-object
rebuild with actual Mesa/Gallium/NIR and unchanged parent archive/other members.
`REGRESSIONS.json` records captured scene controls plus the existing native
fill/copy frontend writer gates. `INITIAL_FAILURE.json` preserves a test-only
counter-baseline failure and verifies the driver object was identical before
and after that correction. `FINAL_BINDING.json` pins final source and receipts.
No GPU operation, new physical initialization result, full API/desktop support
or installed-driver qualification is claimed by these tests.
