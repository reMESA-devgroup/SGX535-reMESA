# Owned scene setup routing at1280x800

IMPLEMENTED_OFFLINE / CONFIRMED_CPU; no new GPU execution.

The eleven-role LAB composer now emits the native setup contributions from its live region owner: seven TA register/value pairs220,208,20c,214,210,248,21c, and a separately labeled raster408 pair. These are values for a future native executor sequence, not a complete MMIO program. No waits, kicks or cookie15 are emitted. The immutable exact32 executor and admission remain unchanged.

The current owned base is0x407d1000. The live layout places the region at base+0x10000, so both21c and408 are0x407e1000. The old32x32-profile offset0x1000 would be wrong. Values208=0x0a028028,20c=0x0701c01c,214=0x460,210=0x3104f,248=0x31f4ff match the preserved original Xpsb scene-info/store-prefix replay for1280x800. Register220 uses cookie8=0;21c uses cookie7. Both offsets are bounded by the owned allocation before serialization.

Evidence is reused rather than rerun: original Xpsb SHA256da531587b1ec59fe433fa20ddd2e691b2f8b7fb35bb82525d4db99259c5e571f, source checkpoint large-target-contract-audit-20261023T041000Z. Store PCs45cc,460f,4618,4621,462a,4633,4642 correspond to the seven new values. The comparison verifies the preserved manifest and rebases only220/21c by the difference between the original fixture base and the actual owned base. Raster408 follows the separately recovered fresh-zero cookie7 rule used by the native plan; this increment does not claim a new raster replay or hardware observation.

Focused tests retain all337 storage/rollback/domain/window cases and add three routing assertions, for340 checks per environment. COMPARE_ORIGINAL.py compares actual outputs with seven independent original x86 stores in native, UBSan, i386/QEMU and the real Mesa archive. Only LAB composition/test sources changed; the static/DRI driver archive remains the already-built and freshness-checked native-scene-storage version. No needless full regression or Mesa rebuild is performed.

Next required composition: actual raster/control pairs, background/PDS auxiliary contents, TA parameter initialization, complete XHW/load/fire inputs, and the held multipage native capture consumer. No GLXGEARS_GPU_PASS or DEPTH_GPU_PASS follows from setup values. All new paths remain preparation-only.
