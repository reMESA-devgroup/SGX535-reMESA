# Non-square background geometry recovery

CONFIRMED_CPU. Binary psb_dri.so SHA25674ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8. Original caller ELF2f317 (Ghidra3f317) reads surface+0c into caller stack+0c at2f4aa/2f4ad, and surface+10 into stack+10 at2f4a3/2f4a6, before calling2ee34 at2f4d0. The constructor packs the first of those values into the high shifted field and the second into the low shifted field. This is a CPU field mapping, not a physical x/y interpretation.

For origin0, the three packed words are:

- `(1024u <<20) | (1024u <<4)`
- `((1024u + width) <<20) | (1024u <<4)`
- `(1024u <<20) | ((1024u + height) <<4)`

Each is followed by the exact float32 depth word supplied by the caller. At1280×800 with depth1, the24bytes are DWORDs40004000/3f800000,90004000/3f800000,40007200/3f800000. Swapping the dimension contributions would instead produce72004000 and40009000, contradicting the original CPU path. Square32/64cases cannot expose that mistake.

REA/Ghidra12.1.4 evidence: constructor `ev_74bed5ad5d9152b537a2e2fc5fe38e542cfbb7129df721a5165aac70f402a1de`; caller `ev_b4341da362794e0b0e1af3200d5944b2ba70c48a9b95c0ffdda8281fdd41a84b`. Complete canonical records are retained. Separate bounded Unicorn executions cover the original caller after PIC setup2f32b..2f4d0, including its normalization assertions, then constructor geometry stores2ef9b..2f023. Allocation/relocation helpers are not executed or simulated. These are not whole original compiler/program executions. Seven inputs vary extents and depth independently;32×32 also agrees with the preserved legacy geometry oracle. Original x86 instruction bytes are not used by the new encoder.

IMPLEMENTED_OFFLINE. `remesa_background_geometry_encode` is added to the existing render-target module and built into the actual Mesa archive/DRI. Its inputs are the original width/height field roles and finite depth0..1. It emits only the24-byte geometry contribution at background object+2c, transactionally, with unsigned shifts. It rejects zero/unsupported extents, nonfinite depth and aliased output. It supplies neither the background object header, PDS references nor a USE program.

Tests: seven independent original CPU oracles, non-square field order, depth variation, bounds/NaN/alias/output guards; native/UBSan/i386 plus actual archive. Existing target/layout and full owned color/Z/region/scene archive tests pass. No broad unchanged ISA regression suite was rerun. All28gate and3ADMISSION source hashes remain unchanged. Zero SSH, MMIO or GPU submissions.

UNKNOWN: the architectural meanings of the packed axes, the physical background consumer at1280×800, depth execution and completion of the new whole native scene. Retained auxiliary programs remain explicit LAB dependencies. No depth or glxgears GPU PASS is claimed.

Next: use the generated geometry in the separately qualified native background object, finish the descriptor header/reference/control construction and combine it with owned1280 color/Z/region/scene, compatible USE windows, TA/control/DPM ownership and held multipage capture. Preserve the old exact32profile and its admission.
