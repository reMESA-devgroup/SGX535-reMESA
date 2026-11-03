# Updating sealed scene data after retirement

The captured frontend/general-session cold/warm path is physically qualified
with frozen inputs at 1280×800 and 640×640. Changing a matrix between its jobs
is still refused in three places: capture holds, Linux `private_active` upload
exclusion, and kernel READ seals. Reusing an idle pipe fence does not authorize
writing through those exclusions.

`remesa_private_session_patch_scene_input` supplies the kernel memory
transaction needed for a future typed update. It is an internal helper with
no ioctl or current general-session caller. Ordinary memory WRITE, write
LEASE and SYNC remain denied; every READ seal stays active throughout.

The caller must retain the outer DRM/source entry and supply a stable policy
derived from initial authenticated program/descriptor admission. Its pure
validator must identify the exact input and data window and validate all
replacement bits. This is not permission to patch arbitrary READ inputs or
CODE. User-controlled policy pointers, functions or raw userspace data are
not accepted by this kernel contract.

The helper copies at most 512 bytes into local staging, then locks endpoint
and memory in the existing order. It requires the same source/backing owner,
an acknowledged DPM retirement operation, valid pool identities, the complete
sealed READ set, and no live jobs, CPU leases, holds or user mappings. Stale epochs,
device/file loss, quarantine and policy rejection refuse the transaction.
The write is DWORD aligned, within the allocation and contained in one page.

Only after validation does it reuse the backing's retained CPU mapping or
temporarily map the affected page. It writes the copied bytes, flushes that
one page and publishes a DMA write barrier. It does not allocate GEM objects,
clear pools, read back frames, compare entire inputs or hash buffers. A refused
policy or failed temporary mapping changes no content. The current READ loan
permissions already forbid GPU writes to the input, so this operation needs
no GPU-to-CPU transfer of device-written data.

## Parameter windows from the captured graph

`remesa_persistent_scene_parameters_snapshot` now resolves each emitted TA
parameter reference to its actual held PARAM owner. It reports the source,
coefficient, transfer, USE, PDS and TA extents through
`remesa_parameter_window`. It also works after the public parameter reference
has been deleted. It performs no provider read, allocation or cache operation
and grants no write lease. The currently admitted TA graph retains its existing
36-word coefficient restriction.

The shared window validator separates coefficient bytes from the self-address
prefix, transfer padding, reserved gaps and executable/command ranges. A patch
may replace a DWORD subset of the coefficient range, including one word.
This structural validation is not a PARAM/DMA encoder or program validator;
initial admission must still establish the actual object kind and image.

`remesa_private_session_patch_scene_parameters` copies a trusted policy and
checks its context, ID, GPU address and allocation size against the live sealed
backing while the transaction holds both locks. The generation in a host
window identifies the frontend owner; native generation checking continues
to use the sealed kernel ID. Its optional bounded-float rule matches the old
restricted parameter profiles. That numeric bound is not a proposed universal
OpenGL uniform restriction.

There is still no ioctl or general-session caller for this update adapter.
In particular, accepting window metadata structurally does not make an
untrusted userspace description proof that a CODE object is a PARAM owner.

The general-session candidate now retains its first draw's native policy after
the first acknowledged execution, before private loan/job release. Binding
uses the existing decoded TA reference and `states.uploads`, the same native
ticket and source owner, real READ seals, IDs/generations, holds and backing
address/size. It refuses early/stale/lost or inconsistent plans without changing
the output. Later jobs retain the policy without rebinding it per frame.
The embedded TA8 span describes the owned CPU producer layout, not another
measured GPU fetch. Native-bound window generation is the kernel BO generation;
it must not be compared with a frontend resource registry generation.

This binds a policy; it does not add an update ioctl or permit changed bytes
through the existing LAB full-image check. Binding failure after retirement
poisons and retains the session, with no release/retry that guesses safety.

## Verification and integration still required

Evidence is under
`/home/gama/sgx535-offline/scene-input-epochs-20261103/v1`.
The actual memory-service/seal/transaction code is tested with injected
GEM/MMU/cache APIs in native, UBSan and static i486/QEMU variants. Prior hardware
retirement and typed policy admission are explicit model assumptions. These
tests are not new physical GPU or shader qualification.

The test policy authorizes one 36-word data window using the existing retained
finite/bounded float rule. Tests cover stale epochs, incorrect owner/ID,
forbidden prefix and partial writes, bounds/alignment, nonfinite values,
jobs/aliases/leases/quarantine, mapping failure, unchanged bytes outside the
window, retained mappings and exactly one affected page's cache publication.
Existing READ-seal, DPM-seal and eleven-role input tests remain separate.

The next layer is tested under
`/home/gama/sgx535-offline/parameter-data-policy-20261103/v1`.
Shared bounds/bit-pattern tests cover 1–127 coefficient words in native,
UBSan and i486. The actual typed kernel adapter also tests mismatched live
identity and single-word patches. A coherent build of current driver objects
with the qualified generic Mesa libraries exercises captured windows at
1280×800, 640×640, 641×479 and 1600×768, plus the existing captured frontend,
COLOR transfer, context and legacy tests. All are CPU models, not new GPU
qualification.

Native policy binding is tested under
`/home/gama/sgx535-offline/native-parameter-binding-20261103/v1`.
The memory/seal binder runs with injected retired plans in native, UBSan and
i486. Separate controller tests inject the binder dependency and check all
four existing profiles, first-job binding, reuse across three jobs, and
post-retirement binding failure without release/retry. These models do not
qualify the new kernel candidate on hardware.

Before making updates callable by the frontend, Linux transport and frontend
capture state must participate in the same acknowledged update, including
uncertainty handling. Kernel-derived binding must remain the authority rather
than an imported userspace description. The general LAB catalogue still authenticates its complete original
inputs and must not be bypassed to accept changed bytes. A changing-input
physical experiment needs its own presealed expectations before execution.
The existing PDS/USSE/TA encoders and qualified program bytes need no rewrite.
