# Depth clear shader and ISP contract — CPU implementation

Status: IMPLEMENTED_OFFLINE. Physical depth remains BLOCKED/ENOTSUP. No new SSH, MMIO or GPU submissions.

## New confirmed CPU evidence

The original psb_dri.so SHA25674ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8 parses both retained fragment sources into COLR-only metadata, with UsesKill zero and parser error position -1. The selected clear shader at ctx13b60 is therefore not an explicit fragment-depth writer. Original source bytes and program instructions were not exported or reused.

A bounded native i386 helper under QEMU calls only CPU parser/initialization entries, using denied-at-entry DRM/XML/glapi services. Six cases succeed: two unchanged original inputs plus four self-authored ARB programs (color, depth, color+KIL, depth+KIL). The authored cases independently establish OutputsWritten bit2 and UsesKill+11dc. The syscall trace contains no ioctl or GPU device opens. Early Unicorn initialization budget failures remain preserved and are not counted as successful tests.

The original clear selector ELF51e43 loads ctx13b60 and51e49 stores its pointer into [ctx13b38]+8. Parser stores fb85b/fb864/fb8b5 establish InputsRead/OutputsWritten/UsesKill; original error-position store1079ca establishes ctx12500. Metadata proves a CPU program property, not physical Z export or implicit depth behavior.

## Implementation

remesa_isp_color_control_encode returns only the observed contribution mask0x0e200000. From original producer2c1b7..2c22d:

- explicit fragment depth:0x0a000000;
- otherwise kill or alpha-test: partial routing ?0x04000000:0x08000000;
- otherwise partial routing ?0x02000000:0, plus0x00200000 only with zero channels and ctx13c84 false.

Partial routing is logic-op or blend enabled or RGBA mask!=15. ctx13c84 remains an uninterpreted original predicate. Public Mesa7.4 ABI and original setter51db1 establish color-state offsets; valid producer bytes are255/0. Existing opaque encoding delegates to this central rule.

Corrected the clear geometry API: removed automatic color_control. Geometry does not determine shader output metadata, blend/alpha state or ctx13c84. Depth_control and fan indices remain independently encoded. This is an incremental correction; the preceding sealed checkpoint is preserved.

## Validation actually executed

20 selected routing cases compared against original x86 producer, native/UBSan/i386-QEMU/i386-UBSan-QEMU; all pass. Existing geometry768bytes across6cases and16opaque-mask cases remain coincident in four variants. The new test linked to the actual Meson Mesa archive passes. Static and DRI Mesa25.2.0 rebuild passes. No broad encoder/NIR suite was rerun: prior results remain historical, not new results.

A new independently authored prospective occlusion oracle uses 32x32 window coordinates and exact rational edge checks. Integer row coverage agrees with the geometric edge oracle, with276inside/748outside and no boundary ties. LESS, ALWAYS, NEVER, write-disabled and reversed-order controls produce the expected discriminants. Corrupt/short/long captures reject. CPU-generated expected images cannot authenticate GPU execution.

## Physical blockers

No typed Z owner is bound by the current physical profile; clear/multi-draw, VS/viewport Z transport, actual Z layout, color preservation and depth final-use/retirement remain unqualified. This implementation does not enable depth in Gallium, expose a depth GLX visual or change admission. The next experiment is specified separately; it is not an admitted executable job.
