// SPDX-License-Identifier: GPL-2.0-only
/* First native hardware profile: semantic triangle plus a qualified external
 * program/state build. No SGX instruction bytes are defined here. Compile only
 * with the isolated FIRE3 triangle assets, never the square/cube seed tables.
 * This is a deliberately narrow first-draw profile, not general shader support.
 */
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <asm/unaligned.h>
#include "gma500_bo_owner.h"
#include "remesa_native_bridge.h"

static const struct remesa_native_draw expected = {
    .version = REMESA_NATIVE_ABI_VERSION,
    .width = 32, .height = 32, .vertex_count = 3, .index_count = 3,
    .argb = 0xffff00ff,
    /* IEEE754 representations of the previously tested window coordinates;
     * these are semantic vertex data, not GPU instructions. */
    .positions = {{0x41000000, 0x41000000, 0x3f000000, 0x3f800000},
                  {0x41c00000, 0x41000000, 0x3f000000, 0x3f800000},
                  {0x41000000, 0x41c00000, 0x3f000000, 0x3f800000}},
    .indices = {0, 1, 2},
};

static int publish(struct sgx535_gma500_owner *owner,
                   const struct remesa_native_draw *request)
{
    struct sgx535_frozen_cpu_view *vertices, *indices;
    unsigned int v, c, i;
    if (!owner || !request || !owner->initialized || owner->user_images_flushed ||
        owner->session.phase != SGX535_PHASE_HOST_IMAGE_FINAL ||
        memcmp(request, &expected, offsetof(struct remesa_native_draw, operation)))
        return -EINVAL;
    vertices = &owner->views[SGX535_BO_VERTEX_TA];
    indices = &owner->views[SGX535_BO_PDS];
    if (!vertices->bytes || vertices->length < 96 || !indices->bytes ||
        indices->length < 512 + 6)
        return -EINVAL;
    /* Layout bound to FIRE3 frozen-address-plan.json: vertices begin at byte
     * zero with stride32, indices at PDS+512. Refuse a mismatched seed before
     * copying caller input; unrelated bounds/program/state bytes stay intact. */
    for (v = 0; v < 3; ++v)
        for (c = 0; c < 4; ++c)
            if (get_unaligned_le32(vertices->bytes + v * 32 + c * 4) !=
                expected.positions[v][c])
                return -EINVAL;
    for (i = 0; i < 3; ++i)
        if (get_unaligned_le16(indices->bytes + 512 + i * 2) != expected.indices[i])
            return -EINVAL;
    for (v = 0; v < 3; ++v)
        for (c = 0; c < 4; ++c)
            put_unaligned_le32(request->positions[v][c], vertices->bytes + v * 32 + c * 4);
    for (i = 0; i < 3; ++i)
        put_unaligned_le16(request->indices[i], indices->bytes + 512 + i * 2);
    return 0;
}

int remesa_native_triangle_profile_install(void)
{
    static const struct remesa_qualified_asset asset = {
        .expected = &expected,
        .construct = sgx535_gma500_owner_construct,
        .publish = publish,
        .core_id = 0x01130000, .core_revision = 0x00010201,
        .timeout_ticks = 10 * HZ, .max_samples = 100000,
    };
    return remesa_native_asset_install(&asset);
}
