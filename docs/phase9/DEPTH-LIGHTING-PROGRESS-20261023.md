# Depth and lighting implementation increment

**DEPTH_CONTROL_STAGED_OFFLINE / LIGHTING_ARITHMETIC_PARTIAL.** This is not
DEPTH_GPU_PASS, GLXGEARS_GPU_PASS or a complete lighting shader.

## Implemented

- The LAB depth scene now stages raster208 + TA56 control DATA through the
  existing persistent owner. A candidate allocation is written, read back,
  checked including zero padding, synchronized and rechecked before replacing
  CONTROL. The previous CONTROL survives ordinary preparation failures.
- The control allocation is typed and immutable through the public write/color
  submission ABI. Integrity loss or uncertain provider effects retain quarantine.
  Referenced resources remain owned by the enclosing LAB scene; this operation
  grants no kernel job holds or admission. External serialization is required.
- The existing USSE arithmetic codec now has a typed-source entry point. It
  admits the observed class2/index0 and class3/index1 ADD/MUL pair, canonicalizing
  either public source order exactly as the original frontend did. Class2 spans
  retain the old profile. Class3 is a numeric CPU class, not a proven DS bank.
- MAX on class2 identity-vec4 sources is now independently encoded. The old
  ADD/MUL API remains unchanged and continues rejecting MAX. No NIR admission,
  capability or execution gate was expanded.

## Independent CPU evidence

Original psb_dri.so SHA256
74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8.
The bounded CPU replays use authored public Mesa program IR and unchanged
original frontend/compiler helpers. They neither execute emitted GPU CODE nor
copy it into the implementation. The reused emulator services and target tuple
limitations remain those documented in mvp-arithmetic-recovery-20261023T031000Z.

Eight valid ADD/MUL programs vary operation, source order and two coefficient
vectors. All return one instruction. In both source orders the normalized tuple
is class2/index0 first and class3/index1 second. Coefficient values change the
coefficient storage, not these instruction bits. This establishes only this
observed profile, not arbitrary uniforms or physical coefficient delivery.

For ADD, W0=(52<<7)|1|(1<<30)|(3<<28); W1 retains the demonstrated ADD fields.
For MUL, W0=(1<<7)|48|(3<<30)|(1<<28); W1 retains the demonstrated MUL fields.
The source bank fields are calculated, not selected from a historical byte array.

Two valid MAX programs reverse the two live input sources. The normalized bases
are (0,4) and (4,0). The original writer selects kind0xd, major3, sub1, arity2:
W0=(L<<7)|R|(2<<30)|(2<<28),
W1=(3<<27)|(1<<9)|(1<<23)|(1<<21)|(3<<12)|1.
The LHS shift is7, unlike the ADD/MUL source0 shift14. This is directly corroborated
by distinct source helpers in the original writer and the two full8-byte outputs.
Unknown classes, modifiers, coefficient MAX, unsupported indices and controls
remain non-encodable. This is **CONFIRMED_CPU**, not architectural validation.

## Focused validation

Per native, native UBSan and i386/QEMU:393 control staging checks,178 coefficient
checks against8 original compiler outputs,54 MAX checks against2 original outputs.
The same tests pass against the rebuilt actual Mesa25.2.0 static driver archive.
Actual-archive regressions:5506 shader ownership,340 LAB scene storage,113 owned
raster/original208-byte oracle,68 old arithmetic checks. Static archive and DRI
module rebuilt successfully. See TEST_RESULTS.json for exact executed receipts.
No whole-Mesa UBSan or physical i386 execution is implied. Existing full encoder
and NIR suites were not repeated because this increment does not change those
lowerers. A linker RWX LOAD-segment warning remains in the build log; it was not
introduced or suppressed by this increment.

## Exact remaining blockers

**Depth physical: UNKNOWN.** The larger scene's background/event/auxiliary images,
DPM/TA initialization, native load/fire consumer and held multipage capture still
need closure under a new qualified profile. The currently admitted exact32
physical route is unchanged. This264-byte control BO is not a ready GPU job.

**Lighting complete: BLOCKED at additional generated instructions and transport.**
The real Mesa fixed-function probe still requires matrix/broadcast/uniform
transport, normal normalization, dot products, clamp, COLOR0 export and FS varying
input. Three new valid original-compiler probes show DP3 emits4 instructions and
RSQ emits3, including selectorB5 and a later9a operation. Neither can safely be
replaced with the arithmetic body alone. MAX emits one. These counts identify
concrete producer work; they are not new GPU instruction semantics. See
GHIDRA_HANDOFF.md for the exactB5 writer and branch to inspect next.

Zero SSH, MMIO or GPU submissions in this increment. ADMISSION and all28 gates
remain hash-identical. Existing physical evidence is preserved, not rerun.
