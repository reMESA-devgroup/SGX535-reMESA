# Native Gallium adapter

The [execution-contract update](../../docs/phase9/PDS-EXECUTION-CONTRACT-20261018.md)
adds original CPU metadata6 packing, typed logical task inputs and VS export
routes used by candidate generation. Gallium validates the integrated contract;
physical DATA/DS, task termination and executable PDS CODE remain unsupported.

The [Phase 9 PDS/shader integration update](../../docs/phase9/PDS-SHADER-INTEGRATION-20261018.md)
adds original typed DATA construction, owned VS/FS host linking and durable
Gallium compiler diagnostics. PDS CODE and full GPU pipeline execution remain
unsupported; no admission or default laboratory setting changes.

The [October 18 implementation update](../../docs/phase9/DRIVER-DEVELOPMENT-20261018.md)
documents the bounded USSE fragment encoder, additional constant NIR forms and
the offline Mesa/Gallium regression results. Full pipeline execution remains
unsupported; the historical first-PC research is inactive.

See the [October 14 development update](../../docs/phase9/DEVELOPMENT-UPDATE.md)
for the current original-fragment integration and its explicit laboratory gate.
The older diagnostic interface described below remains a separate path.

The Mesa 25.2.0 adapter now provides real `pipe_screen`/`pipe_context` callbacks
for buffer/render-target resources, mapped transfers, vertex/index state,
framebuffer state, indexed triangle translation, submission and fences.
The submit seam consumes a clean, immutable projected-position draw packet.
No standalone client is launched and no CPU-rendered image is substituted.

The Linux winsys now issues the new native DRM draw request directly and copies
only the kernel's captured SGX color output into the Gallium render resource.
It does not invoke the historical client. The initial kernel provider supports
one asset-matched triangle, using CPU upload/readback staging in userspace and
owned SGX buffers in the kernel. This hardware path is implemented and compiled,
and the historical diagnostic Gallium draw has a retained hardware receipt.
That result does not qualify execution of the current NIR-generated shader pair.

The recording-backend tests remain separate: they exercise provider transitions
while leaving pixels zero. Neither their success nor compiling the real hardware
application establishes hardware rasterization or OpenGL support.

The initial diagnostic pipeline uses already-projected XY/Z/W positions and an
explicit constant ARGB value. It is a temporary driver-level test interface,
not a shader API. NIR compilation, general framebuffer formats, textures,
depth, clipping, blending and OpenGL capabilities are unavailable. Only a
32×32 `B8G8R8A8_UNORM` render target is accepted. The backend must establish
retirement and CPU publication separately and quarantine uncertain submissions.

[mesa-api-pin.json](mesa-api-pin.json) records official interface identities.
No external SGX implementation is used as a reference. With those headers and
the pinned generated format header available:

```sh
python3 mesa/sgx535/tests/qualify.py \
  --mesa /path/to/mesa-25.2.0 \
  --generated /path/to/generated \
  --output /tmp/remesa-gallium-tests
```

This runs focused native and UBSan tests; an optional compiler/sysroot/QEMU
triplet adds i386 execution. Integration tests enter through Gallium callbacks
and reach the clean resource registry/provider using synthetic completion
receipts. No GPU work occurs.

The remaining first-draw bridge is actual clean SGX allocation/MMU, independently
supported command encoding, device dispatch and visibility-qualified completion.
See [the driver architecture](../../docs/architecture.md).

The actual driver-level test is `tests/gallium_first_draw.c`. It enters through
Gallium resource/state/draw callbacks, performs at most one native submission,
preserves the entire input/output ioctl response even on failure, and preserves
the 4096-byte readback before comparing the exact magenta triangle. Its
`--execute-once` mode requires a separately reviewed execution card and explicit
live authorization. `tests/qualify_linux.py` builds this application without
executing it, and runs interposed ioctl transport tests offline.
# Sprint PDS task output

O perfil CPU pixel restrito e sua integração original com DATA/Gallium estão
descritos em [PDS-TASK-OUTPUT-CONSUMER-20261019.md](../../docs/phase9/PDS-TASK-OUTPUT-CONSUMER-20261019.md).
CODE PDS executável permanece ENOTSUP; a saída CPU não certifica aridade ou
fontes DS físicas.
