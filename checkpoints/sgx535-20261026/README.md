# SGX535 physical GLXGears checkpoint — 2026-10-26

Original GLXGears ran at **1280×800** on the Dell Mini 12 SGX535 rev121: **120 GPU submissions / 120 frames**, clean retirement/ACK protocol and close, zero simulation/legacy submissions, empty diagnostic capture directory. Original stdout reported **7.912 FPS warmup, 8.982 FPS in the following five-second window (~111.33 ms/frame)**. The 30–60 FPS / 19 ms objective remains open; sustained unbounded operation is unqualified.

The qualified private profile uses twelve `LESS` ISP states and a single covering Z-clear triangle. Existing typed encoders generated the packet; only three inline vertex-data bytes and one TA index-count byte change relative to the preceding LESS profile. All gear/shader bytes and the original 1280×800 background object remain unchanged. Logical capacity and indexed payload consumption are recorded separately in `SINGLE_CLEAR_PROFILE_RESULTS.json`.

Six physical operations verified whole-image COLOR silhouette, COLOR/Z co-coverage and clear background, plus nearest depth and visible gear class in 265,411–265,738 stable interior pixels per pose. All 72 STAGEs, six retirement/ACKs, nine negative gates, five input buffers, PTE/ownership checks and teardown passed. This is scoped qualification, not full GL shading/precision conformance or LAB approval.

The older Z-footprint verifier remains failed and preserved: the fix removes one or two old depth-only background artifacts per pose. Each removed point has old/new RGBA zero and an independent no-triangle reference. The separate corrected-clear contract rejects missing visible pixels, extra geometry, COLOR/Z disagreement and nonblack background. Its unit negatives and full synthetic journal/tamper tests passed before accepting the physical readbacks. The rejected 2048 background experiment and earlier failures remain recorded.

`qualified-single-clear/` snapshots the tested private source/profile separately from the integration tree. Physical raw readbacks, initrds, toolchains and complete original artifacts remain under `/home/gama/sgx535-offline`; `MANIFEST.json` records exact source paths and hashes. Existing rollback/stock boot was restored and N/N/N controls confirmed after each terminal test.

`fps-candidates/` contains isolated candidates: X11 request pipelining, borrowing held publisher mappings, and explicit MIT-SHM completion with event preservation. Their offline compilation/tests passed. **Physical FPS gains are unproven.** They are not substituted into the validated source snapshot. The SHM candidate requires loader verification of `libXext.so.6` before GPU execution.

The original client SHA-256 is `159aa90de37785038490ff51bb5d7de86f37d81c129fc8090e13f6687713dc37`. The qualified kernel/image identities and physical evidence are in `evidence/` and the source manifests. FAST normal rendering has no per-frame hashes, diagnostic checkpoints, pixel-oracle readbacks or dumps; COLOR readback remains necessary for X11 presentation.

Snapshot source and captured stdout retain their original bytes, including inherited whitespace. The integration-tree whitespace check excludes these byte-exact archival copies; their content hashes are verified by the checkpoint manifest.
