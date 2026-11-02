# Homogeneous gears scene closure

The separate state15 owner now feeds a complete persistent scene through
`remesa_persistent_gears_homogeneous_body_prepare` and
`remesa_persistent_gears_homogeneous_scene_prepare`. The common capture,
TA-reference and USE collectors retain its PDS, CODE, geometry, parameter
block, attachments, close state and auxiliary programs.

A homogeneous body accepts only homogeneous state owners. It rejects state14
owners and mixed lists before allocating the body. A state14 update omits
group1000, so using it after state15 would inherit the changed mode. This
constructor never assumes that omission restores window coordinates.

The existing initialized prefix explicitly restores group1000 to `0x10000`
for the clear. Each subsequent homogeneous draw writes group1000=0. The
state15 DATA/PDS/USE counts retain the original 60-byte consumption contract;
TA capacity remains four 16-byte units. The ordered draw still contains state,
PARAM and index references, with no new shader instructions.

The retained kernel checker now recognizes this separate 15-DWORD form. It
checks the complete PDS descriptor, matching source header `0x1f41`, final
zero group1000 word, exact MOVE/tail count, per-draw PARAM reference and
prefix window-mode restoration. Every draw in a body must use the same state
count. Subsequent vertex validation still requires the existing exact gears
CODE320 and complete parameter/geometry closure. State validation alone does
not approve a shader.

The legacy constructors and state14 contracts remain unchanged. No ioctl,
profile, capability or native execution permission was added. The old ten
kernel profiles and the separate two version2 profiles are byte-identical.
Those catalogues do not approve the new state15 images.

## Offline validation

Evidence: `/home/gama/sgx535-offline/gears-homogeneous-scene-20261102/v1`.

- Actual Mesa/NIR i486 constructors produce complete 18-binding, 11-USE scenes
  at 1280×800, 640×640, 641×479 and 1600×768. Body tests cover one and sixteen
  draws, mixed-owner refusal, bank/read closure and transitive lifetime.
- Current retained kernel code accepts the four exported scenes in native,
  UBSan and i486 models. Old state14 scenes and the original kernel models
  continue to pass. MMIO, completion and retirement callbacks are modeled;
  these are not physical results.
- Thirty corruptions of actual exports are rejected before modeled hardware
  stages: source/header, PDS count, MOVE/tail count, prefix mode, CODE, PARAM
  auxiliary/padding and missing PARAM references.
- Complete two-draw controls using all14 or all15 descriptions pass in all
  three variants. Both mixed orders are rejected before modeled execution.
- The full gma500 module compiles with the retained antiX kernel inputs and
  no new warning messages. It was not installed or booted.

No COLOR/Z/DPM CPU initialization, framebuffer comparison, diagnostic hash or
readback was added to the normal driver loop. Physical testing remains
stopped during the thunderstorm. The earlier W2 witness proves the original
mode; this newly assembled complete state15 scene still needs its own physical
qualification after bound frontend inputs and independent oracles are pinned.

Next connect the bound Gallium viewport/ISP/constant-buffer snapshot to this
explicit scene mode and its shared fence/retirement path. Resolve Mesa's
`viewport_transform_lowered` contract consistently with the hardware mode.
Keep generic clipping, interpolation, shader/texture coverage and complete
OpenGL/GLES capabilities unproved until validated.

Userspace native and UBSan constructors also pass. All exported packet,
placement and READ-image files are byte-identical to the actual Mesa/NIR i486
outputs at each of the four sizes (`CAPTURE_EQUALITY.json`). These inputs test
resource/command closure; their synthetic coefficients are not physical pixel
oracles and must not be used as a hardware test catalogue.
