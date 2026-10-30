# Private probe collector and controller tests

This is a separate one-shot LAB tool, not the production rendering loop. It does
not install a module, enable parameters, reset the device, or present an image.
The historical PCI removal/private-MMU lifetime blocker is described in
[the kernel audit](rfc/sgx535-kernel-audit-20261029.md). Subsequent CPU-entry,
retention/rebind, GEM/VMA and asynchronous callback changes permitted a separate
controlled normal-path trial on the Mini 12. Its [first physical result](private-first-physical.md)
proves one fixed-profile private-PD execution/retirement/readback case; pixel
approval, complete removal/S3 safety and a production driver remain unqualified.
The collector/model tests alone do not qualify those contracts.

The safe default only reads assets and writes an offline plan:

```sh
python3 tools/remesa/private_probe.py --assets ASSET_DIRECTORY --output NEW_DIRECTORY
```

Assets are the retained 1280x800 stream profile and its five input images. The
collector verifies the full input image digests, binding sizes/address bounds,
and guard placement. These are LAB costs; they are not added to FAST.

The explicitly selected execution branch, reserved for later qualification,
checks the DRM character device's PCI identity, creates file-owned backing,
uploads inputs under CPU leases, and submits the typed fixed-profile ioctl once.
OPEN sends context zero and adopts the kernel-issued context. Memory replies
must preserve their envelope except for explicitly defined output fields;
malformed responses poison the transport and suppress further explicit cleanup.
It checks the returned SGX core/revision, operation/cookie, retirement phase,
lifecycle ledger, released-state flags, and restored provider ownership/mappings
before allowing explicit cleanup. An uncertain response suppresses UNMAP, UNPIN,
DESTROY, and endpoint CLOSE ioctls. Closing the OS descriptor delegates retention
to the kernel; it does not repair the removal defect.

Readback checks readonly inputs and guard pages and saves COLOR/Z for a separate
pixel oracle. A collected receipt/readback remains `ORACLE_PENDING` and explicitly
sets `hardware_driver_qualified=false`. ioctl wall time is not GPU execution time.
Failed cleanup is an error and cannot return a successful collection result.

Run the CPU-only client wire/cleanup tests with:

```sh
python3 tools/remesa/test_private_probe.py
```

The additional `test_private_probe_memory_contract.py` runner compiles the actual
retained memory ioctl into a host shim and passes Python packets to it. It accepts
`--native` (native module sources), `--shim` (compatible GEM test shim header),
and `--output` (fresh artifact directory). Kernel GEM/MMU/PM dependencies remain
modeled; the ioctl's context and wire validation are actual source. This caught
an OPEN mismatch that the initial Python-only transport fixture missed.

Run the actual C ioctl controller against modeled dependencies with an existing
staged private module source directory:

```sh
python3 tools/remesa/test_private_probe_handler.py \
  --module STAGED_PRIVATE_MODULE_SOURCE --output NEW_DIRECTORY --ubsan
```

The runner extracts owner/backend structures and the retained profile from that
source directory, then compiles the actual controller include. GEM, cryptography,
grammar, borrower, source, MMU execution and receipts are modeled dependencies.
Thirty-six independent processes cover admission, preparation, execution failure,
retained cleanup, and successful control flow. They perform zero GPU operations.
`--cc` selects a cross compiler; `--runner` supplies a userspace emulator and its
arguments before the resulting executable. Compilation is sequential and limited
to 768 MiB virtual address space; an emulated test gets 6 GiB virtual address space
for the guest address reservation (not a physical-memory allocation).

The next driver change must define active detach separately from late software
destruction: block and join hardware entry, quiesce callbacks, and establish a
documented GPU stop/isolation policy before freeing translation/backing resources.
Moving unload wholesale to final DRM release or merely clearing PCI bus mastering
does not establish that policy. The linked controlled trial is deliberately
narrower than a complete lifetime or installability qualification.
