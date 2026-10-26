# Restricted two-parameter MUL+ADD: physical pair

The driver now independently emits the complete original compiler/linker
profile `POSITION=P*PARAM0+PARAM1`, common numeric kind0 with source tuples
(2,0)/(3,1)/(3,5), followed by the typed existing export tail. No original shader
image is used as a generator, no selector13 control is guessed and no CODE
padding is inserted. The exact old one-parameter APIs and gates remain intact.

CPU evidence and implementation:
`/home/gama/sgx535-offline/parameter-mad-second-vector-20261024T070000Z/`.
The complete linked original USE16 equals the typed emission. Native, UBSan,
i386/QEMU and the real Mesa archive tests pass. Five previous tests pass in
three environments (15 regressions); static and DRI builds pass. The later
first-term composition also preserves this profile in its regression set.
No full historical regression sweep is claimed.

Physical cases (one submission in each fresh boot):

- `/home/gama/sgx535-offline/native1280-parameter-mad-zero-20261024T070200Z/`:
  boot0d6872bf-a7a5-424f-b3f5-d1fda2de742a, PARAM0 all1, PARAM1 all0,
  COLOR SHA2569562f8cfae4446c552f790b655fe0cb1427c28c3ff5390186b6ff94cfbf52fca.
- `/home/gama/sgx535-offline/native1280-parameter-mad-shift-20261024T070400Z/`:
  bootc432335d-2479-4ee3-a79c-bb42847bcc6a, only PARAM1.x changes to2.125,
  COLOR SHA25635713c25a5299fa52d90e02a8f6cfd29510898b226ca843dfd0e12b3b9b04404.

Pair verifier:
`/home/gama/sgx535-offline/parameter-mad-physical-pair-20261024T071000Z/VERIFY_PHYSICAL_PAIR.py`.
Result `RESTRICTED_MAD_SECOND_PARAMETER_X_DELIVERY_PHYSICAL_LAB_PASS`.
Both full1280x800 images match the independent rational no-edge-tie oracles:
136green and1023864opaqueBLACK; every green position moves exactly2 columns.
Z remains all zero. Completion ledger7, retired phase9, immutable same-syscall
observer authentication, BIF PRE/POST0 and no fault-hook observation in each.
Module build notes and the unchanged native MMIO core/revision equality gate
bind admission to SGX535 core01130000/revision00010201. PCI revision06 is a
separate value and is not used as the SGX revision proof.

The explicit source extent is20DWORDs/80initialized bytes, with17effective
source words including self prefix, TA count5 and unchanged60-byte PDS image
(48DATA+12CODE). Source allocation capacity remains131072bytes/offset6400.
Every input image is hash-pinned before admission; each2190-ioctl memory
transcript validates uploads, CPU reads and all11 PTE/backing spans, ending at
zero objects/context closed. Full COLOR/Z snapshots are kernel-authenticated.
No full post-execution readback of every CODE/source/internal scratch BO was
captured, and there is no instruction trace. Do not infer consumption of all20
transferred words or absence of all possible internal faults from these checks.

CONFIRMED_PHYSICAL: the second dense coefficient vec4's selected X component
controls position in this MUL+ADD profile while PARAM0 is all1. This qualifies
that restricted case; it does not establish arbitrary multiplication scales,
all coefficient components/indices, general uniforms, a complete matrix,
full gears lighting/material NIR, continuous submission or any glxgears frame.
Other historical LAB auxiliary dependencies remain explicitly isolated.

All three new jobs in this continuation (transfer20 control plus MAD pair)
closed with runtime controlsN/N and stock saved boot settings restored BEFORE
submission. No retry or second submission in any boot was attempted. Existing
31 protected hashes were unchanged. The final boot is consumed and must not
be used for another native attempt.
