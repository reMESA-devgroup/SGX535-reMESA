# Actual desktop integration target

The requested product is the user's normal1280×800 desktop rendered through
reMESA, with2D and3D in the same general driver. Isolated color/copy programs
are diagnostic evidence, not that delivery. Minecraft remains an application
target through general OpenGL, not an application-specific renderer.

## Current live desktop

Read-only inventory on stock boot
`5acedcdc-692f-4bb9-b3cd-63cebd71dbd3` confirms Xorg(pid1779) and IceWM(pid2816).
System packages are Xorgcore `2:21.1.16-1.3.0nosystemd1` and Mesa EGL/DRI
`25.0.7-2`. The current Xorg log loads modesetting, tries glamor, reports
`Refusing to try glamor on llvmpipe` and `glamor initialization failed`, then
initializes the DRISWRAST GLX provider. The current desktop is not SGX-accelerated.
The new native2D resource factory is not connected to this Xorg render path.

Evidence root:
`/home/gama/sgx535-offline/remesa-desktop-integration-20261028/v1/`.
`LIVE_XORG_INVENTORY.json`, copied current log/config files and `FINDINGS.json`
bind the live observations. No Xorg module/configuration or library was replaced
by this audit. Xorg development headers are not installed on the Mini12; this
is build preparation information, not authorization to disturb its packages.

## Shared driver boundary

Matching upstream Xorg21.1.16 source was read from the already retained archive,
SHA256 `b14a116d2d805debc5b5b2aac505a279e69b217dae2fae2dfcb62400471a9970`.
`XORG_SOURCE_BINDING.json` records extracted source hashes. Its modesetting
`try_enable_glamor` loads `glamor_egl_init` on the DRM FD. The EGL side creates
a GBM device/display and a real surfaceless GL context, with desktop GL2.1 or
GLES2 fallback checks, and requires `GL_OES_EGL_image`. Glamor further requires
working shader/texture/FBO/buffer/state operations and relevant extensions.
Merely advertising a version is not implementation or qualification.

The intended common route is:

```text
normal desktop / XRender / window damage
    -> Xorg modesetting + glamor
    -> EGL/GBM + Mesa Gallium
    -> shared reMESA resources, shader compiler, native jobs and synchronization
    -> SGX535 + owned presentation/scanout resources
```

Application3D must use the same underlying driver contracts. The existing
restricted gears path and the qualified native copy/clear primitives remain
preserved while general admission is built. Native2D private buffers alone
cannot replace the Xorg renderer. No independent color demo or a CPU-rendered
frame uploaded for display counts as this desktop goal.

## Concrete missing contracts

1. General DRI/EGL context admission backed by real GPU shader/texture/raster/
   render-state/color-preservation support. Current `remesa_dri_create_screen`
   deliberately refuses those missing contracts; its gate must remain honest.
2. GBM/image/FBO/pixmap/scanout backing ownership and coherency. Current native
   memory allocations are private and display GTT overlap is excluded. An
   unowned display address must not be mapped to bypass this boundary.
3. The textured shader draws used for glyphs, XRender composite and window
   contents. Solid fill/copy alone does not implement the whole desktop.
4. Safe Xorg and application3D coexistence. Current experimental admission is
   privileged/exclusive and exact-file/context bound; it is not unprivileged
   multi-client isolation. Resource lifetime and engine ownership must extend
   to that real workload before desktop installation.
5. A reversible installation with a stock fallback and actual desktop checks:
   normal windows, text, scrolling, damage/redraw, resize and simultaneous3D.
   Verify physical submissions/retirement and correct displayed results;
   measure interactive frame times and fallback scope explicitly.

Do not force glamor to accept llvmpipe, spoof a renderer, remove missing-GL
gates or equate X11 presentation with GPU rendering. Do not replace the user's
working Xorg configuration until the replacement is concrete, offline checks
pass and a recoverable physical integration is ready. The earlier white flash
remains unexplained; sampled unchanged sysfs state does not qualify coexistence.

The full desktop/integrated3D/installable-driver objective remains active and
incomplete. The [owned2D factory](NATIVE-2D-FACTORY-20261028.md) is a validated
building block for the common resource lifetime, not desktop completion.
