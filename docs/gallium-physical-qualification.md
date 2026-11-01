# Physical bound Gallium/NIR scene qualification

The bound-context scene path from commit
`39c2552e0307910134e15cee8fbbefb99f2f0623` rendered on the physical Dell Mini 12
SGX535 rev121 at 1280x800 and 640x640. Both tests used the same unwrapped x86
client and kernel module, with separate fresh boots and one GPU submission per
boot. Existing ISP, PDS, USSE, initialization, depth and scene encoders were
reused.

The executed path was:

```
bound Gallium NIR + raw P/N/L vertex buffer + typed COLOR/Z FBO
 -> remesa_context_submit_linked_scene
 -> retained scene builder and provider
 -> actual Linux GEM transport and closed native profile7/8
 -> SGX535 submission, completion, retirement and CPU publication
 -> real COLOR/Z readback, independent oracle and explicit cleanup
```

This is an explicit closed LAB draw through the real frontend and driver
ownership path. It does not qualify ordinary GL draw admission, OpenGL 2.1,
GLES 2.0, sustained rendering, desktop acceleration or an installable driver.
The working historical GLXGEARS path was preserved; it was not benchmarked here.

## Physical results

| Check | 1280x800 | 640x640 |
| --- | --- | --- |
| Closed profile | 7 | 8 |
| COLOR oracle mismatches | 0 | 0 |
| Defined Z24 oracle mismatches | 0 | 0 |
| Immutable GPU READ buffers intact | 12 | 12 |
| CPU-only/source buffers intact | 3 | 3 |
| A5 guards intact | 4 | 4 |
| Objects before capture consumption | 24 | 24 |
| Remaining objects verified and explicitly freed | 7 | 7 |

The kernel receipts identify core `0x01130000`, revision `0x00010201` and PDS
execution base `0x20000000`. Each completed operation/cookie1, phase9,
ledger7 and stages15 without error or retained quarantine. The saved trace
contains the real hardware event samples; the executed raster/TA controls
match the prelaunch snapshot from the existing validators in all 66 words.

COLOR coverage and Z24 were checked independently over the whole target using
the existing analytical oracle. Undefined X8 bits are ignored and the documented
one-unit Z24 quantization tolerance is retained. At 640x640 the 320 exact edge
ties allow either fill only when COLOR and Z agree on coverage.

The trusted client reads temporary owners after authenticated native retirement
and before capture consumption releases them. The supervisor then verifies the
saved data and independently queries the seven remaining live GEM objects.
Reference images are comparisons only; neither Python nor the client renders
the expected framebuffer on CPU or uploads reference pixels to COLOR/Z.

Seven controls returned to N, the source guard closed, GEM cleanup completed,
and the stock GRUB configuration was restored. The final physical verifier
also requires an independently observed stock return before emitting its PASS.

## Timing scope

These are cold LAB operations, with shader/context setup, fresh allocations,
diagnostic reads and validation. They are **not frame times or FPS**.

| Host-measured phase | 1280x800 | 640x640 |
| --- | ---: | ---: |
| Frontend/preparation before LAB callback | 402.93 ms | 331.87 ms |
| LAB initial inventory/read collection | 9.95 ms | 16.15 ms |
| Native ioctl including its whole lifecycle | 184.92 ms | 196.62 ms |
| LAB retired read collection | 157.29 ms | 80.43 ms |

The trace separates CPU publication/flush from host timestamps observing GPU
events. Neither that trace nor the whole ioctl interval is a GPU-only timer.
No continuous-throughput conclusion follows from these samples.

## Evidence and recovery

Evidence root:
`/home/gama/sgx535-offline/gallium-physical-20261101`.

* `v1/PHYSICAL_RESULT.json`: profile7/1280 physical result and stock recovery.
* `v2-640/PHYSICAL_RESULT.json`: profile8/640 physical result and stock recovery.
* Each `hardware/physical-evidence.tar` preserves the actual deployed payload,
  raw receipt, trace, readbacks, inventories, source state and kernel log.
* Each `PRELAUNCH_VERIFIERS.json` fixes the pixel/control oracles and physical
  auditors before its execution; failures from the first offline control
  exporter compilation were preserved and corrected before hardware work.

Consumed GPU boots, **never rearm**:

* 1280: `d7a6a80b-1694-47ab-b76c-10ee230b4836`.
* 640: `28da72c6-9067-4e7c-89f0-7d7af83ede77`.

The module SHA-256 is
`5c50700c7e72b74629c88535405bfcc5da8313eb363d6d75f036c516657256da`;
its note SHA-256 is
`ec51a844aef421d02571f8af7218c6776126fcbc7d0da6b9340e92b884e853c4`.
The recoverable initrd SHA-256 is
`dbadc1e7b50d15d1769daf07917c103780870ecde816157fd51cfb1b0f56ac83`.
The actual client SHA-256 is
`7408b897b4c04715b72f692d8095d61ba6d919a86194f987d7396e0b5bb9a736`.

The next work is production integration: translate ordinary GL state and draws,
reuse validated resources safely across submissions, establish dynamic native
admission and session ownership, and expand qualified shader/API coverage.
The closed profiles, default-disabled gates and truthful capability reporting
remain in place until their replacements have their own evidence.
