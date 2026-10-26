# FAST X11 publication pipeline candidate — physical gain unproven

Only `remesa_xcb.o` is replaced in the physically measured Y libGL composition. The qualified bulk-clear archive, GLX frontend object, R bootstrap, kernel, geometry/shaders, GPU submission, readback, fences and retirement are unchanged. Target remains original GLXGears1280×800. No hashes/checkpoints/pixel checks or diagnostic waits are added to FAST.

Measured motivation: Z600 available-phase audit reported actual X11 wrapper30.320ms/frame, including publisher21.571ms. Original profiler failed an unused legacy probe; that failure is not reclassified. Latest unprofiled baselineY8.826FPS (~113.3ms) was one steady5s window, not target success.

Normal FAST publisher changes synchronous barrier opportunities from5 to3:

| Before | After |
| --- | --- |
| Geometry reply | Geometry+Attributes both queued, then both replies drained |
| Attributes reply | included above |
| Checked CreateGC | unchanged, before any image |
| Last image check thenall earlier checks | enqueue images thenchecked FreeGC; check FreeGC first, then consume every image cookie inwireorder |
| Checked FreeGC | included above |

This counts synchronous call phases, not measured kernel/user context switches or physical time saved. No FPS projection is justified yet. On errors libxcb may need extra synchronization; correctness remains required.

LAB nonFAST query/send/check behavior, SHM helper, over64-cookie image fallback, window dimensions/format/visual/XID ownership validation all remain. FAST queries still validate eachframe: no geometry cache, event ownership change, persistentGC, unchecked request, discarded checked error or resend. Allqueued replies and image errors are consumed even on failures. FreeGC only releases the already-created serverGC after ordered PutImages. CPU framebuffer resource remains held through all checks.

Tests native/UBSan/i386-qemu run actual publisher plus mockedXCB/Gallium. Includes120×16chunks paddedpixels, one stagingallocation, eachimage errorposition, simultaneousimage/freeerrorpriority, bothqueryerrors drained, partialzero cookies, zerofreecookie noretry, fatalconnection, wrapinsideimages/exactlyFreeGC, cap64/fallback65 and LABserial. These are CPU mocks, not GPU or Xserver proof. No Xvfb/Xephyr was installed/started.

Next physical comparison (root owns hardware): after geometry correctness is physically qualified, replace ONLY thislibGL in a new private manifest, keep qualifiedRbootstrap/newqualifiedkernel/assets, use FAST+PUTIMAGE, original client1280×800. Compare ≥600frames baseline/candidate, no framecaptures/hashes, close0, exactGPU/submission/retirementcounts, N/N/N and stockrollback. Externalphaseprofiling must use offsets from thisactualELF, particularly publish_checked_batch/remesa_xcb_present_oriented_staged. If FPS/phase time doesnot improve measurably, retain baselineY; no automaticdeployment was performed.
