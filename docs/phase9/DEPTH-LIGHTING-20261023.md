# Depth and lighting: programmable vertex profile

Status: IMPLEMENTED_OFFLINE; physical depth and glxgears lighting remain unqualified.

The CPU depth scene at1280x800 now has an explicit vertex profile. COPY preserves the previous recipe. COMPILED_POSITION4 selects the independently reconstructed programmable-VS task words and output-size descriptor together. It still uses the existing generated16-byte POSITION4 VS and8-byte constant fragment shaders. It does not admit the new48-byte arithmetic VS, perform a CPU transform, or enable GPU execution.

The profile propagates into immutable scene metadata and the persistent owner snapshot. A later native composer can distinguish the selected recipe without guessing from CODE bytes (the two producers can have identical CODE and different DATA). Unknown profiles return ENOTSUP transactionally. The old default remains COPY.

Validation compares the complete generated image: only four pairs of PDS task words, four TA output-count fields and the explicit profile metadata change. All geometry, code, state uploads, fragment programs, references, padding and masks outside the selected fields remain unchanged. The owner test verifies independently allocated/addressed copy and compiled scenes, immutable full-page readback, profile reporting and unchanged original allocation after destruction of the second scene. Existing error, alias, ownership and rollback tests remain.

The underlying CPU rule is preserved in lighting-scheduler-20261023: programmable PDS DATA+0x14=e0000000 and+0x24=0c, versus zero for copy; POSITION4 output size16 bytes is distinct from source stride4DWORDs. These are confirmed original CPU producer relationships. Their complete architectural interpretation is not asserted.

Lighting progress: original typed arithmetic now generates three complete48-byte normalization/scale USE allocations matching independent original compiler/linker outputs, in addition to the earlier ADD/MUL/MAX/component-control encoders. Full fixed-function lighting still needs matrix/uniform delivery, light dot/clamp composition, VS color export and FS varying consumption, followed by physical qualification. It is not enabled for glxgears yet.

Depth execution remains blocked by complete native1280 job construction: authenticated auxiliary/background/event payloads, DPM/TA initialization, XHW load/fire and held multipage framebuffer capture. Owned color/Z/region/control storage and the clear+three-draw+close CPU scene are implemented. CPU image equality is not proof of Z comparison or preservation.

Next bounded physical witness, after native closure and admission: clear Z to1, three overlapping draws at.75/.25/.75 using the existing1280x800 no-edge-tie oracle. LESS+write predicts magenta; ALWAYS or write-disabled predicts final yellow. A single colored readback cannot alone prove Z semantics: packet order, actual ISP state, completion, retirement and authenticated full frame must agree; order/control variants require separate bounded authorizations. The independent sample model expects276000 covered pixels. No new physical execution is performed by these changes.

ADMISSION and28 gate files remain unchanged. No SSH/MMIO/kicks/submissions this increment. No OpenGL version/capability upgrade or software-rendering fallback is introduced.
