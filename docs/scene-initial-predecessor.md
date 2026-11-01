# Explicit initialization before Z-clear

The optional initialized prefix composes the preserved initialization DATA11,
the existing Z-clear primitive, linked draws and scene close. The old Z-clear
constructor remains available and produces byte-identical legacy images.
The initialization record is copied from the unchanged sparse frozen input and
checked against the qualified physical ROLE0+0x4350 bytes. It is not substituted
for Z-clear and introduces no new interpretation of ISP or USSE instructions.

`remesa_persistent_initialized_prefix_prepare()` stores the additional DATA44
and PDS60 in the same 4 KiB prefix allocation. Effective initialized content is
556 bytes, versus 444 bytes for the legacy prefix. The original first 444 bytes
and every USSE instruction are unchanged. The additional PDS code is the existing
state-upload encoder's required three-instruction task, not capacity padding.
Both uploads use the same existing count-11 USE auxiliary.

The initial primitive's entire 20-byte INDEX descriptor must equal the following
Z-clear descriptor. It therefore shares the exact authenticated index, vertex
PDS and vertex DATA window. Only its distinct state upload adds a reference.
This keeps the existing seventeen GEM bindings, ten USE entries and two vertex
windows for the one-draw fixture. The parser preserves sixteen compiled draws,
with at most 53 references. Legacy streams still have their original grammar.
All syntax is checked before visitor callbacks; mismatched shared descriptors
and repeated state pointers are refused. The native state checker requires the
complete preserved initialization record, including zero fragment references,
and the matching READ backing/PDS/count-specific USE auxiliary. Fragment
validation indexes the subsequent Z-clear and draw uploads explicitly.

Closed LAB selectors 1/2 retain the prior 1280x800 and 640x640 inputs. Selectors
3/4 add the initialization predecessor at those sizes. The generator requires
both catalogues and admission asserts that its four-entry table matches the
UAPI bound. No normal FAST loop, capability advertisement, shader encoder,
canonical eleven-role probe, memory permission or retirement rule is changed.
The new stream still needs physical pixel/depth qualification.

Offline evidence is under
`/home/gama/sgx535-offline/general-scene-initial-predecessor-20261101/v1`.
It includes three producer variants with legacy and initialized exports,
byte-exact legacy comparisons, eighteen initialized producer-to-kernel replays,
twelve deep refusals, parser tests through sixteen draws, and the closed
controller's 46 cases in native/UBSan/i486 builds. Hardware callbacks in these
models do not execute a GPU. `PHYSICAL_INPUTS_INITIALIZED.json` pins only future
LAB inputs; it is not physical rendering approval.
