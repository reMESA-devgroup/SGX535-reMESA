# Selected constant/texture upload-image construction

The shared userspace code now constructs a bounded immutable-byte image from
literal32 uniforms and checked texture-state triplets. This bridges descriptor
construction toward the shader-constant upload contract. It does not submit a
job, implement a texture instruction, establish a hardware SA assignment or
admit general Gallium rendering. The full userspace2D/3D and installable-driver
objective remains incomplete.

## Evidence changes the integration route

Following the final texture-state producer's sole direct caller led to the
original GLES2 validation path. A selected consumer at Ghidra0x41230 (original
ELF/file0x31230) reads the resulting triples into a constant upload image. It
allocates `literal_count + 3*texture_record_count` DWORDs, gathers compiler-
indexed literals and masked/ORed texture values, advances a CPU ring by the
effective byte count, and records a device address plus DWORD count.

That is a distinct route from the existing primary PDS R/T texture grammar.
Those preserved encoders are not replaced or reinterpreted. They must not be
blindly connected to this secondary-constant image as though the two delivery
paths had identical coordinate/results or task controls.

Evidence root:
`/home/gama/sgx535-offline/texture-sa-upload-contract-20261028/v1/`.
The artifact remains the original libGLESv2 SHA256
`7859b0fa0cd3cd44ed18a0c056607066c07312307f8c6d86fd8e211f9a617ed0`.
`REA_SA_CONSUMER.json` retains the analysis, with a reusable
`GLES2_ANALYSIS_SNAPSHOT.json`. Key records:

- Validation caller: `ev_5b38667e9da3328383d569d975046d22792cd60c08204f659e830deb827d67e1`.
- Image consumer: `ev_7a995f3876659f5b449047943ee01e3848f58203ee8fae9da198377d1762ba80`.
- Ring reservation: `ev_dc89cbf5c6fa41270132ec7f41c88dcb0a90091d386bac4d7351e5f5911d27cd`.
- Ring advance: `ev_8b3fd3725a9b6c9c377b20a54a3f901e9dd74c6d4833da826e09a8856e17617f`.
- Device-address calculation: `ev_f5d9c81521bc18f83b154ee685747700abd8414c5f7638cef66012f300dabcb2`.

The old index database was inspected as a locator only; its missing extents and
lack of instruction rows did not provide the required route. A direct host-
instruction lookup and focused REA analysis supplied the consumer observation.
No new Xpsb/psb_dri code, executable shader template or excluded PSP2 source was
used. No vendor code or compiler patch table is imported into reMESA.

## Controlled original replay

`replay_sa.c` maps the original RX segment and invokes only the selected image
consumer plus its CPU reservation/advance/address helpers. An owned synthetic
empty ring guarantees the checked free-space path; no wait branch, original
constructor, GL initialization, allocation ioctl or GPU job executes. Exact
ELF/size/hash checks,5-second guest alarm and15-second parent timeout apply.
`BUILD.json`, `REPLAY.json`, raw stdout/strace and `VERIFY.json` bind the result.

The160 cases use the40 previously replayed strided sampler triples, with one
or two texture records and identity or selected nonidentity masks. Two literal
uniforms are scattered around the triples. All32 source slot indices are
covered. Destination images, ring advance, recorded address/count and untouched
context/program/ring/image memory are checked. There are zero ioctls or device
opens. The synthetic recorded GPU address is not an authenticated mapping.

The effective CPU images contain5 or8 DWORDs,20 or32 bytes. The ring helper
aligns the selected reservation start to32 bytes. That alignment and CPU
advance do not prove a hardware DMA read count, base SA register or shader task
extent. The selected role argument0 is not promoted to a hardware data-master
assignment. Actual upload and sampling remain unqualified.

## Checked shared implementation

`remesa_texture_cpu_sa_image_encode` in `remesa_texture_state.c/.h` accepts
literal32 values with destinations and texture destinations with the existing
checked stream sampler inputs. It reuses that encoder to construct the full
three-word state, preserving addresses and layout fields. Unlike the original
consumer, it does not accept arbitrary patch masks/ORs or float conversions.
Those could invalidate a texture's validated memory bounds and need their own
contracts before any GPU admission.

All destinations must be in range, disjoint and cover exactly
`uniform_count + 3*texture_count` words. The constructor rejects null, aliased
or wrapping host spans, duplicate/overlapping destinations, unsupported sampler
state, row/address/span violations, insufficient capacity and images exceeding
4096 bytes. That limit matches existing host shader storage policy, not an SA
hardware limit. An empty image returns ENODATA. Every failure preserves the
output and written count. Success writes only effective DATA bytes, with no
holes, capacity fill or CODE. There is no heap allocation, GPU call, hash or
normal-loop diagnostic. Owned mapping, visibility, lifetime, upload/fence and
retirement validation remain backend obligations.

`qualification/RESULT.json` records, for each native/UBSan/i386 variant:
37 image-boundary/transaction checks,54 existing state-constructor regressions
and80 identity images matching the original consumer. The80 masked cases are
retained oracle observations, not implemented mask functionality. The updated
unit compiled under actual Mesa flags (`MESA_OBJECT.json`) and a fresh106-file
isolated overlay. No full new Mesa library was linked or installed. All31
protected files remain unchanged. Prior semantic-descriptor tests retain their
exact bound source hashes (`DESCRIPTOR_SOURCE_RECHECK.json`); admission remains
ENOTSUP.

To run the pure image test from the repository root:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Imesa/sgx535 \
  mesa/sgx535/remesa_texture_state.c \
  mesa/sgx535/tests/test_texture_sa_image.c -o /tmp/remesa-texture-sa-test
/tmp/remesa-texture-sa-test
```

## Next physical contract

Trace the recorded address/count into the existing constant-load PDS/USE path,
including destination SA/base, physical transfer extent, stage/task lifetime
and retained ownership. Derive the NIR shader layout and texture coordinate/
result binding before claiming a textured shader pipeline. A physical candidate
must keep address/range/permission/isolation/coherency/fence/retirement gates,
then verify identity, submission, completion, retirement, readback and guards
at the required sizes. No CPU image/replay is accelerated rendering.

General GL/GLES2, actual desktop/EGL/GBM/image ownership, multi-client isolation
and reversible installation remain required. No Mini12 operation occurred in
this increment, and the new image builder is not called from normal drawing.
