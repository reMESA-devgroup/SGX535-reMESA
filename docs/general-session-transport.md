# Bounded general/native session transport

The Linux provider now exposes `remesa_linux_memory_general_open`,
`remesa_linux_memory_general_submit` and `remesa_linux_memory_general_close`.
The staged private kernel registers their separate176-byte ioctl at command15,
`remesa_general_session_ioctl`. This connects userspace to the existing
[retirement-bound general scene session](general-scene-warm-session.md).
It does not replace the old136-byte eleven-ID session or either one-shot ABI.

This first transport is explicitly a bounded LAB admission. It accepts the
already qualified native CODE344 catalogue selectors5/6, representing1280×800
and 640×640. It accepts eighteen owned IDs, not caller addresses, packets, CODE,
PFNs or injected events. Root privilege, the existing default-off memory and
scene controls, exclusive source lineage and the consumed-per-boot boundary
remain required. No new control is enabled by staging or by the C provider.

## Operations and ownership

OPEN supplies context, selector, eighteen distinct IDs and a maximum of1–64
GPU jobs. The kernel checks actual generation, size, placement, pinning,
initialization and CPU exclusion against the unchanged catalogue. It prepares
and authenticates the cold scene, seals the DPM pools, cancels that admission
job, acquires the source lease and releases the preparation guard. OPEN returns
a session handle and identity, with no operation/cookie or GPU receipt. Once
backing is acquired the boot attempt is consumed; cleanup does not rearm it.

SUBMIT supplies the session, next sequence and a bounded budget. The kernel
checks rev121/PDS identity, reserves the producer guard and uses the new general
session preparer. The first GPU job uses cold DPM origin; later jobs can claim
the acknowledged warm epoch and actually initialize used REGION pages.
The ordinary executor retains permission, address, root, cache, completion,
source attribution and retirement checks. Successful loan/guard/job release
precedes acknowledgement and the next submission.

The LAB endpoint authenticates the thirteen fixed READ inputs under job holds
on every submission, because this admission uses immutable catalogue images
without a new general READ-input seal. That diagnostic/catalogue cost is
explicit and is **not** added to production FAST. It does not hash COLOR/Z,
read back pixels or compare output oracles in the submission operation.
Readback belongs to the supervisor between jobs. The existing complete
18-binding LAB trace query supplies the latest plans/events for independent
comparison after each successful submit; it is not a production trace path.

CLOSE supplies the last successful sequence. It validates idle source/backing
ownership and performs the existing coherent release. CLOSE remains available
after execution controls have been disabled. A poisoned session cannot close,
retry or free its retained graph. File/device loss is rejected before hardware
access. A failure after possible preparation/execution retains the same owner
storage, job/root/guard state and source poison; no reset is inferred.

The userspace provider validates all local IDs before its initial INFO queries
and checks backing once at OPEN. SUBMIT/CLOSE perform no extra INFO, allocation,
hash, readback, buffer comparison or memory ioctl. They check the complete
returned prefix, identity, state, stages, phase9/ledger7 and strictly increasing
matching operation/cookie. An errno, positive unexpected ioctl result or
malformed response poisons all local slots, including unused ones. No retry,
unlease or cleanup is guessed. Explicit INFO and host-only forget remain
separate from releasing potentially live hardware resources.

Both session protocols share provider lifetime exclusion but have distinct
protocol tags. Calling an old eleven-ID submit/close against a general session
is refused locally. While a general session is active, the existing typed
page-upload helper cannot modify its READ inputs. Existing kernel topology and
DPM write exclusions remain authoritative even for callers bypassing this C
provider. The Mesa loader overlay copies the new provider include and UAPI.

## Verification

Evidence root: `/home/gama/sgx535-offline/general-session-wire-20261102/v1`.

- `transport-final/RESULT.json`: six actual provider/transport suites pass in
  native, UBSan and static i486/QEMU. The new maintained
  `test_general_session_transport.c` covers532 corrupted-response-byte/errno
  cases per variant, bounded successful sequencing, local admission and
  protocol separation. The five existing transport suites still pass.
- `handler-final-v2/RESULT.json`:84 actual new-controller cases across both
  selectors and all three variants cover successful three-job sessions,
  privilege/gates/profile/backing rejection, sequence/budget maximum and
  identity, preparation, input authentication, execution, receipt, loan,
  guard, job and source-close failures. The complete LAB trace is checked
  after each successful job. Backing/source/MMU/crypto/executor callbacks
  are modeled; prior tests cover the actual general executor independently.
- `legacy-controller/RESULT.json`:44 old CODE344 one-shot cases per variant
  retain their original catalogue, ABI and consumed-attempt behavior.
- `incremental-final/RESULT.json`: complete antiX5.10.240 i486 build passes
  without new warning messages and defines `remesa_general_session_ioctl`.
  Candidate `module/gma500_gfx.ko` SHA256
  `a0db616098026fea162bb5aad2e240d01442aeab5c68eacb4b65cf2a45188f49`.
  That build record is offline. Later hardware qualification of this same
  module is recorded in [the real OpenGL client results](opengl-general-session-client.md).

Preserved fixture failures: `handler/native-build.log` retains two unused
variables from the inherited fixture's unused main. `handler-final/` retains
the first trace check accidentally compiling the older staged handler. The
final fixture includes the worktree handler by absolute path, and the staged
module was synchronized only after the earlier build ended and then rebuilt.
No live hardware test or consumed command was repeated.

Provider reproduction opens no device:

```sh
python3 tools/remesa/test_private_session_transport.py --output NEW_DIRECTORY
```

Pair `--i486-cc` and `--qemu-i386` for the i486 model. Kernel-controller
generators, command journals, source pins and failed/successful logs are kept
in the evidence root.

## Physical integration

The real native OpenGL client now uses this transport in its separate
`--session-2` mode, preserving the one-shot mode. It captures each response,
complete trace, COLOR/defined Z24, inputs and guards separately. Offline client,
archive-refusal and supervisor tests precede hardware execution; see
[the client results](opengl-general-session-client.md) for their evidence paths.

Cold/warm pairs have passed on the physical Mini 12 at 1280×800 and 640×640 in
separate controlled boots, each followed by verified stock recovery.
The earlier warm transition is qualified only by these new results, rather than
inherited from the historical FAST donor's three cold jobs.

This endpoint supplies no sustained FPS, general arbitrary-shader admission,
GL2.1/GLES2 conformance, complete desktop2D or installation approval. The goal
remains the complete userspace driver; this LAB path qualifies the repeated
transaction before exposing a production submit interface.

A subsequent [general READ ownership contract](general-scene-input-ownership.md)
adds immutable input seals at OPEN and metadata checks before execution. It
keeps LAB authentication and qualification separate. Its new module is an
offline-tested candidate; the physical pair above qualifies the earlier module.
