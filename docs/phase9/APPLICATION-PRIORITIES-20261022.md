# Application priorities

Owner update2026-10-22: **GLXGEARS → Minecraft Java Beta1.7.3**. Doom is deferred.

Official vanilla b1.7.3 client and Linux libraries preserved and hash-verified in
`/home/gama/sgx535-offline/minecraft-b173-official-20261022/`.
Client SHA1:43db9b498cb67058d2e12d394e6507722e71bb45. Java8 profile and ELF32/i386
LWJGL are confirmed from preserved official artifacts; no game/launcher executed.
Mini12 currently has no Java command. Assets/runtime/login remain separate setup.

Real X server query:
`/home/gama/sgx535-offline/mini12-glx-capabilities-20261022T172842Z/`.
DRI2/GLX/Present advertised, DRI3 absent, desktop1280x800. This is not evidence
of an accelerated drawable or application. Mesa25.2's compiled DRI3 GLX route
therefore needs a compatible alternative transport on this server.

The restricted original-OpenGL triangle physical milestone remains preserved.
General GLX, depth, transform/lighting shader support, texture sampling, framebuffer
preservation and continuous submissions remain incomplete. No FPS is claimed.
ADMISSION and28 gates unchanged; no new GPU submission is authorized by this note.
