# Compact owned depth scene — Mini12 1280×800

IMPLEMENTED_OFFLINE: generated VS, three constant FS, clear geometry, draw geometry, PDS DATA/CODE, state updates and TA commands are placed in one driver-owned 4096-byte allocation. The TA stream is 124 bytes. Address fixups derive from the allocated GPU VA. This replaces eighteen small fetched allocations with one; it does not change the backend sixteen-resource limit.

The primary diagnostic target is 1280×800, with a 5120-byte color stride and 4096000-byte color readback. Geometry: (160,100), (1120,100), (160,675); draw Z values 0.75,0.25,0.75; clear Z 1; LESS with writes enabled. The integer pixel-center oracle predicts 276000 magenta pixels and 748000 opaque black pixels. Oracle SHA-256: a5db3894b515f31b3d656fe91393dfc6419b08a7042a2d7fff182e44c467b03c. This is an expected image, not a hardware capture.

Existing 32×32 tests remain regression references. 1600×768 is not a target of this increment. No GPU submissions or SSH occurred.

The owner API rejects caller-assigned VAs, public writes, aliased output pointers and malformed requests. Readback and sync use the injected provider in CPU tests. Host shader images may be destroyed after upload. Mapped resources retain ownership until the final lease closes.

BLOCKED physical qualification: this small-object arena is not a complete native GPU job. Color/Z, scene/region/DPM backing, merged raster prefix, ownership of every page and full-frame readback authentication still need a bounded native profile. The old exact 32×32 admission path must not be reused by changing only dimensions. ADMISSION and gate files remain unchanged.

Next executable step: construct a separately validated native resource inventory joining this arena to the existing variable-size color/Z/scene layout, verify every fetch and prefix address against ownership, and qualify multi-page readback before submitting a 1280×800 diagnostic. A depth PASS requires physical completion, retirement, authenticated full readback and contrasting LESS/ALWAYS or write-disabled controls.
