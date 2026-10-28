# Native resource copies through Gallium

Gallium `pipe_context::resource_copy_region` now reaches the qualified native
SGX535 copy transport through the persistent resource owner. A physical Mini12
run completed 48 operations, 16 each at 1280×800, 640×640 and 641×479. This is
explicit opt-in for a privileged experimental service, not an installed desktop
driver, general OpenGL/GLES admission or complete 2D acceleration.

## Connection and lifetime

After opening a resource-only persistent device backed by the owned Linux
memory session, enable the backend before allocating resources:

```c
remesa_persistent_set_native_copy(&device, remesa_linux_memory_native_copy);
screen = remesa_screen_create(&device);
remesa_screen_enable_native_copy(screen, remesa_persistent_copy_region);
```

Check every return value. Screen opt-in must precede creation of its children;
the caller externally serializes contexts and resources. The adapter does not
change public device/config struct layouts or drawing capability flags.

The frontend validates matching screen/format/target, mip/rectangle bounds,
samples and active transfers, then waits previous writers of both resources.
It reserves a new destination content epoch before dispatch. The owner checks
live membership, registry state, pin/mapping/lease state, ordinary storage role,
actual logical byte span, dimensions/pitch and the qualified 256MiB address
window. Source and destination receive extra references across dispatch.
The existing kernel independently authenticates owned IDs and mappings.

A zero callback result attests synchronous authenticated GPU completion,
publication and retirement. The frontend then publishes the new content epoch.
Known unissued errors cancel the reservation. Uncertainty poisons both frontend
resources and retains both backend references; it cannot be retried or turned
into a CPU copy. Native opt-in currently admits only mip0 2D texture copies
between distinct allocations. Overlap/self-copy, other targets, unsupported
dimensions and unqualified cases fail explicitly. Default CPU copy callbacks
remain explicit CPU storage operations when native opt-in is absent.

There are no diagnostic hashes, readbacks, clocks, logs, CPU maps, new buffer
allocations or extra INFO calls in the native callback. Fences for prior
writers, bounds/permissions, coherency, retirement and fault handling remain.
The service retains its default-off gate, root/CAP_SYS_ADMIN and exact
file/context checks; this does not qualify unprivileged MMU isolation.

## Offline qualification

Artifact root:
`/home/gama/sgx535-offline/gallium-native-copy-20261028/v1/`.

- `owner-gallium-v3/result.json`: native, UBSan and i386 tests of actual owner
  and Gallium callbacks at all three storage sizes, zero CPU fallback/maps,
  owned IDs, active-lease/self-copy refusal, known-unissued cancellation/retry
  and uncertain two-BO quarantine. The injected GPU callback writes no pixels.
- `writers-final/result.json`: native/UBSan/i386 source/destination writer
  waits across contexts, timeout, unsupported requests and terminal failures.
- `private-mesa-v2/RESULT.json`: eight changed objects compiled using actual
  pinned Mesa25.2 Meson arguments in a private archive, owner test passed.
  Original cached objects/archive rechecked unchanged.
- `regressions-v2/RESULT.json`: six tests against that same private archive:
  native writer gates, legacy CPU copy/clear, native protocol transport,
  original memory transport, unissued scene captures and legacy gears geometry.
- `PROBE_BUILD.json`, `PREPHYSICAL_GATE.json`, `staged-overlay/integration.json`:
  external i386 probe binding, physical prerequisites and 102-file source
  overlay including the new internal header. No driver installation occurs.

Initial fixture compile failures and the accidentally replaced transport C
file are preserved in the artifact root. The transport was restored from the
validated parent implementation and only the small typed adapter appended.
The included legacy test main gained an explicit return0 for the actual Mesa
warning policy; no assertions or runtime gates were weakened.

## Physical qualification

Boot: `c5a137ce-e512-4bb2-b211-e7e21659f9cf`.
Client SHA256:
`e978f71b80e2d5f01bf657f4e0813230c3e35a158b2cdc7fe07e94707500333a`.
The unchanged normal-copy module SHA256 is
`9967c6f2e409e4a9e4341dc183dabe854fecf0a6a2685272f9a5be8fdd1cf22c`.
Private image SHA256:
`22eb901478c24351abb0d152103844f8993d8f84af5289340edc59400bbd944f`.

`tools/remesa/gallium_copy_probe.c` creates real resources, uploads source and
destination patterns through Gallium transfers, and calls only
`resource_copy_region` for the tested rectangle. External ioctl observation
does not modify requests/responses. At each size the 16 callbacks produced
exactly16 native ioctls, zero CPU copies and zero memory ioctls, leases or
allocations during the copy sequence. Both buffers and GPU mappings were reused.

Every call passed the normal transport's exact SGX535/rev121 identity,
completion/visibility/retirement response checks. Each session ended at
operation16, cookie1, stages31, error0, retained0 and clean context teardown.
Cookie1 is scoped to the reservation; operation numbers are scoped to the
session. Neither is a global GPU counter.

Final readbacks checked all logical image bytes, including left/right guards
and the extra rows: 14,037,960 bytes across all six images. An independent
offline verifier checked exact patterns, unchanged source and exclusively the
requested destination rectangle changed. Common prefixes match the earlier
physical normal-copy oracles byte for byte. This does not inspect GEM page
padding or approve pixels of every intermediate copy.

| Copy rectangle | Mean callback wall time | Min | Max |
|---|---:|---:|---:|
| 1280×800 | 41.807547 ms | 33.793404 ms | 56.786272 ms |
| 640×640 | 13.127341 ms | 12.869025 ms | 13.347158 ms |
| 641×479 | 10.728538 ms | 9.352865 ms | 12.667882 ms |

Timers are external to the production callbacks. These include owner checks,
submission, cache maintenance, synchronization and retirement; they are not
isolated GPU time or presented FPS. The sample is short and DVFS/load were not
controlled. No speedup claim follows from comparing these values to earlier
low-level ioctl measurements with different buffers/boundaries.

Evidence: `hardware/PHYSICAL_RESULTS.json`, per-size `*-INDEPENDENT_VERIFY.json`,
`*-FETCHED.json` and `artifacts-*/`. All controls returned to N, no producer
remained active and no new failure appeared in the captured kernel-log delta.
Boot default and empty next entry were restored before GPU work. A single
return-to-stock reboot was requested only after all48 operations were already
retired, readbacks fetched/verified and contexts closed; reset is not retirement.
The final stock-state receipt is `hardware/FINAL_REMOTE_STATE.json`: stock
boot `608368aa-3df7-407d-ba3c-25b9c21da890`, exact original module, observer and
experimental controls/proc endpoint absent, original default and empty next
entry. No GPU job or reboot remains pending.

## Remaining work

Native fill/clear and the other 2D operations, integration into general driver
creation, unprivileged admission, presentation and installable packaging remain
unqualified. General3D still needs complete event PDS, attachment/DPM
initialization, raster control and typed native draw admission, followed by
paired physical desktop GL/GLES tests. The previously validated restricted
gears path is preserved. Video acceleration remains a later requirement.
