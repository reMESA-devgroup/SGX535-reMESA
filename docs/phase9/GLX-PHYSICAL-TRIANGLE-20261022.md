# GLX-origin triangle on SGX535: physical LAB qualification

Status: GLX_ORIGIN_TRIANGLE_PHYSICAL_LAB_PASS. This is one32x32 depthless draw through the private GLX frontend, not glxgears, a general DRI screen, repeated rendering,1280x800, OpenGL2.1 or complete shader independence.

## Physical execution

A controlled reboot selected the previously qualified quarter initrd SHA256ed7101f6d00a2163f83ad68492047a9e04edc6d3e92d73fe0ab6ecf3f9090be9. Boot UUIDb9d2ffdb-2884-4816-a4d0-6752150b913e loaded the expected gma500 build448696fd63c1662c62adf2778838a488d3334d4b/notea66ca2de.... The old boot's module could not admit this carrier; its checks were not bypassed. Stock saved boot/default, empty one-shot entry and original0600 custom.cfg hashd1d812f7... were restored before submission. The private module remains loaded until a normal subsequent boot.

The isolated root-private runtime pins twelve libraries plus the client. The interpreter header/path was rebound without changing instruction payloads; the authenticator still checked the actual parent executable and its original eleven-artifact schema. A separate CPU-only X11 fixture passed before the physical attempt, with its outputs kept separate. No system Mesa or Xorg installation, network authentication, ADMISSION or protected gate policies were changed.

The client created a GLX context/window using the explicit reMESA backend, linked its own GLSL VS/FS, issued one BACK-buffer glDrawArrays and swapped via the checked readback presentation path. There was exactly one retained ioctl attempt, no retry, clientexit0 and no watchdog expiry. The observer/envelope and actual memory journal independently verify context1/operation1/slot0/generation1/serial1, SGXphase9, ledger7 and GEMstate6. The memory service closed successfully after authenticated retirement. Both temporary test controls returned to N; no fatal kernel signatures were found.

## Pixels and shader provenance

All4096 BGRA bytes match the independent tie-free half-sample oracle:136green and888zero pixels. Readback SHA256d9a4ac05e454e11b73cffa17683bfeb47e6fc56a308c5694e2eb73b0028f05b1. All3072 X11 RGB bytes match the independently computed vertically oriented window plane, SHAd9a4a65681f633a51f8a3875f910b6a5f3417bae3a82b5bcb8d21605a664ca8b. CPU fixture frames were not substituted for either result.

The captured original VS16 and FS8 source ranges belong to the same owned program handle42; their bytes are witnessed in USE+0x40 andUSE+0. The final source write is in the real memory journal. Historical auxiliary PDS/other USE remain explicit LAB dependencies.

Geometry qualification is deliberately finite: the original48byte clip-position VBO was CPU-staged and validated against the exact profile. The retained GPU geometry role contains the corresponding viewport-projected window coordinates. It was not dynamically replaced by arbitrary application VBO contents. This qualifies this exact OpenGL-origin scene, not general GPU vertex transformation or unrestricted vertex uploads. GEOMETRY_SCOPE.json records the distinction.

## Source delta and testing

The executed client/library are immutable and preserved by hashes. A subsequent source change in tools/remesa/glx_lab_triangle.c records actual renderer/vendor/version and drawable dimensions and rejects a foreign renderer or wrong size before drawing. It compiled under native strict warnings and as a new i386 candidate; QEMU usage/missing-carrier refusal passed. That instrumented candidate was NOT physically executed and cannot reuse this execution card. The old physical client did not save raw renderer/version strings; the CPU fixture's strings are not presented as those of the physical context. Hardware provenance comes from the bound frontend, actual submission/observer/memory evidence and full-plane matches.

## Remaining route to original glxgears

The original application remains unexecuted on this profile. It needs automatic bounded backend setup for an unmodified GLX client, an admitted depth visual/attachment and clear, matrix/lighting/color-varying shaders, broader geometry and multiple preserving draws. Persistent reuse and repeated completion/retirement must be qualified before continuous swapping. Larger targets need coordinated color/front/back/Z and scene/table/DPM layout rather than enlarging a32x32 allocation;1280x800 is not qualified. No FPS measurement is claimed from this one-shot setup/authentication experiment.

The next useful scene work is complete ISP/MTE state upload and index ordering with a typed Z owner. Its CPU descriptor encoders and prospective depth oracle remain separate from physical admission. Further GPU work requires a new exact profile, complete backing/relocation proof and a fresh bounded controller; a completed single-attempt card is never reused.
