# Private GLX LAB frontend — 2026-10-22

Status: GLX_LAB_CPU_CLEAR_WINDOW_PASS. GPU-backed GLX and glxgears remain BLOCKED.

A private, explicit-install Mesa 25.2.0 Xlib GLX frontend now creates an SGX-owned screen/context/window and publishes acknowledged snapshots through the existing XCB publisher. No system libGL, Xorg configuration, DRI admission or kernel configuration was replaced. The generic DRI screen constructor remains restricted. This is a private LAB frontend, not general DRI2/DRI3 support.

## Implemented and demonstrated

- Actual Mesa Xlib frontend, pinned overlay, SGX factory, finite writer waits, checked publication receipts and front/back ownership. No softpipe/llvmpipe screen or indirect-GLX dependency.
- RECT renderbuffer snapshots with unchanged bounds/ownership checks; CPU scissored clear advertises the existing bounded CPU callback.
- Bottom-left SGX-owned drawable storage and X11 publication inversion. An asymmetric red rectangle independently distinguishes orientation errors.
- Current-context/deferred-destruction cleanup, auxiliary-context/screen release and failed-publication poisoning.
- Persistent rejected-draw diagnostics prevent publication of stale framebuffer contents after unsupported draws. Unknown draw states still return ENOTSUP.
- A 512 KiB begin/end stream hint restores Mesa's normal immediate-mode allocation. The previous zero hint caused a NULL stream write at _mesa_Vertex3f+0x82; the corrected path safely rejects unsupported fixed-function draws instead of crashing.
- Restricted GLX extension advertisement; depth/stencil/MSAA/stereo/accum visuals remain unavailable. No conformance/version override was introduced.

## Tests actually executed

The final frontend mock suite passed 68 checks each in native, UBSan, i386/QEMU and i386/UBSan. RECT clear/transfer/snapshot negatives passed native, UBSan, i386 and actual Mesa archive; the current rebuilt archive passed again. Capability checks passed native, UBSan and i386. The actual archive passed 32 NIR builder cases. Static Mesa/DRI rebuilt; the unchanged encoder regression results are retained, not claimed rerun.

On the physical Mini12, the final private binary passed CPU-only GLX transport, negative-draw and deferred-destruction modes (two frames each). The explicit recording backend printed NO SGX and its submit callback always returned ENOTSUP. Green/red and magenta frames matched independent XGetImage RGB oracles, BACK/FRONT readbacks matched, swaps acknowledged and allocations returned to zero after display close. The unsupported immediate draw returned error 95, acknowledgement 0 and submissions 0. SP_NO_RAST did not bypass that rejection. The automated clean build also passed a separate native CPU transport run.

Evidence: attempts/20261022T193330184239Z/{REMOTE_RESULTS,DISTINCT_RESULTS}.json, reproduction-native/REMOTE_RESULTS.json, focused-final/FOCUSED_TEST_RESULTS.json, final-rect/result.json, rect-current-archive/result.json, caps/TEST_RESULTS.json and nir-final/result.json.

Earlier failed attempts are preserved, including visual/stride rejection, orientation mismatch, lifetime leak and the immediate-mode crash; those results are not counted as final passes.

## Hardware boundary and remaining blockers

Zero new GPU submissions, MMIO writes, kicks or kernel changes were performed. The old physical OpenGL triangle readback (136 green/888 zero, SHA-256 d9a4ac05e454e11b73cffa17683bfeb47e6fc56a308c5694e2eb73b0028f05b1) is retained, not a new GLX GPU result.

A new one-submission GLX→NIR→existing native backend candidate compiles and rejects missing protected inputs in offline preflight. It is UNQUALIFIED_NOT_EXECUTED. It uses genuine linked VS/FS, a fresh BACK resource, one draw, independent half-pixel oracle and XGetImage verification. Its client result explicitly requires external completion/retirement evidence. Existing auxiliary historical LAB programs remain declared dependencies.

The old execution card binds another boot/executable: it cannot authenticate the new candidate/library. The next physical step requires matching protected controller inputs/card, exact executable/library hashes, bounded memory-budget audit and the existing qualified lifecycle. No global gate relaxation follows from a successful CPU window.

Original glxgears currently fails its requested DEPTH_SIZE=1 visual. Physical depth, general fixed-function shader/lighting/transform support, broad geometry, repeated GPU frames and the 1280×800 physical BO/scene layout remain blocked. No FPS or accelerated-GLX result is claimed. New depth work is outside this frozen checkpoint.

## Reproduction and scope

source/ is the immutable snapshot of 18 changed/new implementation files; IMPLEMENTATION.patch applies to before/ plus new files with zero fuzz and is hash-verified by PATCH_APPLY_RESULTS.json. Detailed build/run argv and logs are preserved. REPRODUCE.md builds into a new directory. TEST_RESULTS.json separates native CPU work from GPU execution. The 28 protected gates and 3 ADMISSION sources are checked against the preceding checkpoint.
