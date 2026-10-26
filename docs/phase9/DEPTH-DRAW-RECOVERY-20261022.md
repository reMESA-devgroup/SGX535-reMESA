# Depth clear and drawing recovery — incremental CPU implementation

Status: DEPTH_CLEAR_ENCODER_OFFLINE_PASS; physical depth and glxgears BLOCKED.
Parent frontend checkpoint: glx-lab-frontend-20261022T190000Z (660 artifacts).
No new SSH, GPU submission, MMIO, kernel or gate changes in this depth round.

Implemented in existing remesa_render_target.c/h, used by its real Mesa archive:

* An original bounded clear vertex-data encoder: four ordered 32-byte vertices, normalized float Z, four color components and explicit IEEE binary32 little-endian serialization. Original fan expansion generates two triangles with u16 indices; no shader or PDS program is embedded.
* The existing depth comparator produces ALWAYS/write-enabled `0x01c00000` for this clear. A new restricted opaque color-mask codec emits the independently recovered contribution: full RGBA→0, nonzero partial→0x02000000, zero→0x02200000. It does not emit the other ISP fields or implement arbitrary masked exports.
* Finalization of an encoded depth surface contributes OR0x004000d0 iff both original CPU predicates are true. Their architectural meaning remains UNKNOWN. No live surface status is changed on preparation or failed encoding.
* Invalid bounds, nonfinite data, invalid mask, inconsistent split relocations, noncanonical controls, undersized/partial surface records and unsupported dimensions fail transactionally. No Gallium depth admission has been loosened.

## New independent evidence

CONFIRMED_CPU: original psb_dri.so SHA256 `74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8`. ELF addresses below; existing Ghidra adds0x10000.

Hardware renderer ctor26678 is installed at3736c inctx13b84; software renderer53166 is distinct. Selector2d124 chooses currentctx13b88. Clear529cb obtains mapped vertices from26c9f, writes z at each vertex+8, chooses6 through26aa1 and dispatches26a0f before267e2 commit. Six original ctor/builder CPU replays match768 bytes of independent C output. Callback services are intercepted at their proven addresses: this is not a complete original-driver or GPU execution.

Original class6 maps to0x10000000 and27fa0 selects27eb2, which expands a fan to triangles. Six original helper cases confirm ordering and base-index arithmetic; quad→0,1,2,0,2,3. REFUTED: literal6 is directly a hardware primitive code.

526d3 aliases clear depth state;51c9d sets enable/write1 and compare207. Original ISP slice2c0f3 confirms01c00000 for visual depth16/24,01d00000 for0. Three replay cases.

526b9 aliases clear color state;51db1 copies actual GL masks or zeros all four bytes. 2c03b packs R/G/B/A into8/4/2/1; 2c087 sets partial-mask predicate. Under the explicitly documented opaque branch (state69/38/2c, shader20 bit2/11dc andcontext13c84 zero), 2c20a contributes02000000 for partial masks;2c226 contributes00200000 for zero masks. All16 masks and two forced-disable cases executed in the original x86. The C codec matches all16 in four CPU variants. These are CPU encoding observations, not proof of physical color preservation.

3b890, previously suggested as depth setup, actually allocates an index command (20bytes at3b8d6), scales its index offset by2 at3b9c2 and emits relocation3b9db. Observed function contains no vertex-Z transformation. Following this path further is not a solution to Z export.

## Validation actually executed

Native, UBSan, i386/QEMU and i386 UBSan/QEMU pass focused geometry/control/negative/transactional tests. Each matches6 original vertex payloads (768bytes) and16 color-control contributions. Actual Meson-built driver archive passes the same test. Existing target/scene regression passes native, UBSan, i386 and actual archive. Mesa25.2 static/DRI rebuild passes. Unchanged 377455 encoder tests and32 NIR cases were not rerun in this depth-only round; prior evidence retained.

The first attempt used an absent i686-linux-gnu-gcc shell path. The subsequent static-UBSan attempt failed on toolchain TLS linkage; final i386 UBSan uses the established dynamic runtime with static-libubsan and passes. Both failure logs remain. Upstream DRI RWX warning is retained, not suppressed.

## Exact remaining functional boundary

GPU depth = ENOTSUP. Current format/surface/framebuffer admission still rejects Z; DSA still rejects depth_enabled; current native draw path admits only its32×32 one-triangle authenticated profile. Adding a visual alone would falsely advertise unsupported depth.

Clear vertices are WINDOW coordinates. The qualified independent VS currently consumes CLIP coordinates with its canonical viewport. These payloads cannot simply be connected without establishing the correct transform/state. Physical nonzero-Z export, Z backing/layout/visibility, complete ISP/scene state and preservation across the clear/occluding draws remain UNKNOWN. A CPU-built descriptor cannot prove any of these. A fresh new physical profile must bind its BOs, programs, packet and executable/library hashes; an old one-triangle card cannot authorize this new clear.

No new GLX GPU result or FPS is claimed. The previous physical OpenGL triangle and readback remain preserved. Private GLX CPU context/two-frame presentation is separately recorded in the parent checkpoint.

Additional boundary: the clear initializer constructs two ARB fragment-program objects at525ad/525cf, parses through52619/52654, and selector51e37 storesctx13b60 into the active program state. Whether this object writes fragment depth is UNKNOWN until its output metadata is audited. No proprietary shader source/instructions were exported for this step. See GHIDRA_HANDOFF.md.
