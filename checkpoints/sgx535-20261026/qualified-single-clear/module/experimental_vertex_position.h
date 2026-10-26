/* SPDX-License-Identifier: MIT
 * Original numerical CPU-form generation, not a proprietary shader template.
 * Provenance: remesa_usse_vertex_candidate exact class2[0..3]->class1[0..3]
 * profile; valid original VS frontend->2e7f2/2e72c->4252d->30a09->30fb7.
 * Physical bank/export/termination semantics remain unqualified.
 */
#ifndef SGX535_EXPERIMENTAL_VERTEX_POSITION_H
#define SGX535_EXPERIMENTAL_VERTEX_POSITION_H
#include "frozen_kernel_contract.h"
#define SGX535_EXPERIMENTAL_VERTEX_OFFSET 0x40U
#define SGX535_EXPERIMENTAL_VERTEX_BYTES 16U
_Static_assert(SGX535_EXPERIMENTAL_VERTEX_OFFSET % 16U == 0,
               "selected USE entry alignment");
_Static_assert(SGX535_EXPERIMENTAL_VERTEX_OFFSET +
               SGX535_EXPERIMENTAL_VERTEX_BYTES <= 0x8000U,
               "selected USE allocation bounds");
struct sgx535_experimental_cpu_register { sgx535_u32 class_id, number; };
struct sgx535_experimental_cpu_move {
    struct sgx535_experimental_cpu_register source, destination;
    sgx535_u32 control12, control21, control23;
};
struct sgx535_experimental_cpu_tail {
    struct sgx535_experimental_cpu_register source[3];
    sgx535_u32 extension[3], control21, control12_13, control18;
};
static inline void sgx535_experimental_vertex_position(sgx535_u32 words[4])
{
    const struct sgx535_experimental_cpu_move body = {
        {2U, 0U}, {1U, 0U}, 3U, 1U, 1U
    };
    const struct sgx535_experimental_cpu_tail tail = {
        {{0U, 0U}, {2U, 0U}, {2U, 0U}}, {0U, 1U, 1U}, 1U, 1U, 1U
    };
    /* Selected original MOV-family CPU fields. The source and destination
     * numbers/classes match the prior eight-scalar body; only the original
     * repeat/control field changes from7 to3. No address or flow field grows. */
    words[0] = (body.source.number << 7) |
        (body.destination.number << 21) | (body.source.class_id << 30) |
        (1U << 29);
    words[1] = body.destination.class_id | (5U << 27) | (1U << 16) |
        (body.control12 << 12) | (body.control21 << 21) |
        (body.control23 << 23);
    /* Original4252d arguments(0,8,8,0,1,0,0,1), helper tag8 -> class2/ext1.
     * These are numerical CPU rules; no control is named hardware END. */
    words[2] = (tail.source[0].number << 14) |
        (tail.source[1].number << 7) | tail.source[2].number |
        (tail.source[1].class_id << 30) | (tail.source[2].class_id << 28) |
        (tail.control21 << 21);
    words[3] = (31U << 27) | (3U << 24) | (1U << 21) | (1U << 14) |
        (tail.source[0].class_id << 2) | (tail.extension[0] << 19) |
        (tail.extension[1] << 17) | (tail.extension[2] << 16) |
        (tail.control12_13 << 12) | (tail.control18 << 18);
}
#endif
