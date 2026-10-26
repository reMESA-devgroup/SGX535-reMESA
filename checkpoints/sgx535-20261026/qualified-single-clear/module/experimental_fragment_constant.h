#ifndef SGX535_EXPERIMENTAL_FRAGMENT_CONSTANT_H
#define SGX535_EXPERIMENTAL_FRAGMENT_CONSTANT_H

#include "frozen_kernel_contract.h"

/* EXPERIMENTAL_HYPOTHESIS -- NOT ESTABLISHED.
 * CPU construction from retained psb_dri.so, SHA-256 74ca4299...:
 * clear builder ELF 0x39ac4 -> 0x30ffd(default flags 0x0804)
 * -> 0x30d15(destination descriptor 1, packed ARGB) -> 0x30fb7.
 * This is the historical one-instruction clear-color constructor, not a
 * general ISA encoder or proof that the frozen primitive reaches fragments.
 * Destination bank(1)=1 at 0x304d9; extension(1)=0 at 0x30cb1.
 * The packed immediate occupies word0[20:0], word1[8:4], word1[17:12].
 * Default constructor flags and final END produce the fixed remaining bits.
 */
static inline void sgx535_experimental_fragment_constant(
    sgx535_u32 argb, sgx535_u32 words[2])
{
    words[0] = argb & 0x001fffffU;
    words[1] = 0xfca40001U | (((argb >> 21) & 31U) << 4)
               | ((argb >> 26) << 12);
}

#define SGX535_EXPERIMENTAL_FRAGMENT_ARGB 0xffff00ffU
/* frozen_triangle_bo.build(): fragment_use is USE BO+0, size8, align32.
 * Same allocation, length, relocations and PDS launch; no other payload change.
 */
#define SGX535_EXPERIMENTAL_FRAGMENT_OFFSET 0U

#endif
