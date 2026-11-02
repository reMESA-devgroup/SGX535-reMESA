# Separate gears LAB handlers and positive-witness catalogue

The private staged module now registers submission 13 and trace 14 for the
separate 18-binding wire format. Both are root-only and share the existing
default-off gate, source/backing leases, controller and single consumed attempt
with the original entry. The module is compiled offline, not installed or
physically qualified by this change.

Submission validates the complete version-2 envelope, selects one of two exact
static gears profiles, and normalizes all 18 IDs into the existing controller.
Result fields are translated explicitly: the new wire result begins at byte
96, while the internal legacy-shaped container begins at byte 88. The new
handler never accepts a caller packet, VA or CODE. The original request,
ioctls and all ten old profile records remain byte-identical.

Trace publication records the protocol independently of job count. This is
necessary when a new-format attempt fails before creating a job and has zero
bindings. The old query refuses every new-format trace, including such failed
attempts; the new query refuses old-format records. Owner/file/device/context,
capability, gate and zero-input validation remain mandatory. The complete
4,280-byte record is copied into the supplied query buffer under the existing
locks; no large kernel stack scratch copy is introduced.

## Inputs and offline evidence

The two profiles were generated from actual bound Gallium captures at
1280x800 and 640x640. POSITION/NORMAL and all 36 coefficient words come from
the sealed Oct24 positive physical witness, rather than generic test values.
All 431 files in that witness manifest were checked read-only. Its CODE320
matches the common encoder exactly. The raw fetch source resolves to role2
at offset zero; the new helper preserves the first two float4s and initializes
the unconsumed third float4. No vertices or lighting are evaluated on the CPU.

`tools/remesa/build_gears_scene_probe_profiles.py` generates only the separate
gears catalogue. It checks packet/placement shape, exact target contract,
aligned disjoint backing ranges, READ identities, CODE against the witness,
the complete coefficient/self-address/padding upload, and raw vertex packing.
It refuses changed inputs and output overwrites. Regeneration is exact and
18 corruption cases are rejected. The original profile generator is unchanged.

Evidence is under
`/home/gama/sgx535-offline/gears-kernel-wire-20261101/v1/`.
The current coherent Mesa/NIR build produces both positive-input captures and
passes the previous context regressions. Both captures pass the complete
retained kernel validation/execution/retirement model in native, UBSan and
i486 builds. Controller tests cover 58 legacy cases, 44 internal extended
cases and 44 actual version-2 handler cases per variant. They include failed
empty traces, last-owner validation, protocol refusal, uncertainty and
cleanup. Crypto/source/hardware callbacks in controller tests are modeled;
this is not physical approval.

The full staged gma500 module builds without new warnings. Four old capture
trees, the original UAPI/catalogue and all 31 protected artifacts are unchanged.
No gate, module installation, boot or hardware operation was performed.

The next physical prerequisite is a native client using the new bound gears
preparation/submission and transport, complete trace decoding and an immutable
collector. Its pixel/control/source/guard oracles must be pinned independently
before launch. Reusing positive inputs does not establish pixels under the new
initialization, viewport, allocations or target layout. In particular, the new
cold full-target depth clear differs from the old witness's limited footprint.
Preserve that distinction and compare only the defined Z24 bits.

After client/fault/recovery-image qualification, test fresh recoverable boots
with identity, real submission, completion, retirement, readback, all-buffer
integrity, cleanup and stock recovery. Do not reuse consumed boots or reinterpret
the failed MATRIX_WINDOW experiment as approved. Production GL/GLES admission,
continuous sessions and the complete installable driver remain unfinished.
