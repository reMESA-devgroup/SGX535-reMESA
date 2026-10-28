# Typed 2D LAB endpoint and physical-copy client

The [physical follow-up](TWOD-PHYSICAL-FAULT-20261028.md) records a rejected
1280x800 attempt and verified default recovery. The offline evidence below
does not override that result or approve the native fetch contract.

The optional LAB endpoint resolves existing file-bound GEM IDs and dimensions
into the shared typed copy request. It accepts no raw packet, GPU address, PFN,
native callback or injected completion. The DRM descriptor is ROOT_ONLY and
the handler independently requires CAP_SYS_ADMIN, the exact file/context,
`remesa_memory_enabled` and `remesa_twod_lab_enabled`. Both runtime gates start
disabled. This is an isolated laboratory ABI, not general GL/GLES admission.

```sh
python3 tools/remesa/stage_twod_reservation.py \
  --native-source /home/gama/sgx535-offline/gears-depth-less-single-clear-20261026T160400Z/module \
  --output /tmp/remesa-twod-lab --with-lab-ioctl
```

The output directory must not exist. Without `--with-lab-ioctl`, the stager
does not add the compile flag that registers ioctl6. The handler and runtime
gate are also conditionally compiled. Staging does not install or boot anything.

`include/remesa/twod_lab_uapi.h` defines a152-byte request shared by native and
i386 clients. Input/reserved/response fields are strictly checked before work.
Once prepare succeeds, ioctl0 returns a structured outcome even on failure.
Success requires error0, stages31 and retained0; ioctl0 alone proves nothing.
The response also includes operation/cookie, observed core/revision and LAB
wall times for issue, wait, publication and retirement. Wait includes polling
and synchronization; these are not independent GPU timestamp measurements.

The endpoint allocates a heap-owned job/native context before possible issue.
Only an unissued PREPARED job can be cancelled. Any uncertain, faulted or
attached operation retains that heap context and the GEM/module/device/power
ownership. FD death clears the file association through the existing postclose
quarantine; it cannot leave native callbacks pointing to a returning stack.
Successful retirement detaches and frees the context and permits BO reuse.
No pixel readback, full-buffer comparison, hash or checkpoint runs in the
kernel copy path. Phase timing and the ABI itself are LAB-only.

## Preserve legacy source history

The native adapter now notifies `remesa_route_source_producer` around its FIFO
writes, just as the original console producer does. Every write is bracketed
by a source-active notification, and the matching end occurs before reporting
a post-write fault. The existing source and observer locks do not acquire the
fixed IRQ lock on this path, preserving the established lock order.

This corrects an omission in the earlier, uninstalled native candidate. The
old first-owner3D lineage must observe that another engine has run. Its lost
history is neither cleared nor hidden behind bit27's benign classification.
Therefore LAB2D tests must use an isolated boot; do not expect the legacy
first-owner3D gate to approve subsequent work in that same lineage. Production
integration still needs qualified reusable ownership across3D/2D. This change
preserves the legacy gate rather than declaring that integration finished.

## Client and evidence

`tools/remesa/twod_lab_probe.c` defaults to usage; `--describe` opens no device.
Its explicit physical mode is:

```sh
twod-lab-probe-i386 --run /dev/dri/card0 1280 800 /NEW/ARTIFACT/DIRECTORY/copy
```

Use only after selecting and authenticating the LAB module/image and gates.
The client allocates two GEM BOs, uploads distinct deterministic source/guard
patterns, asks the native endpoint to copy the requested width/height, and
requires completed retirement/core/revision before reading either BO. A
one-pixel border, row/page padding and an extra guard page remain outside the
copy. It checks every source and destination byte and saves raw readbacks with
exclusive filenames. It performs no CPU copy to the destination after upload
and has no rendering or presentation fallback. A failed/uncertain ioctl stops
readback and explicit teardown; FD close follows the kernel retention policy.
Readback mismatch after confirmed retirement can clean up safely and remains
a failed test with preserved bytes.

Current evidence directory:
`/home/gama/sgx535-offline/twod-lab-20261027/v2/`.

* `RESULT.json`: actual GEM/preflight/lifecycle/LAB code, native/UBSan/i386,
  each277 parent checks plus seven joined fault/cancellation cases. Covers
  file/context/capability/gates, malformed input, BO reuse, same-BO overlap,
  native failures, failed receipt/visibility/retirement, cancelled unissued
  source and FD-close retention without dangling file association.
* `NATIVE_RESULT.json`: actual native adapter with source notifications,
  each2946 parent checks plus20 MMIO fault models; default-root42 and IRQ838
  parent checks plus62 event-fault children per variant. All are CPU models.
* Client builds pass in all three modes; ABI descriptions agree. `/dev/null`
  negative cases fail and cannot report GPU success.
* `CLIENT_ATOM_PREFLIGHT.json`: the client actually runs on Mini12's Atom in
  `--describe` mode, ABI152/ioctl3231212614, no DRM open or GPU submission.
  Observed controls remainN. This is CPU/runtime compatibility evidence only.
* `FULL_MODULE_BUILD.json` and `MODULE_AUDIT.json`: complete antiX module build,
  ioctl6/root flag derived from the retained DRM header, LAB gate in zeroed
  BSS, and old compiled gears profile byte-exact. The21 full-build warnings are
  in unchanged original files. Focused builds/tests have no unexpected stderr.
  The first ELF auditor assumed the wrong numeric ROOT_ONLY value; its note
  is preserved and the auditor now derives BIT(2) from the kernel header.
* `INITRD_BUILD.json` and `INIT_CHAIN_TEST_RESULTS.json`: offline image retains
  original first-load logic and other members; only module identities and
  their parent bindings change. Five original/tampered identity-chain checks
  pass, using harmless CPU marker payloads, not device/module execution.

The parent evidence directory preserves the earlier endpoint-only build.
The v2 native candidate includes the source-history fix and is the current
candidate. No new image/module was installed or booted, no gate was enabled
and no GPU copy has been executed. Existing Mini12 boot and GLXGears artifacts
remain untouched.

Next: bind a one-shot boot/controller to this exact module, image, client and
fresh boot identity; preserve default/recovery and restore N/N controls. Then
run paired1280x800/640x640 copies and occasional641x479, collect physical
submission/completion/retirement, raw readbacks and guard integrity. Do not
fill the desktop GL/GLES rendering matrix or advertise Gallium2D acceleration
from these offline results or the Atom `--describe` run.
