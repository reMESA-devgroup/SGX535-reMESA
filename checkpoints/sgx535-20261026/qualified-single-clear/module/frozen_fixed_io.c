#include "frozen_fixed_io.h"

int sgx535_fixed_run_actions(const struct sgx535_fixed_io *io,
                              void *context,
                              const struct sgx535_frozen_reg_action *actions,
                              size_t count, sgx535_u32 max_poll_reads)
{
    size_t i;

    if (!io || !io->write || !io->read || !io->barrier || !actions ||
        !count || count > 31U || !max_poll_reads ||
        max_poll_reads > 300000U)
        return SGX535_FROZEN_BAD_REQUEST;
    for (i = 0; i < count; i++) {
        const struct sgx535_frozen_reg_action *a = &actions[i];
        sgx535_u32 n, value = 0;
        int matched = 0;

        if (a->kind == SGX535_REG_WMB) {
            if (a->offset || a->value || a->mask || io->barrier(context))
                return SGX535_FROZEN_BAD_REQUEST;
            continue;
        }
        if ((a->offset & 3U) || a->offset >= 0x1000U)
            return SGX535_FROZEN_BAD_REQUEST;
        if (a->kind == SGX535_REG_WRITE) {
            if (a->mask || io->write(context, a->offset, a->value))
                return SGX535_FROZEN_BAD_REQUEST;
            continue;
        }
        if ((a->kind != SGX535_REG_POLL_SET &&
             a->kind != SGX535_REG_POLL_CLEAR) ||
            !a->mask || (a->value & ~a->mask) ||
            (a->kind == SGX535_REG_POLL_CLEAR && a->value))
            return SGX535_FROZEN_BAD_REQUEST;
        for (n = 0; n < max_poll_reads; n++) {
            if (io->read(context, a->offset, &value))
                return SGX535_FROZEN_BAD_REQUEST;
            if ((value & a->mask) == a->value) {
                matched = 1;
                break;
            }
        }
        if (!matched)
            return SGX535_FROZEN_BAD_REQUEST;
    }
    return SGX535_FROZEN_OK;
}
