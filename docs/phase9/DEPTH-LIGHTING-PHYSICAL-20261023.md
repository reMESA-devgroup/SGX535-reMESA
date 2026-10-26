# Restricted depth and lighting physical controls — 2026-10-23

The target is an owned1280×800 BGRA framebuffer, pitch5120. The tested geometry remains one16-pixel-wide reference triangle:136 covered pixel centers. This is a LAB qualification, not general OpenGL depth, complete lighting, glxgears or continuous rendering. Historical auxiliary programs remain explicit LAB dependencies. ADMISSION and28 gate files are unchanged.

## Confirmed physical observations

* A compound CPU caller-finalization contribution changes reg480 from054f00 to454fd0. Relative to the otherwise identical black D2 scene, the authenticated Z snapshot changes from zero to1024000DWORDs00ffffff. COLOR stays black. Individual bit semantics remain unknown.
* Adding the typed Z attachment/finalization and ISP ALWAYS/write to the previously successful COPY triangle preserves136green pixels. Exactly136 Z words become00800000 and occupy precisely those green-pixel positions with linear5120-byte pitch. The active nonzero Z extent is the top-left48×48:2168clear words00ffffff plus136half-depth words; remaining Z stays zero. The input vertex Z is0.5. No general tiling or quantization rule is inferred beyond these observations.
* Switching the known reference to the compiled POSITION-only PDS/TA profile, without changing its geometry or VS16/FS8, preserves the expected136green pixels. Thus that profile transition alone does not explain the later zero-pixel cases.
* The independently generated64-byte POSITION+white diffuse VS, separate P/N/L attributes, eight-byte varying FS, generated primary PDS and depth ALWAYS/write produced136WHITE pixels with the expected hash, completion and retirement. The Z image is identical to the successful COPY+depth control. The differential changed only L.z from+1 to−1 at the three vertices. The color became allzero while the complete Z snapshot stayed identical (136half-depth words at the same positions). Both jobs completed and retired with authenticated captures. This qualifies the selected unit-light response profile; arbitrary lighting inputs remain unqualified.

All observations use same-syscall kernel-origin immutable fullCOLOR/Z snapshots, native-observer hashes and completion/retirement closure. BIF PRE/POST samples are zero and no requester-fault hook was captured. This is not instruction trace or proof that no transient fault was possible.

* The three-draw occlusion differential uses unchanged GREEN/MAGENTA/GREEN geometry in Z order0.75/0.25/0.75. ALWAYS/write ends136GREEN and rawZ00c00000. LESS/write ends136MAGENTA and rawZ00400000; the last farther triangle does not overwrite the nearer result. Only two ISP comparison words differ between the submitted immutable inputs; CPU order/index data and all other GPU bytes are identical. Both fullCOLOR oracles and linear136-position Z masks match, with completion/retirement and native-observer authentication. This qualifies LESS/write occlusion within one scene for this LAB profile.

## Implemented offline

Typed white-diffuse emission computes POSITION=P and COLOR.xyzw=clamp(dot(N.xyz,L.xyz),0,1), including alpha. It requires caller-supplied unit vectors; it supplies no matrices, RSQ normalization, uniforms, material transport or general NIR admission. Five complete original compiler outputs independently match. LogicalUSE64, reserved144 and alignment32 are distinct. Whole original programmable PDS/TA oracles establish input48 and output32 bytes. Existing transactional validators reject other combinations. Static Mesa andDRI builds and focused native/UBSan/i386/archive tests pass.

## Evidence and remaining limits

Private evidence checkpoints: native1280-d2-finalizer-bif-20261023T220700Z, native1280-copy-depth-bif-candidate-20261023T222500Z, native1280-position-only-profile-bif-20261023T221800Z and native1280-white-lambert-depth-bif-candidate-20261023T223200Z; each hardware directory contains authenticated captures. The lighting encoder proof is lambert-vs-minimal-20261023T221500Z.

Color hashes: COPY+depth9562f8cfae4446c552f790b655fe0cb1427c28c3ff5390186b6ff94cfbf52fca; white1bacc8bbd208f303529a8c2e3203f885aacaa3c03fb49aec9f4311a6537f1f60. Shared Z hash0c3f92bbad7816c951c53cec976891aa715e252900cbb727c187b182e3b5215e.

Depth functions other than the tested ALWAYS/LESS, write-disabled behavior, stencil, general preservation across submissions, arbitrary lighting inputs, nonzero viewport matrices and repeated execution remain unqualified. The earlier failed VS40/MATRIX cases remain preserved; replacing their varying FS by constantGREEN was insufficient. The successful combined case also changes PDS placement and viewport, so it does not isolate the original failure. GPU submission admission is not broadened by this document. First physical PDS PC remainsUNKNOWN.

Occlusion evidence: native1280-copy-occlusion-bif-20261023T230800Z/hardware and hardware-less. LESS colorSHA d63c8de2e94c9a0d497b03ded0ea88010f05923fa2a2cd1956064610aafcd991; ZSHA276162bda8de5aa3798c7c4c6fd499cf57ed629935952e2844f0653ea8e1627f. Lighting differential: native1280-negative-light-bif-candidate-20261023T224900Z/hardware.
