# Complete emitted PDS images for ordinary draws

The ordinary Mesa frontend can build a matrix/lighting `glDrawArrays` scene.
Its Linux submitter still needs independent input admission. Matching sparse
known fields is insufficient for that boundary: the other emitted bytes must
also match the existing constructors.

The additional image checks cover the current constructor outputs:

| Object | Committed bytes | Existing decoder reused |
| --- | --- | --- |
| ISP/PARAM upload | 60 | `remesa_state_upload_pds` / `remesa_parameter_upload_pds` |
| Copy/compiled vertex primary | 64 | `remesa_vertex_fetch_pds` |
| No-R/COLOR0 fragment primary | 56 / 60 | `remesa_fragment_primary` |
| BGRA8 background primary | 64 | `remesa_background_primary` |
| Fixed event | 148 | `remesa_event_fetch_read` |

The new `*_image` helpers require previously masked DATA cells to match the
producer's zero initialization. They add no architectural meaning to those
cells and do not alter the old sparse decoders. Existing exact secondary,
background-object, event-target and USE-body checks are reused. No encoder,
shader or instruction was replaced, and allocation capacity is never treated
as extra CODE or committed input.

`remesa_gem_scene_job_pds_images_check_locked()` revalidates the held scene
closure once, then checks the actual PDS objects identified by its plans.
The earlier upload-image checker shares its internal checked-plan consumer,
so this traversal does not repeat the full TA/state graph validation.
The job must be prepared and unpublished, with matching retained ticket and
command identity. Existing endpoint/MMU locks, bound private root and immutable
held views are required. Scratch plans can change on failure.

This prerequisite does not bind a service, allocate, map, clear, flush, hash,
read pixels, publish or execute. It is intended for immutable input admission,
not periodic frame diagnostics. No performance gain is claimed. Existing LAB
consumers still use their original checker and complete catalogue authority;
no runtime gate or normal ioctl is enabled by this change.

The OpenGL call benefiting from this work is the existing ordinary
`glDrawArrays(GL_TRIANGLES, ...)` with matrix/lighting inputs. Its exported
1280x800 and 640x640 scenes each contain 11 checked PDS objects. The CPU replay
checks those actual Mesa-produced bytes, refuses 332 reserved-byte mutations
per scene, checks stale tickets/publication/borrowed-view failures, and then
runs the existing modeled executor and retirement. This is zero GPU work,
not evidence of physical API rendering or pixel correctness.

Reproduce a complete-image replay with an existing exported fixture:

```sh
python3 tools/remesa/test_gem_job_capacity.py \
  --native-source /path/to/qualified-single-clear/module \
  --output /tmp/remesa-pds-images \
  --scene-actions \
  --pds-images-fixture /path/to/export/1280x800 \
  --pds-images-fixture /path/to/export/640x640
```

The focused vertex/fragment/background/event contract tests also compare the
checks with the unchanged C producers and retained oracles, including every
masked-byte bit mutation, transactional output and spare-capacity behavior.
Evidence from November 3 is recorded under
`/home/gama/sgx535-offline/native-pds-images-20261103/v1/`.
Native, UBSan and i486 runs are kept separate from physical evidence.

Remaining normal-admission requirements include source ISP payload and
vertex/PARAM value domains, initialization/source ownership and the Linux
submission/lifetime connection. A successful emitted-PDS check is not full
source or execution authority. The complete driver, GL2.1/GLES2, desktop and
installation goals remain open. Rollback leaves this additional prerequisite
unused and retains the existing sparse/LAB contracts.
