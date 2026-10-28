# DMA-BUF / DRM PRIME presentation audit

The Reddit proposal is relevant to eliminating framebuffer copies, but is not
currently an available toggle. No FPS gain has been measured or claimed.
Read-only physical evidence:
`/home/gama/sgx535-offline/prime-presentation-audit-20261028/{remote.py,RESULT.json,SUMMARY.json}`.
This audit made no GPU submission, export/import, allocation, presentation,
module/control change or boot change.

On recovered STOCK boot `cbb8f28e-ceca-4552-961c-a890edfa81e7`, kernel
5.10.240-antix.1-486-smp, GET_CAP(DRM_CAP_PRIME) returned0. The query uses the
actual5.10 UAPI (cap5,16byte argument, ioctl0xc010640c), opened and closed
card0 read-only. The qualified source's drm_driver contains no PRIME hooks.
The current native2D stager does not add them. This is not a claim about future
PRIME support or another kernel.

An authenticated xdpyinfo query on the real desktop succeeds at1280x800:
Present and MIT-SHM are advertised; DRI3 is absent. Xorg logs contain
"Initializing extension DRI3", but that line alone does not establish a
usable DRI3 extension on the screen. A DRI2 extension name also does not
establish hardware GL support (the log says Screen0 is not DRI2 capable).
The modesetting DDX is in use.

The [Linux5.10 dma-buf contract](https://www.kernel.org/doc/html/v5.10/driver-api/dma-buf.html)
requires exporter/importer lifetime, device mapping, coherency and fence /
reservation synchronization. Sharing a file descriptor alone does not provide
those guarantees. CPU-access bracketing is distinct from waiting for GPU work.
Do not export the custom GEM backing through a shortcut which bypasses retained
jobs, file permissions, quarantine, bounds or GPU/CPU ownership.

The [XDC2022 implementation presentation](https://indico.freedesktop.org/event/2/contributions/60/attachments/66/107/XDC-20220-DRI3-modesetting-without-glamor.pdf)
describes DRI3 importing/exporting pixmaps, GBM allocation, linear modifiers
and Present, including a modesetting approach without glamor. That demonstrates
a possible integration approach, not proof it works on this Mini12 stack.
Present may still copy rather than flip. Screen-format, pitch, mapping and
scanout compatibility must be verified independently.

## Implementation order and measurements

1. Correct and physically qualify the existing native2D ownership/completion
   contract; the phase8 MMU fault is still a blocker to promoting native2D.
2. Implement actual GEM PRIME lifetime/export/import hooks with reservation
   fences and safe shared backing. Preserve memory isolation and quarantine.
3. Add userspace resource-handle import/export and the Xorg DRI3/Present path,
   matching format/stride/modifier and waiting for safe buffer reuse. Do not
   infer the correct import/mapping contract from extension names.
4. Compare the same physical1280x800 workload against qualified presentation:
   GPU submission/completion, coherency, CPU copies, X11 and total wall time,
   including sustained FPS and resize behavior. Keep only demonstrated gains.

One32bpp1280x800 frame is4,096,000bytes. Eliminating one complete frame copy
would remove245,760,000bytes/s of logical copied payload at60FPS. These are
arithmetic quantities, not measured bandwidth, eliminated DRAM traffic or a
predicted FPS result. DMA-BUF does not itself reduce shader/geometry execution.

"Limit components sent to GPU" should mean avoiding redundant uploads and
state: reuse immutable vertex/index buffers and update only changed uniforms
or resources. Omit unused attributes only after the actual pipeline proves
them unused. Preserve complete scene geometry, depth, normals and image
correctness. Do not reduce resolution, drop gears/triangles or return to CPU
rendering to inflate FPS. Existing validated geometry/shaders are preserved.
Instrument the real active draw path before changing its packing or caching;
source-level opportunities alone do not prove a physical bottleneck.
