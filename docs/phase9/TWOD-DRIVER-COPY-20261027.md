# Native 2D copy preparation, independent of PDS

The user redirected development to the driver/2D path. The initial component
is `mesa/sgx535/remesa_twod.c/.h`: a bounded prepare operation for the native
gma500 2D FIFO SRCCOPY packet. It is registered in the Mesa overlay generator's
source list and Meson static-library target. It does not submit hardware or
replace the existing Gallium CPU transfer callback, and no acceleration cap
or broader OpenGL/GLES admission is advertised.

## Command and memory contract

The packet contains exactly10 DWORDs/40 little-endian bytes: FENCE, destination
surface and address, source surface and address, source origin, SRCCOPY BLIT,
destination origin, size, FLUSH. It contains no PDS/USSE program or artificial
CODE padding. The FENCE header is an engine packet element, not proof of job
completion, retirement or an application fence.

The request names two allocation identities, linear ARGB8888 surfaces, their
GPU addresses/allocation lengths, dimensions and pitches, and a rectangle.
All allocations must fit one aligned256MiB request-base window. The preparer
checks nonzero identities, 32-bit/window bounds, four-byte alignment,15-bit
pitch,12-bit coordinates/sizes, full storage bounds and every rectangle edge.
The returned spans bound logical pixel rows, not a proven GPU bus-fetch width.

Same-allocation copies require identical layouts and preserve overlap ordering
in all four directions. Different identities cannot have overlapping virtual
allocation ranges. Physical alias/ownership/permission checks remain kernel
backend responsibilities; userspace-supplied metadata is not authorization.
Dimensions equal to8 are refused because the original gma500 callback records
an eight-pixel erratum. There is no hidden CPU fallback for an admitted packet.
Unsupported conversion, scaling and other formats are not silently approximated.

All validation precedes output assignment; failures leave output and request
unchanged. Preparing bytes does not issue MMIO, allocate GPU buffers or prove
that they are mapped/initialized/owned correctly.

## Offline validation

The original public-kernel `psb_accel_2d_copy` and its direction helper were
compiled unchanged into a private CPU oracle. Only its submission sink was
replaced with packet capture; no pixels or device operations were simulated
as hardware. Exact source identity and extraction are in:

`/home/gama/sgx535-offline/twod-driver-20261027/RUN.py`

`/home/gama/sgx535-offline/twod-driver-20261027/RESULT.json`

Native, UBSan and i386 each pass366 checks, including38 exact original-kernel
packet comparisons:1280x800,640x640,641x479, minimum/large dimensions, overlap
directions, independent pitches and a final32-bit-window-page case. Negative
cases include zero/erratum/oversized rectangles, coordinate overflow, malformed
base/address/length/pitch/format, identity collisions, aliases and reserved data.
These dimensions are **packet tests only**, not physical rendering results.

`MESA_FLAGS_COMPILE.json` records the new object compiled with the actual Mesa
i386 flags. `PRIVATE_ARCHIVE.json` records a private Mesa archive containing the
new symbol and38 unchanged original members. `OVERLAY_CHECK.json` verifies
that the pinned Mesa overlay stages both files, registers the source in Meson
and refuses overwriting an existing output. The selected runtime/build cache
was not replaced and nothing was installed on the Mini12.

## Next implementation and physical gate

The next component is a typed kernel endpoint that resolves allocation IDs to
held GEM pages, validates virtual/PFN ranges and permissions, publishes CPU
writes, serializes the shared2D engine and `BIF_TWOD_REQ_BASE`, and submits only
the reconstructed bounded packet. It must record an authenticated operation,
wait for FIFO/BLIT completion, handle errors without unsafe global resets, and
retain resources on uncertain completion. Idle polling alone must not be
presented as an application fence or a general retirement mechanism.

Then validate on the SGX535 rev121 with source/destination guards, exact
readback, submission/completion/retirement and cleanup, paired1280x800+640x640
and occasional641x479. Kernel-header compilation and matching CPU packets do
not satisfy that gate. Fill/clear and Gallium dispatch follow the qualified
native path. The existing3D/GLXGears path and its evidence remain intact.

The interrupted private-MMU table-publication experiment was moved intact to
`/home/gama/sgx535-offline/private-mmu-publication-draft-20261027/` as an untested
draft, with its patch/header/source snapshots. Qualified owner source was
restored before this2D change; no draft was installed or used as a safety proof.
