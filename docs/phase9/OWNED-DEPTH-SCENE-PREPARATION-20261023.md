# Owned depth scene preparation

Status: IMPLEMENTED_OFFLINE. Physical depth and glxgears are not validated by this increment.

The existing immutable state uploader now accepts the demonstrated source sizes and retains the shader token's code, PDS and geometry until every referring state allocation has actually been disposed. Public shader deletion cannot release those dependencies while a state object or its CPU lease survives. Ordinary failures roll back; uncertain provider effects retain quarantine. The original two-DWORD ISP and final-state layouts are preserved.

The explicit selected F41 draw profile remains available. The separately named `remesa_persistent_after_zclear_state_prepare()` uses the original cached B41 first-draw contract (52 bytes), while `remesa_persistent_fragment_state_prepare()` emits a 16-byte group40 update. These APIs name predecessor requirements; they do not infer a live cache, execute a clear or schedule GPU work. Every fragment reference is derived from the endpoint's own immutable PDS allocation.

`test_owned_depth_scene.c` joins the real encoders and injected resource owner into one CPU fixture: a scene Z-clear quad, three overlapping position-only triangles at Z=.75/.25/.75 with green/magenta/yellow shaders, and final scene close. The command stream contains four state/index pairs and a close, totaling124 bytes. It derives18 distinct fetched allocation handles, and checks that dependencies survive public shader deletion and are eventually released. Those handles are an inventory, **not** native job holds or proof of MMU requester coverage. No device is opened by the test.

The qualified physical profile still validates an exact depthless image and ten bindings. Its native dispatcher does not accept this new resource set or whole-scene command image. The remaining step is a separately qualified, bounded native scene contract that holds Z, command/state, vertex/index, generated USE/PDS and auxiliary resources through authenticated retirement. Existing admission and execution gates remain unchanged. No physical depth support or general OpenGL capability is enabled.
