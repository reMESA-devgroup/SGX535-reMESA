# Real Mesa images through the general kernel scene path

Current integration status (2026-11-01): the bounded admission, capture packet,
general service/backend, ordered executor and existing initialization/depth
finalization are connected and physically verified at1280x800 and640x640, with
full COLOR/Z24 oracle and retirement. See
[depth finalization](general-scene-depth-finalization.md) for current evidence.
Descriptions of missing transport/adapter work below record earlier checkpoints;
do not reimplement those completed components. Normal GL/GLES provider dispatch,
complete API/resource behavior, desktop integration and installation remain open.

The producer/consumer integration test now feeds actual bytes from the existing
Mesa owner constructors into the retained kernel memory service, full scene
closure, ordered executor and explicit retirement. It preserves every GPU
address and program byte. Only file-local resource IDs are translated between
the injected userspace provider and the modeled kernel endpoint.

`scene_capture_export.inc` optionally exports a frozen capture's packet,
placement metadata and complete READ allocations. Writable COLOR/Z/REGION/DPM
storage is not exported or presented as initialized. Normal capture tests retain
their zero-provider-I/O assertions. File exports exist only when the explicit
`REMESA_CAPTURE_EXPORT_DIR` test environment variable is supplied; they are not
a FAST render-loop feature or a public driver interface.

The replay creates real modeled GEM objects at those same addresses, uploads
READ images through the actual memory lease/write/unlease entry, then uses the
actual packet preparer and general executor. All instruction/reference/range,
private translation, publication and job-state gates remain active. The source,
backend and hardware-MMU callbacks are modeled; phase9/ledger7 in this test
are not GPU submission/completion evidence.

The MMU test hook now also accepts an explicit expected rule list for replay.
It compares each sorted binding's actual pages, range, access and cache policy
against that list. Existing positional fixtures keep their original checks.
This changes test primitives, not the native MMU checker or an admission gate.

The optional finite-input fixture passes bounded window-position values and
unit normal/light vectors through the same actual program/geometry owners.
It does no CPU vertex transformation or rendering. Its Z=0.125 is below both
existing clear depths0.25 and1 under the selected LESS contract; the preceding
Z=0.5 finite fixture is retained as an intermediate CPU artifact and is not the
final physical-input selection. Raster output still requires physical testing.
No shader, geometry encoder, PDS or USSE implementation was replaced.

Evidence root: `/home/gama/sgx535-offline/mesa-kernel-scene-replay-20261101/v1`.
`producer-depth` is the final producer source-test build. `kernel-tests` retains
the existing native/UBSan/static i486 scene/executor regressions. `replay-depth`
contains18 successful producer-to-kernel runs: original and finite inputs at
1280x800,640x640,641x479, each through native/UBSan/i486 consumers. Six separate
negative copies change an actual VS opcode or an actual u16 index. Their
preparation succeeds but the deep kernel image/bounds checks refuse with
stage0, before any backend callback. Original exported files stay untouched.

`tools/remesa/test_scene_replay.py` reproduces export and replay from separately
built producer and consumer binaries. It records commands, binary/source hashes
and every binary fixture hash. Both the test and its receipt explicitly state
GPU0. Existing encoders, driver production sources and protected evidence remain
unchanged. No kernel-module rebuild is needed for these test-only changes.

A fresh read-only Mini12 observation confirms the recoverable stock boot,
original module note/GRUB configuration, empty next entry and absent experimental
controls/source/observer. It verifies current recovery state, not fresh core
MMIO identity or hardware execution.

The next live component is a separate privileged bounded admission/transport
entry that consumes this already connected path. It must retain the DRM/file,
backing session, device/power, source lease, producer guard and whole context;
authenticate actual identity/PDS window and complete pinned input construction;
require correct cold initialization; and retain every consumed failure. A fresh
pinned test boot/module and stock rollback are required before launch. The
existing eleven-role LAB controls stay separate. There is no need to repeat
encoder reverse engineering or substitute handwritten fixture bytes.

Full hardware OpenGL 2.1/GLES2 resource/API behavior, whole-desktop integration
and an installable validated driver remain incomplete.
