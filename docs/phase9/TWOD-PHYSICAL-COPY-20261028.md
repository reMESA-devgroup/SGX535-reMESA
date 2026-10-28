# First native 2D SRCCOPY qualification on SGX535 rev121

Three disjoint ARGB8888 copies passed on the physical Dell Mini12, using the
same isolated vendor-stream/ALPHA-ownership candidate:1280×800,640×640 and641×479.
Each case returned submission, completion, visibility and retirement, followed
by full source/destination readback, guard validation and successful context
cleanup. This qualifies those native LAB copy cases. General 2D/Gallium dispatch,
desktop acceleration, desktop OpenGL/GLES2 and an installable driver remain
unfinished. The previously qualified restricted GLXGears path is unchanged.

## Bound candidate and evidence

Artifact root:
`/home/gama/sgx535-offline/twod-vendor-stream-20261028/v2/`.

- `CLOSEOUT.json`: offline prerequisites, not the later deployment state.
- `INITRD_BUILD.json`: private image0956d3d5d474d4c02f90b4ddc561642e4477f2dcd57a610454e6c9d705e9bd16.
- Candidate module SHA256524a19eeb00bb48951a5bb75238caf7b6021c998364c33a155b87b68231107b6.
- `hardware/INVENTORY.json`: fresh stock bootef55dee2 before the isolated boot.
- `hardware/postcheck.stdout`: exact module/observer notes, virgin source state,
  four N controls, first-load identity chain and empty next boot.
- `hardware/restore.stdout`: original GRUB/default restored before GPU work.
- `hardware/PHYSICAL_RESULTS.json`: combined physical report and per-phase times.
- `hardware/artifacts-1280x800/`, `artifacts-640x640/`, `artifacts-641x479/`:
  raw readbacks, client result, before/after kernel logs and source state.
- Each `<size>-FETCHED.json` binds the fetched archive/files. Each
  `<size>-INDEPENDENT_VERIFY.json` binds the separate full readback comparison.

Physical candidate boot: `06f1b0b5-8796-4d55-b9a2-d14a1302dd82`.
Every case identified core/revision `0x01130000/0x00010201`, PCI8086:8108,
and ran the unchanged pinned i386 client
SHA2563a772d80a9c5da7ca477305b8eaf7b5309a8c98904a252bb09f1df0546b91462.
LD_PRELOAD/LD_LIBRARY_PATH were cleared by the controller. No CPU copy or X11
presentation fallback implements the operation.

## Results and guard coverage

| Copy | Pitch B | Source checked B | Destination checked B | Copied B | Destination guards B |
| --- | ---: | ---: | ---: | ---: | ---: |
|1280×800|5128|4,120,576|4,120,576|4,096,000|24,576|
|640×640|2568|1,654,784|1,654,784|1,638,400|16,384|
|641×479|2572|1,245,184|1,245,184|1,228,156|17,028|

All three cases returned stages31, error0, retained0, uncertain0 and
cleanup_error0, with an owned operation/cookie1/1. Each case created and closed
its own context, so those identical numbers are not a reused fence across
contexts or proof of long-running same-context reuse. The cases ran
sequentially in the same candidate boot, without uncertainty or an intervening
reset. All four runtime controls returned to N and producers_active stayed0.
No new native failure, BUG, WARNING or Oops appeared in the kernel-log deltas.

The client initializes different deterministic source/destination patterns
before the ioctl. Its full readback checks every byte only after retirement.
The independent Python verifier reconstructs the expected source, initializes
the destination to its original inverse pattern, then substitutes the requested
rows in private host memory. It verifies all14,041,088 readback bytes, an
unchanged source, exactly6,962,556 changed destination bytes, and untouched
borders/padding/tails. This offline expected-byte calculation is validation;
it does not write the GPU buffers or render the copy.

Archive SHA256 values:

| Size | Evidence archive SHA256 |
| --- | --- |
|1280×800|0b98821927c81d22a554a7975f8b3304a8c7db3ecb57cc805ccfc30e65c565a7|
|640×640|b796fdb94ba9e29b39abc6c1f9b8aa3d26c7f3d9b8295597682807c1942cc5de|
|641×479|3a56409711090eb9e4b94eb4c1211b3333f7d34dc5678bbeb89fe92c2e1ba26a|

## LAB phase wall times

| Copy | Issue ms | Wait ms | Publish ms | Retire ms |
| --- | ---: | ---: | ---: | ---: |
|1280×800|9.916625|27.452932|3.362998|0.145340|
|640×640|4.523550|8.595437|1.409118|0.060901|
|641×479|3.973690|6.049163|1.201549|0.057269|

These are CPU wall intervals around the typed kernel operations, including
checks, cache work, LAB observation and polling. They are not isolated GPU
execution times, sustained copy throughput or GLXGears frame times/FPS. The
client wall times also include initial uploads, complete readbacks, verification
and cleanup; none of that diagnostic cost is promoted into FAST.

## What changed and what remains

The earlier ten-word native profile faulted after its FENCE and remains
unqualified; its original encoder/evidence are preserved. A separate profile
uses the independently captured original vendor eight-word body plus FLUSH,
with36 effective submitted bytes in40 reserved. The unused tail is never sent.
The adapter saves ALPHA, publishes the captured0xff00 under exclusive ownership,
checks it through the operation, and restores/validates the saved request base
and ALPHA before releasing the reservation. Successful retirement traverses
those MMIO readback checks. This replaces the initial-value prerequisite with
owned state transitions, without removing bounds, PFN, permission, coherency,
IRQ-cookie, completion or uncertainty gates.

These results do not explain every implicit FENCE access, qualify overlap or
other formats, expose a safe unprivileged native context, or provide production
Gallium/Xorg/PRIME integration. The endpoint is still an explicit root-only LAB
interface, not the installable general driver. Next, integrate the qualified
copy contract with authenticated userspace resource ownership and a production
lifecycle, keeping LAB hashes/readbacks/observations outside normal rendering.
General3D still requires the complete event PDS, attachment/DPM initialization,
raster control and typed native admission before either API can be advertised.

After all three readbacks and context closes passed, one return-to-stock reboot
was requested. It is a cleanup boot after already-proven retirement, not a reset
used to retire a failed job. `hardware/FINAL_REMOTE_STATE.json` must establish
the returned stock identity, original GRUB/default and empty next boot before
claiming that final state.

The return is now independently verified on stock boot
`c1fad495-4592-4912-bfab-a2cf6748b9ff`: exact stock module note, observer absent,
controls{}, no experimental proc endpoint, original custom.cfg and saved default,
empty next entry. All three copies were already retired before reset. All test,
fetch, SSH and reboot-observation handles for this qualification are terminal.
