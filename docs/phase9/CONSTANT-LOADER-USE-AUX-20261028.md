# Existing end auxiliary joined to the pixel constant-loader candidate

The original fallback auxiliary has now been identified as `nop.end` by the
SDK's original SGX535/rev121 decoder. The existing reMESA EE final-control
encoder already produces it; no new instruction encoder or opaque template
was added. A checked candidate planner joins that auxiliary, the separate
constant-loader PDS profile and caller-retained source/program windows.
This is preparation, not physical upload/sampling or general desktop approval.

Evidence root:
`/home/gama/sgx535-offline/constant-loader-use-aux-20261028/v1/`.
`REA_AUX_INITIALIZER.json` retains the original initializer observation
`ev_a7f342fe217605bcc4aefc2c2ea691a03c2f58a0d24dad76a2d705cbefa06552`,
Ghidra0x2f8e0/ELF0x1f8e0 in the same pinned original GLES2 library. Its two
fallback allocations request8 bytes and128-byte alignment from distinct
heap fields. Both receive the same single64-bit record. No initializer or
vendor memory allocator was executed by this investigation.

`DECODER.json` runs the original SDK35 decoder with its original535/rev121
descriptor. The selected record prints `nop.end`; removing the final-control
bit prints `nop`. Other selector mutations print different instructions and
remain external CPU probes, never candidate GPU CODE. The decoder's integer
return is recorded without treating it as an execution gate.

Inspection of unchanged `remesa_usse_cpu_control_ee_encode` established that
mode8=0,record_last=1 already derives the exact record from its existing fields.
`loader-qualification/GENERATED_AUX_ORACLE.json` decodes the output generated
by the actual new planner and unchanged encoder: `nop.end`. Eight committed
bytes are distinct from128-byte start alignment and allocation backing.
There is no instruction padding or copied vendor shader program.

## Joined preparation

`remesa_pds_constant_loader_prepare` in `remesa_pds.c/.h` checks:

- Nonzero operation, generation and owner metadata; bounded32-bit retained
  source, auxiliary and PDS allocation windows with consistent mapping identity.
- Exact source window length=count*4, independent backing capacity; auxiliary
  CODE8 at the observed128-byte start alignment; PDS start alignment32.
- Disjoint selected windows and disjoint complete extents for distinct cookies.
- Canonical pixel USE entry planning and lower19 fixup using the existing public
  kernel-based helpers; no new address truncation or register publication.
- PDS effective size60/64 fits the separately retained reserved PDS window.
- Attribute words=max(compiler requirement, transfer words), ceil(words/32),
  and lossless representation in the observed pixel-role field mask01fc8000.
  That seven-bit representation check is not a physical attribute-capacity limit.

It generates the8-byte auxiliary and constant PDS bytes/masks into a host plan,
with execution_error=ENOTSUP. Unknown DATA padding is not promoted to recovered
contents. `remesa_pds_constant_loader_revalidate` regenerates the plan and
compares fresh caller metadata, rejecting corruption and stale mapping/epoch
information. These bounded code/metadata comparisons are not framebuffer
hashes, readbacks or normal-loop diagnostics.

The planner acquires no GEM references, pins, leases or cache visibility. Its
bindings are trusted caller snapshots, not authentication of a provider or real
PFNs. The caller must serialize them, retain both source images and all indirect
texture/constant dependencies, retain the USE bank lease and apply the required
permission/isolation/coherency/fence/retirement contracts before GPU publication.
The previous two-image distinction remains essential: the PDS source is the
later source image, which may refer to the first constant image. This plan is
not a complete shader-resource closure and does not inspect source contents.

## Qualification

`loader-qualification/RESULT.json`:25 metadata, range, exact-size, attribute,
alias, corruption and stale-generation checks pass in native, UBSan and i386.
The actual generated auxiliary independently decodes as `nop.end`.
`MESA_OBJECT.json` binds updated compilation with actual Mesa flags, and the
fresh106-file overlay includes the new API through its existing unit. No full
new Mesa library was linked or installed. `SOURCE_BINDING.json` binds all
relevant unchanged/updated source inputs; all31 protected files are unchanged.
No Mini12, GPU, MMIO, boot or selected runtime module changed.

The physical candidate still needs complete shader/coordinate/result linkage,
actual private-MMU publication/restoration, full indirect resource ownership,
correct stage/task/attribute application and hardware qualification. The normal
sampling/DRI gates are unchanged. Full general GL/GLES2, actual desktop2D,
EGL/GBM/image ownership, multi-client isolation and recoverable installation
remain required for completion.

## Requested share ZIP

The user requested a usable copy of the existing decoder for a friend. The ZIP
was created and tested after extracting the actual archive; source decoder
artifacts and driver inputs were not changed. Copies are:

`/home/gama/Downloads/SGX535-rev121-USSE-disassembler.zip`

`/storage/emulated/0/Download/SGX535-rev121-USSE-disassembler.zip`

Size2,427,677 bytes; SHA256
`8e6644788899cb63a6f7c5db271ef0ab08f307e7e28e3504e0b910b938572ca3`.
It includes the original SDK binary, checked shim and source, Python launcher,
needed i386 runtime/notices, provenance, hashes and a small nop.end example.
Linux Python3 is required; ARM additionally needs qemu-i386. The sample from
an extracted ZIP successfully decoded with the original535/rev121 context.
No GPU driver is required or installed. `share-package/RESULT.json` binds the
archive test and both identical copies. No message/file was sent to anyone.
Driver work resumed immediately after the ZIP was ready, as requested.
