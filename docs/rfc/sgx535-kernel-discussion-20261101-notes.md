# Review notes for the SGX535 kernel discussion email

Prepared 2026-11-01. Draft only; no email sent, push, commit, staging, hardware
operation or production source edit performed by this RFC task. The sendable
plain-text draft is [sgx535-kernel-discussion-20261101.txt](sgx535-kernel-discussion-20261101.txt).
It uses the supplied identity, Gabriel Gama
<operatingsystemsdepression@gmail.com>, as From and ordinary signature. This is
not a patch and contains no invented DCO certification or Signed-off-by trailer.

## Recipients and official guidance

Recommended To: `dri-devel@lists.freedesktop.org`; Cc:
`Patrik Jakobsson <patrik.r.jakobsson@gmail.com>`.
The current upstream [MAINTAINERS file](https://raw.githubusercontent.com/torvalds/linux/master/MAINTAINERS),
checked online on 2026-11-01, lists that maintainer, list and
`https://gitlab.freedesktop.org/drm/misc/kernel.git` for
`drivers/gpu/drm/gma500/`. This is the specific architectural discussion venue;
no broad LKML, stable, Intel i915/xe or PowerVR recipient expansion is proposed.
The SGX535 work here is not the upstream PowerVR driver for newer hardware.

[Submitting patches](https://docs.kernel.org/process/submitting-patches.html#select-the-recipients-for-your-patch)
asks patch authors to identify subsystem recipients from MAINTAINERS and history;
it also advises against unrelated recipients. Its default LKML guidance concerns
patches. Choosing dri-devel plus the gma500 maintainer for this patchless design
question is an editorial recommendation, not a claimed mandatory exception.
Before a later actual patch series, rerun current-tree `scripts/get_maintainer.pl`
on that series and follow its affected subsystem guidance. Cached 5.10 metadata
was not used to establish current recipients. The kernel.org raw MAINTAINERS URL
returned HTTP 403 during research; the successful source is Torvalds' own Linux
repository above, not a third-party maintainer directory.

The [DRM contribution introduction](https://docs.kernel.org/gpu/introduction.html#contribution-process)
provides subsystem process context. The draft asks for the appropriate current
review tree rather than asserting that a 5.10 build is compatible with it.
[DRM userspace requirements](https://docs.kernel.org/gpu/drm-uapi.html#open-source-userspace-requirements)
require real open userspace reviewed for a suitable upstream project before new
UAPI can merge. A restricted test client does not meet that requirement. The
email acknowledges this without asking maintainers to accept a frozen probe ABI.
The [DRM memory-management documentation](https://docs.kernel.org/gpu/drm-mm.html)
and [device-loss/reset/wedging documentation](https://docs.kernel.org/gpu/drm-uapi.html)
are the official background for questions 2 and 3; those questions deliberately
do not assert which helper is correct before the design is reviewed.

## Repository visibility

Local `git remote -v` reports
`https://github.com/gabrieelgama/SGX535-reMESA.git`. Opening that public URL on
2026-11-01 redirects to the public canonical
[reMESA-devgroup/SGX535-reMESA repository](https://github.com/reMESA-devgroup/SGX535-reMESA),
which is the link used in the email. No remote configuration was changed.
Do not attach an unpublished local commit URL or imply that GitHub has the
latest module, source snapshot or physical evidence bundle. No remote commit
ancestry/publication check was made, so the draft explicitly states the limitation.

## Factual audit at the drafting boundary

Source reviewed in the shared worktree:
`/home/gama/sgx535-worktrees/driver-resume-20261018`, branch
`phase9/driver-resume-20261018`, HEAD at initial review
`3115c6e1a4f8bf486455a5204ad4fb475008fce3`
("SGX535-reMESA: resolve actual TA references into retained scene backing").
The main task is concurrently extending validation; the email does not promise
that later local work is published or qualified. No applicable AGENTS.md was
found at the repository, docs, or checked ancestor locations.

| Draft statement | Evidence reviewed and limit |
| --- | --- |
| Dell Mini 12, Z520, exact core/revision, antiX i486 base | Current README; `remesa_mmu_switch.inc` hardware checks; latest handoff. These identify this development target, not all SGX535 variants. |
| Historical restricted glxgears and native 2D | Current README and `docs/phase9/PHYSICAL-GLX-STATUS-20261027.md` reference; latest handoff preserves later physical 2D and private-session history. Draft omits FPS, improvement attribution and broad API claims. |
| Bounded fixed-profile private-MMU jobs physically proven historically | Handoff section 2026-10-30 "C transport + typed PDS updates PHYSICALLY PASSED three jobs", pointing to `/home/gama/sgx535-offline/private-c-pds-20261030/v1/PHYSICAL_COMPARISON.json`. This is canonical eleven-role execution with retained inputs, not the generated seventeen-allocation scene or current module. No fresh hardware query was made. |
| Current general scene is offline and not admitted | `docs/native-job-capacity.md`, `docs/native-scene-ta-references.md`, `docs/native-scene-use.md`, `docs/desktop-api-targets.md`; actual `remesa_gem_scene_ta.inc` reads retained TA, requires matching actions/controls/USE/private loan, and resolves minimum spans. It does not validate the complete PDS/USE/indirect closure or issue general work. |
| Current module built, uninstalled, unqualified | Last handoff section at the drafting boundary records `652022fa30482ed25c7759c933fae37dd6fe4b24623fb2476811a56aaeaa598e`, complete antiX i486 module build. Deliberately not advertised as a deployable artifact or as the historical physically tested module. |
| Last observed stock desktop llvmpipe | `docs/desktop-api-targets.md`: historical read-only inventory, kernel 5.10.240, installed Mesa25.0.7/Xorg21.1.16; glxinfo accelerated=no and Xorg glamor refusal. Not a fresh observation and not a new driver rendering failure. |
| Whole desktop/GLES2/GL2.1/installable incomplete | Current README, desktop targets, latest handoff. Full APIs and performance goals remain separate from specialized physical tests and CPU models. |

## Implementation inspection behind the questions

Actual source reviewed, not inferred solely from earlier RFCs:

- `kernel/remesa/gma500/remesa_gem_private.inc`: resolves retained backing,
  generation/hold/lease state and private rules; constructs the loan and guard.
- `remesa_gem_private_execute.inc`: still requires `SGX535_BO_COUNT` (canonical
  fixed profile). Authenticates before MMU writer interval; bind, fixed service,
  visibility, backend close, root restore and source closure precede acknowledged
  retirement. Errors quarantine; no synthetic recovery or asynchronous fence
  contract is supplied here.
- `remesa_mmu_private.inc` and `remesa_mmu_switch.inc`: hold pages and both roots
  on uncertainty, audit permissions/control pages, explicit x86 table cache
  publication, and refuse release while active/uncertain. These are narrow
  ownership checks; compilation does not certify hardware stop or coherency.
- `remesa_private_gem_lifetime.inc`: unplug-aware entry, orphan quarantine,
  retained-device/rebind predicates. Keeping a software object alive alone is
  not proof that DMA is stopped.
- `remesa_private_power.inc` and `remesa_private_pci_suspend.inc`: checked PM
  admission and PCI operations. Comments explicitly exclude GPU-stop/S3/runtime
  qualification. Runtime-PM enable remains disabled in the retained base.
- `tools/remesa/stage_twod_reservation.py`, especially `private_entry_sources()`
  and `private_power_sources()`: checked source transformation adds unplug joins,
  endpoint loss, guarded entry callbacks, retained teardown branch/rebind refusal
  and the power includes. This staging technique is not proposed as an upstream
  delivery mechanism.

The existing `sgx535-kernel-rfc.md` (2026-10-29) is retained unchanged as history.
Its earlier unguarded teardown finding must not be repeated as though current
source had no unplug/retention integration. Conversely, those newer guards do
not establish that every MMIO/IRQ/PM path is qualified or that reclaim/reset/S3
is safe. The new email asks about the remaining hardware stop and lifetime
contract rather than claiming it solved or claiming a reproduced hardware bug.

Review scope was documentation/source reading and live verification of official
recipient/process sources. No new kernel build, runtime test, conformance test,
GPU execution or measured result was produced by this RFC work. Original
checkpoints and protected evidence were only read and remain untouched.
