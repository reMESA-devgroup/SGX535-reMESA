# Owned scene/region backing at1280×800

IMPLEMENTED_OFFLINE: `remesa_persistent_scene_allocate` now consumes the existing dimension-dependent scene-layout encoder and the existing allocator, pin/MMU mapping, VA reservation and lease implementation. It uses the previously recovered rev121 fresh-context CPU option0 branch. The entire rounded BO, including tail padding, is explicitly initialized to zero and independently read back before provider synchronization. This is fresh storage initialization, not proof of GPU-generated region tables or device visibility.

At1280×800 the original CPU layout requires0x1d320 bytes, rounded to0x1e000 (30pages). The original clear-page count is16 and cookie7 is0x10000. The live snapshot derives `region_gpu_va` from actual allocated base+cookie7 and exposes the original15 written cookie words. It does not manufacture word15 or submit a hardware cookie. The old physical32×32 scene remains unchanged.

The same endpoint fixture now owns color4096000, Z4096000, scene/region122880 and generated scene code/data4096. Their VA ranges do not overlap. Every scene byte is verified zero, the matching region pointer is checked, malformed requests allocate nothing, failed initialization readback rolls back, and direct CPU writes/color reinterpretation are rejected. Deferred resource destruction, registry ownership and uncertainty quarantine reuse the existing implementation. No native job is issued by these APIs.

Validation:88checks in native/UBSan/i386 and actual Mesa archive;5506existing persistent shader checks and72compact scene ownership checks against the rebuilt real archive; static/DRI Mesa build. No huge unchanged encoder suite was rerun. ADMISSION and28gates are unchanged. Zero SSH/MMIO/GPU submissions.

This reduces the native inventory gap but does not close it: auxiliary PDS/USE/background programs, control images, TA page table/parameter backing, full held use-set and dimension-dependent native submission still need the separate qualified profile. The held-job1000-page readback helper and USE routing are available from prior checkpoints. No physical depth or glxgears PASS is claimed.
