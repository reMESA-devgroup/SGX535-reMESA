# Reproducing the restricted SGX535 GLXGears path

The latest completed physical qualification ran original GLXGears at 1280×800
for 600 frames / 600 GPU submissions and measured 16.1345 FPS over six steady
windows. See the [physical report](../phase9/PHYSICAL-GLX-STATUS-20261027.md)
for evidence and limits. This is not yet a complete public reproduction kit:
the latest paired direct-FD userspace, qualified kernel/initrd, original-client
bundle, profile assets and machine-bound supervisor are retained outside Git.
The repository's earlier source and checkpoint candidates are not substitutes
for that exact pair.

## 1. Public checkout and offline build checks

This documentation branch contains the current upstream README plus the audited
physical report and reproduction notes; it does not publish the newer driver
checkpoint or private runtime bundle.

```sh
git clone --branch docs/physical-glx-reproduction-20261027 \
  https://github.com/gabrieelgama/SGX535-reMESA.git
cd SGX535-reMESA
python3 mesa/sgx535/tests/qualify.py --help
python3 tools/remesa/prepare_mesa_loader.py --help
```

For the published native Gallium tests, prerequisites are Python 3, a C compiler
supporting the native and UBSan builds, the pinned Mesa 25.2.0 source interfaces,
and its matching generated headers. The archive identity and generated-format
identity are recorded in `mesa/sgx535/mesa-api-pin.json`; the generated directory
must include `util/format/u_format_gen.h` and the compiler/NIR headers needed by
the runner. With `MESA_SRC` and `MESA_GENERATED` set to those existing directories:

```sh
python3 mesa/sgx535/tests/qualify.py \
  --mesa "$MESA_SRC" \
  --generated "$MESA_GENERATED" \
  --output /tmp/remesa-gallium-tests
```

That command compiles and runs CPU/synthetic-provider tests, not GLXGears or GPU
rendering. Optional i386 tests require the runner's compiler/sysroot/QEMU options
together. The published loader helper can stage a separate source overlay:

```sh
python3 tools/remesa/prepare_mesa_loader.py \
  --mesa "$MESA_SRC" \
  --output /tmp/remesa-mesa-overlay
```

The output directory must be new. This helper stages an experimental overlay;
it neither installs a driver nor builds the latest direct-FD GLX runtime.
There is currently no published one-command build/install procedure for the
qualified 16.1345 FPS bundle. A normal system `glxgears` launch may use another
driver and is not a reproduction of this result.

## 2. Qualified physical prerequisites

The tested machine is the Dell Mini 12, Atom Z520, Intel Poulsbo PCI 8086:8108,
SGX535 core ID `0x01130000`, revision `0x00010201` (rev121), running i686
Linux `5.10.240-antix.1-486-smp` and an X11 desktop at 1280×800. The required
kernel includes the qualified read-only COLOR GEM-FD/READ-lease implementation;
a stock kernel with the same release string is insufficient.

The controller verifies SHA-256 of the loaded modules' build-id note bytes:

| Module | Qualified note SHA-256 |
| --- | --- |
| `gma500_gfx` | `d5279b212cf7d7da4477261996f8b15e02dad534aa3a37614dc7b2772b2b83c2` |
| `sgx535_provenance` | `5fada73a6db16bd944cbdafc6e90fae7547eef2be66d26657dddc20b801aaec7` |

It also requires a fresh verified one-shot boot, stock future boot restored,
N/N/N controls and quiet source/ownership state before admission, no pending
operation, successful CPU FD-policy qualification on that same boot, sufficient
memory, and an authorized X11 connection. CPU policy success is not GPU rendering.
The qualified supervisor checks and manages admission, timeouts, child ownership
and cleanup. Do not substitute the old default launcher or a different kernel.

The exact private 23-file bundle contains the original i386 client, its private
dynamic loader and libraries, matching libGL/bootstrap, and the unchanged S
profile plus five role assets. Its manifest SHA-256 is
`5d37eef365f813f6e74cde98bc771a392a59a973f2e49da53f11b161deb16764`.
Client/libGL/bootstrap identities are in the physical report. The supervisor
verifies all files and rejects mixed binaries or dependencies resolved outside
the bundle. X11 MIT-SHM read-only FD attachment/completion support is required.

## 3. Actual recorded GPU invocation

The following records the inner client invocation from the qualified supervisor.
`B` means its already verified private bundle and `OUT` its fresh capture
directory; these are not files supplied by this public checkout. `DISPLAY` and
`XAUTHORITY` belong to the authorized physical desktop. This excerpt is not a
standalone launcher: the supervisor's boot, policy, ownership, admission and
cleanup steps are required around it.

```sh
env -i PATH=/usr/bin:/bin LANG=C \
  DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" \
  MESA_SHADER_CACHE_DISABLE=true \
  REMESA_GEARS_HARDWARE_LAB=1 \
  REMESA_GEARS_FRAME_LIMIT=600 \
  REMESA_GEARS_PROFILE=FAST \
  REMESA_GEARS_READBACK=MMAP \
  REMESA_GEARS_PRESENT=SHM \
  REMESA_GEARS_GEM_FD=1 \
  REMESA_GEARS_ASSETS="$B/assets" \
  REMESA_GEARS_AUDIT="$OUT" \
  "$B/lib/ld-linux.so.2" \
    --library-path "$B/variant:$B/lib" \
    --preload "$B/lib/gears-bootstrap.so" \
    "$B/glxgears-original" -info -geometry 1280x800 -fullscreen
```

Despite the historical environment-variable name `HARDWARE_LAB`, this selects
restricted FAST and confers no LAB approval. The normal direct-FD path performs
no per-frame COLOR snapshot copy, hashes, pixel comparisons or dumps. X server
presentation still copies pixels. Kernel and matching X completion must precede
reuse, with detach before FD/GEM close.

The separate continuous-session controller uses the same pair and command with
`REMESA_GEARS_FRAME_LIMIT=0`, an external FPS-progress watchdog and explicit owned
child cleanup. Its offline watchdog tests passed, but that is not evidence of a
completed unbounded physical run. The bounded 600-frame result remains the
published qualification; no continuous-session running state is claimed here.

For a bounded reproduction, verify the terminal report contains 600 frames and
600 GPU submissions, zero queued/error/legacy/offline counts, exit 0 and
`presentation_error=0`; capture must remain empty and the final ownership/control
audit must pass. Original stdout FPS windows establish performance. Do not infer
FPS or per-frame pixel correctness from submission counts or GL version strings.

## Publication gaps and audit sources

Before independent users can reproduce the hardware result from a checkout,
the project still needs to publish the exact source/build recipe for the paired
direct-FD runtime and matching kernel, redistributable profile/bundle inputs,
and a portable qualified launcher with documented boot/policy setup. Private
X11 authorization, SSH credentials and host-specific control files are not part
of that package. No local maintainer path is a download/install instruction.

This guide was checked against retained local
`gears-fast-gem-fd-original-glx-20261027T013000Z/hardware-glx/SUPERVISOR.template.py`,
its stage/physical/performance records, and
`gears-fast-gem-fd-continuous-20261027T020000Z/hardware-glx/SUPERVISOR.template.py`
plus `CONTINUOUS_OFFLINE_TEST_RESULTS.json`, under the evidence root identified
in the physical report. Those local scripts are not shipped as public launchers.
