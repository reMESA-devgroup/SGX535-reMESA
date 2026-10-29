# Typed private-context qualification entry

The private module now has an explicit kernel caller for the same-call private
GEM executor. It is a separate privileged, single-operation probe, defaults
disabled, and is not a replacement for the proven continuous FAST provider or the
original LAB admission path. Its complete handler has not yet been qualified on
hardware or by full lifecycle fault injection. Do not treat compilation or wire
validation as authorization to skip that work.

The normal continuous provider freezes topology through stream_bound and may
activate TA cache domains; those semantics differ from the current cold private
loan. Rather than weakening private preparation or changing that provider, the
new handler uses an otherwise idle file-bound GEM endpoint and the existing
independent source lease. Non-private staged source profiles remain byte-identical.

## Admission and execution

`DRM_IOCTL_REMESA_PRIVATE_PROBE` uses command index 9 and a 112-byte version 1 request.
Only context, eleven GEM IDs and a bounded time budget are inputs. Flags, reserved
and all response fields must be zero. The handler requires CAP_SYS_ADMIN, the
existing memory enable and the new `remesa_private_probe_enabled` parameter. Both
remain false by default. It accepts no GPU address, page frame, instruction stream,
command pair, supplied digest or completion event.

The endpoint device/file/context must match, with no executor, stream binding,
TA cache-domain mode or live job. Core/revision are read from the powered device
and must match SGX535 rev121. Every real GEM address/size must exactly match the
retained kernel profile, with the original MMU ceiling. Job preparation and
snapshot validation establish page/view/hold authority before image admission.

The independent strict profile checks hash all five input images, mask only the
existing reviewed PDS source fields and run the existing geometry validator.
This is single-operation probe/LAB work, never inserted into FAST frames. The
kernel profile asset itself is pinned by the stager. Mutable engine/output
objects are initialized under held jobs; TA clears and full cache maintenance
are cold qualification costs, not claimed performance improvements.

The actual stream borrower constructs the eleven-role owner. The handler reserves
the existing idle producer guard, prepares a private loan with readonly input
roles and read/write engine/output roles, and excludes LOCAL objects from GPU
PTEs. It claims the existing source lease and invokes the actual same-call private
executor. Only its complete retirement/restore/source-close receipt permits
borrower teardown, final source settling/release, loan/guard release and ordinary
GEM job retirement. Data readback remains a separate required physical check.

An explicit consumed flag survives loan clearing: later cleanup failures cannot
be mistaken for an unattempted operation. Failures after execution consumption
retain their heap context and remaining resources. Only known unattempted state
can use preparation cancellation. Source-lease ownership is released while GEM
holds still retain the endpoint lifetime. A module-lifetime consumed flag plus
the source's original no-rearm rule prevents another attempt by toggling runtime
enables. ioctl0 conveys a structured result, not proof of GPU success.

## Evidence and outstanding qualification

`/home/gama/sgx535-offline/private-probe-entry-20261029/v2/REQUEST_RESULT.json`
records 10,546 wire checks in each native, UBSan and i386 run. Tests cover response/
reserved/flag bits, null input, version/context/budget limits, zero/duplicate IDs,
and ABI size/offset assertions. These tests do not execute the handler, crypto
image checks, cleanup sequence or GPU work. Actual psb_drv/GEM kernel objects
compile. v1 preserves the initial GEM compile error caused by using a local name
that did not satisfy the retained PSB_RSGX32 macro; it was corrected without
relaxing compiler checks.

The complete module with the entry is:
`/home/gama/sgx535-offline/private-module-build-20261029/v3/module/gma500_gfx.ko`.
Full Kbuild/modpost/final link completed in 114.133 seconds with one build job. SHA256:
`7c2237860dfa2c3b0b0f40509d518e4babc42e1dd394a0af2d8eb0e85b13e1f4`.
`MODULE_VERIFICATION.json` confirms target ELF/vermagic, private handler presence
and zero BSS defaults including probe enable/consumed flags. Original and probe
kernel profile symbols each match the same 5,844 bytes exactly
(`PROBE_PROFILE_VERIFICATION.json`), and the existing original profile oracle also
passes. The read-only verifier leaves the module unchanged. Five non-private
staging profiles match 735520d (`LEGACY_STAGING.json`). Previous module artifacts
are preserved. No module is installed and no GPU operation occurred.

Next work must exercise the actual new caller's success and error cleanup with
provider/backend/source lifetime checks, then add a userspace qualification client
that uploads the already validated profile and captures real readback/guards.
Only then can a controlled Mini12 test establish identity, submission, completion,
retirement, CPU visibility and buffer integrity while preserving N/N and rollback.
The existing continuous GLXGears path is unchanged. Complete userspace OpenGL/GLES,
desktop 2D, VA-API and an installable qualified driver remain unfinished.
