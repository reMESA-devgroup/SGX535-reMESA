# Retained USE program windows and bank controls

The general scene consumer needs the complete set of USE windows referenced
by its owned graph, rather than the fixed executor's two canonical entries.
`remesa_persistent_scene_use_snapshot()` derives those windows from a frozen
capture: three Z-clear programs, scene-close/draw-state auxiliaries, both
linked shader stages, the background program and two event auxiliaries.
Shared owners are visited once through the capture's deduplicated holds.

Each result binds the captured owner/serial, backing handle/generation, exact
producer offset/length, data master, bank and lower19 projection. The snapshot
reconstructs each existing entry plan against current owned storage, checks
the captured read-only reference and rejects bank conflicts. It does not infer
program length from reserved backing, padding or task metadata. The reference
capacity55 follows prefix3 + close1 + sixteen draws*3 + background1 + event2;
it is not extra CODE or an instruction-count claim.

The snapshot remains usable after public owners are released because capture
holds retain the graph. It performs no provider I/O, allocation, cache flush,
readback, hashing or hardware operation. Auxiliary-only captures still lack the
fixed event and return `ENOTSUP`. All scene-incomplete bits remain set.

## Native consumer

`remesa_gem_scene_use_prepare()` accepts another kernel-only immutable command
section containing held IDs and bounded program windows, never raw GPU VAs,
register words or CODE. The section must be disjoint from the matching action
and control sections, have an exact bounded count/size and zero reserved fields.
Scratch objects cannot be supplied as programs. Addresses derive from actual
exclusive retained backing, including its generation/mapping checks.

The existing userspace integer planner is staged for kernel compilation with
equivalent fixed-width type names, platform includes/errno alias and one moved loop-index
declaration. Its arithmetic is unchanged. The generated source/header normalize
back to the original byte-for-byte. The kernel retains the original strict
512KiB window endpoint and lower19 packing checks, rejects EDM rendering entries
and requires compatible register words when programs share a bank.

The bound-root check first authenticates the complete private loan and matching
action/control plans, then reconstructs all USE words and requires every
program binding to be GPU-read-only with the expected cache policy. No bank is
actually leased or programmed by these helpers. No source attribution, MMIO,
initialization or execution gate is added or bypassed.

## Validation and remaining boundary

Evidence directory:
`/home/gama/sgx535-offline/native-scene-use-20261101/v1`.
Mesa tests exercise the ten entries/two banks of the actual fixture graph,
their owned extents and fixups, stable snapshots through all eight capture
slots and frontend destruction, stale/foreign/alias refusal and both REGION
variants. Kernel models exercise complete17-resource loans, valid shared banks,
the55-entry limit, stale generations, scratch rejection, invalid bank/master,
window bounds/alignment, conflicts, RW/cache refusal and tampered plans.

The original and kernel-port planners run the existing526161 register-field
checks and375 entry/window checks under native, UBSan and static-i486 builds.
These are CPU models and codecs, not instruction or SGX execution qualification.
The first kernel test failure is preserved: its supposed invalid ID increment
selected the next live BO. The test now changes the generation instead; no
production refusal or protection was weakened.

The first full kernel build also identified the userspace-only `ENOTSUP`
name. Its private kernel translation uses `EOPNOTSUPP`, matching the original
Linux userspace error value; the failed build remains preserved. C90 style
diagnostics from the shared C99 producer are recorded rather than suppressed.

The kernel cannot yet certify that a supplied list covers every instruction
reference in TA/PDS or that instruction contents are valid. Complete image and
reference validation must bind this metadata to the actual immutable images
before general native execution is admitted. The old fixed11-role executor,
source/IRQ attribution and LAB gates remain intact. Correct ordered issue,
completion/retirement, physical qualification and normal loader/X11/full-API
integration remain necessary for the complete driver objective.
