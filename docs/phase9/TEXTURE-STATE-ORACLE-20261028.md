# Original rev121 texture-state builder: selected CPU replay

A texture-state construction lead is now bound to a retained original library
whose build string explicitly specifies `SGX535`, `SGX_CORE_REV=121`,
`SUPPORT_SGX535` and `pc_i686_poulsbo_d0_linux`. It provides selected state-word
observations, not physical texture sampling or accelerated desktop completion.
No production driver, normal rendering loop, Mini12 or boot state changed.

Evidence root:
`/home/gama/sgx535-offline/texture-state-emgd-20261028/v1/`.
The source artifact is `libemgdsrv_um.so.1.5.15.3226` in
`/home/gama/sgx535-offline/phase9-full-legacy-driver-20261016/package-members/`,
SHA256 `5f3bbea4ea98907fd44be0f84fe8832d395b41c07fbc408b2c19f6c61c974e7c`.
`TARGET_BINDING.json` records the exact build string and address translation.
No Xpsb/psb_dri shader bytes or excluded PSP2 material were used.

## Static route and coverage

The exported PVR2D 3D-blit wrappers in `libEMGD2d` import `SGXQueueTransfer`.
REA/Ghidra followed the retained services implementation to a state builder
at Ghidra `0x1ea90` (original ELF VA/file offset `0xea90`).
`REA_STATE_HELPERS.json` retains complete original evidence, including:

- Queue dossier: `ev_57e7cccceba9d10f4ea1eb08abec580ff4d45beb94a23ebc94d88d8394cc2266`.
- State builder: `ev_5593ba9f47a73b6b1a985b632a54c35293e760c0fea82a65dab9f2039cafcd3a`.
- Layout selector: `ev_64b4ee84bbd1058f9fdeacbedbf4533cfb7e877362cc8eba3ee4b18a60b52a7d`.
- Numeric format helper: `ev_a0519bf0f2c63a2291c1c7521e632125d266b863a8e60ef1c30fdc958ab5deb6`.

Ghidra rebases this shared object by0x10000. Its large queue dossier has
noncontiguous function membership and its data-upload helper has an unresolved
jump table; neither is a complete semantic model of transfer submission. The
small state builder's actual instructions and selected outputs were checked
separately. A later helper initially considered for upload generates USSE
instructions; it is not evidence of a PDS decoder or a texture-state uploader.
No generated proprietary shader was reused.

## Bounded controlled replay

`replay_state.c` maps only the original first RX PT_LOAD privately, after exact
ELF/class/machine/segment/file-size checks; the launch verifies the binary SHA.
It calls only the audited pure state/format routines and their local PC thunks
and relative tables. It does not dynamically load the vendor library, execute
its constructors, call SGXQueueTransfer, or invoke any shader generator.
There is no writable executable mapping and the original library is unchanged.

The harness was compiled for i386 with warnings as errors and run under QEMU
with a15-second parent timeout and5-second guest alarm. `BUILD.json` and
`REPLAY.json` retain commands; stdout has240 records and strace has no ioctl or
DRM/device-memory opens. Every call checks untouched words around the destination.

The240 cases cover slots0/1/2 plus invalid3, numeric format IDs20/21, numeric
filter IDs0/1/2, numeric layout IDs1/3 and five dimension/pitch tuples:
1×1/1pixel,2×2/4,640×640/640,1280×800/1280,641×479/644.
There are180 state writes and60 invalid-slot no-write checks. The three state
words occupy offsets12,16,20 for slot0; later slots advance16 bytes. Twelve
bytes written by the CPU in a16-byte slot do not establish GPU fetch size,
state upload size, or the separate TA descriptor's capacity/alignment contract.

For the observed tuples, define F as0,0x1400,0x2800 for numeric filter0,1,2,
and C as0x0c000000/0x0d000000 for numeric format20/21:

```text
layout1:
  word0 = F | 0x001e0092
  word1 = 0x20000000 | C | ((width-1)<<12) | (height-1)
layout3, P=stride_pixels-1 (4 bytes/pixel in these cases):
  word0 = F | 0x80000000 | (P&0x1ff) | ((P&0xe00)<<9)
  word1 = 0x60000000 | C | ((width-1)<<12) | (height-1)
both:
  word2 = address & 0xfffffffc
```

These formulas are observations of selected builder branches, not a complete
ISA specification. `verify_replay.py` compares all240 original outputs to the
field model and checks all31 protected files. `REPLAY_VERIFY.json` records the
scope. The original builder masks address bits; a future driver must reject
invalid addresses instead of silently rounding them down.

The dual MIT/GPLv2 OMAP `servicesext.h` names IDs20/21 ARGB8888/ABGR8888 and
explains little-endian byte order. That is a cross-version reference, not the
exact EMGD build's header. These semantic names, numeric filter meanings and
clamp/border behavior are not promoted to production Gallium support here.

## Next required integration

The previous [SMP instruction observations](TEXTURE-SMP-ORACLE-20261028.md) and
this state-builder replay now narrow the unknowns. Establish the exact sampler
semantics, image layout/fetch bounds and PDS/SA state-delivery contract, then
implement the checked encoder and qualify a real textured draw. Preserve all
address/owner/range/coherency/retirement gates. The existing hardware descriptor
emitter remains ENOTSUP until those contracts can support it honestly.

Do not equate the12-byte CPU write with a proven3-word GPU read. Do not add CODE
to fill a capacity. The retained native copy/fill qualification remains valid,
but actual desktop acceleration still needs general textured rendering,
EGL/GBM/image ownership and safe Xorg/application coexistence. No red/green demo,
software-rendered desktop or API version advertisement replaces that end state.
