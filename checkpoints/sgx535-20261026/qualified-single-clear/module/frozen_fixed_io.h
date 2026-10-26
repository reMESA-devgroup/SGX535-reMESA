#ifndef SGX535_FROZEN_FIXED_IO_H
#define SGX535_FROZEN_FIXED_IO_H

#include "frozen_fixed_service.h"

/* Internal executor boundary for generated fixed actions. No user request
 * can provide an action array. The backend must supply its own qualified
 * read/write/barrier implementations and a finite poll budget. */
struct sgx535_fixed_io {
    int (*write)(void *context, sgx535_u32 offset, sgx535_u32 value);
    int (*read)(void *context, sgx535_u32 offset, sgx535_u32 *value);
    int (*barrier)(void *context);
};

int sgx535_fixed_run_actions(const struct sgx535_fixed_io *io,
                              void *context,
                              const struct sgx535_frozen_reg_action *actions,
                              size_t count, sgx535_u32 max_poll_reads);

#endif
