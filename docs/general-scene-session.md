# General scene executor on the existing private source session

The native OpenGL recipe already passed physically at1280×800 and640×640,
as recorded in [the API client](opengl-native-client.md). This component adds
the missing kernel-internal session adapter; it does not restart shader, PDS,
TA, ISP or MMU implementation.

`remesa_private_session_scene_execute` authenticates the same endpoint, file,
backing owner and live source lease as the existing private session, then calls
the unchanged `remesa_gem_scene_job_execute` on an actual prepared general job.
It locks endpoint then memory internally. Its caller retains the outer DRM
entry and stable arguments and must not already hold either mutex.

The adapter refuses the old retained five-input/six-work seal grammar rather
than treating those eleven roles as general scene inputs. Every residual ID
slot is checked, including incomplete metadata with a zero first slot or a
cleared aggregate flag. It adds no ioctl or user-visible structure and does
not change the eleven-ID transport or one-shot LAB controller.

Successful execution keeps the source lease. The caller separately releases
the loan/reservation and retires the real job before another preparation.
Each job retains the existing translation, permission, coherency, completion,
publication, root restoration and retirement validation. Every entered
executor error poisons the source/session, quarantines its ticket and marks
the job HELD. Arguments remain retained; failed quarantine does not grant
release or retry. File/device loss is rejected before another executor call.

## Remaining scratch initialization contract

Keeping the source lease does not make rendered scratch pristine again.
Publication consumes `zero_pristine` on GPU-written REGION and DPM objects.
The next cold scene preparation requires authenticated zero origin for those
objects. Resetting a flag, omitting a clear or reopening a consumed gate would
discard that contract.

An actual-core replay of each sealed API scene performs normal preparation,
modeled execution and retirement, then prepares the same packet against the
same owned buffers. The second preparation returns `-ENODATA`, unwinds its
unissued job and reaches no second backend stage. It clears no extra pages.
This passes in native, UBSan and i486 variants at both sizes. It is evidence
of the remaining blocker, not approval of a second rendered frame.

The next required increment is an explicit post-retirement scratch/reset or
reload epoch, justified by the existing DPM initialization/ownership contracts.
It must establish what the next GPU load consumes, protect all still-owned
buffers and preserve cache/translation rules. No warm-reuse exemption follows
from these CPU models. Only after that contract and its offline checks may a
separately supervised physical multi-job qualification be attempted.

## Verification

Evidence root:
`/home/gama/sgx535-offline/general-scene-session-20261102/v1`.

- `tests-scene-final/RESULT.json`:721 helper checks per variant, including
  modeled general jobs on one lease, all legacy seal slots, owner/loss denial,
  pre-issue/execution/quarantine failure and no-retry retention.
- Existing backing/source-session tests pass in native, UBSan and i486.
- `dpm-reuse-block/RESULT.json`:six actual-core second-preparation refusals,
  with no additional clears or backend stages.
- `incremental-final/RESULT.json`:actual antiX5.10.240 i486 module builds with
  no new warning messages and contains the new adapter symbol. SHA256
  `82c27a8ce519dde56565ea7cbd43f65199517fa0a01bb90d314eb7fa4df55358`.
  It has not been installed or booted.

The initial full build exposed an include-order error in staging; its log is
preserved. The corrected staged module includes the adapter after both source
helpers and the general executor. A negative residual-seal test exposed the
first-slot-only guard; its failed transcript and pre-correction module/source
are retained. The final incremental build and tests cover the corrected guard.
The full-build helper also retained a stale output-path field; the authoritative
final module path and hash are in `incremental-final/RESULT.json`.

These tests model hardware/source callbacks and supply no new GPU operations,
FPS or physical retirement approval. The previously qualified private session
and finite OpenGL cases remain preserved. No diagnostic hashes, readbacks,
frame dumps, cold clears or other work were added to production FAST.

The focused helper tests can be run without opening a device:

```sh
python3 tools/remesa/test_private_session.py \
  --module STAGED_MODULE_DIRECTORY --output NEW_DIRECTORY --scene-session
```

Pair `--i486-cc` and `--qemu-i386` to add the i486 model variant. Sustained
general rendering, full OpenGL/GLES, normal DRI routing, desktop2D and complete
installation remain unfinished.
