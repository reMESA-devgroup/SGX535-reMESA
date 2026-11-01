# Bound Gallium viewport to the original window serializer

The resource-only frontend can retain one finite Gallium viewport at ordinary
render-target sizes. Previously its callback rejected every viewport except
identity or the legacy32x32 profile. This binding change does not enable draws:
resource-only `draw_vbo` still rejects and the executable legacy/gears paths
retain their previous viewport restrictions and capability gates.

`remesa_context_window_viewport_encode()` connects that retained state to the
existing `remesa_ta_window_viewport_encode()` CPU encoder. It derives target
dimensions from the bound retained COLOR resource. It accepts positive X scale,
negative Y scale, integral in-target rectangles and the existing finite dyadic
depth-range contract. It rejects mirrored X, positive Y, fractional rectangles,
out-of-target bounds and depth intervals requiring rounding.
The existing serializer limit is2048 per axis; larger resource storage does
not widen that command contract or any advertised graphics capability.

For this explicit original window branch, the inverse mapping is:

```text
width = 2 * scale.x
height = -2 * scale.y
x = translate.x - scale.x
y = target_height - translate.y + scale.y
near = translate.z - scale.z
far = translate.z + scale.z
```

The pinned Mesa25.2.0 state tracker `st_atom_viewport.c` obtains the public
viewport transform and, for `Y_0_TOP`, negates Y scale and replaces Y translation
with framebuffer height minus translation. The inverse above preserves that
convention; it does not silently apply another Y inversion. The selected API
names the original window serializer explicitly, not inferred framebuffer
identity. Positive-Y/FBO and other original serializer branches remain separate.
The16/24-bit normalization argument is a CPU encoding choice, not evidence of
a bound/initialized Z attachment or physical depth support.

Encoding uses local scalar arithmetic and the unchanged original codec. It
does no allocation, mapping, synchronization, hashing, upload or submission.
Output must not alias context/screen, shader/element/fixed-state handles or
the currently bound COLOR/Z resource objects. Every rejection preserves the
output. Rejected viewport binds keep the state invalid until a valid rebind;
unbinding the framebuffer removes its dimensions from the encoding contract.

Tests exercise1280x800,640x640,641x479 and1600x768 storage, full/subrect windows,
16/24-bit and reversed depth, invalid numeric/orientation/range cases, output
preservation and absence of provider calls. The native2D storage route is
selected in the fixture with a fill callback that asserts if called; no pixels
are rendered by that fixture. Legacy viewport rejection remains covered.
The shared fixture also runs alongside real Mesa Gallium/NIR linkage and
attribute snapshot regressions.

Evidence root:
`/home/gama/sgx535-offline/linked-window-viewport-20261031/v1`.
`actual-mesa-viewport-v4/RESULT.json` records the real Gallium/NIR context
passing with a private two-object Mesa archive and simulated BOs/fences. Its
parent archive is unchanged. `i386-final/RESULT.json` consolidates native,
UBSan and static i486 source tests; the final static link explicitly supplies
libatomic for the existing64-bit resource-cookie operations. Earlier fixture
compile/admission/unsigned-negation failures and the initial missing-libatomic
link failure are retained. No driver gate or codec was weakened to pass them.
These are frontend/CPU integration checks, not physical viewport, Vaio1600x768,
desktop GL/GLES, FPS or installed-driver qualification. Complete generated
event/raster/init/native admission and presentation remain outstanding.
