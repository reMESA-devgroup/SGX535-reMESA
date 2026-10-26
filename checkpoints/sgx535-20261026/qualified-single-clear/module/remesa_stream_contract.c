#include "remesa_stream_contract_aliases.h"
#include "frozen_kernel_contract.h"

/* Explicit offline experimental build only. Normal builds retain FIRE #2's
 * original bytes. This option carries no live/execution authorization. */
#ifdef SGX535_EXPERIMENTAL_CONSTANT_FRAGMENT
#if SGX535_EXPERIMENTAL_CONSTANT_FRAGMENT != 1
#error "experimental constant fragment must be explicitly selected as 1"
#endif
#include "experimental_fragment_constant.h"
#endif

/* Private offline build only; no installed driver or admission gate change. */
#ifdef SGX535_EXPERIMENTAL_OWN_VERTEX
#if SGX535_EXPERIMENTAL_OWN_VERTEX != 1
#error "experimental own vertex must be explicitly selected as 1"
#endif
#include "experimental_vertex_position.h"
#endif

#ifdef REMESA_LAB_DEPTH1280_PROFILE
#include "remesa/scene_layout.h"
#endif

_Static_assert(sizeof(struct sgx535_frozen_request) == 16,
               "fixed frozen request must remain 16 bytes");
_Static_assert(sizeof(struct sgx535_frozen_xhw_bind_fire_wire) == 132,
               "selected XHW request must remain 132 bytes");

static const struct sgx535_frozen_requirement requirements[SGX535_BO_COUNT] = {
    [SGX535_BO_PDS] = {0x20000, 0x1000, SGX535_DOMAIN_PDS},
    [SGX535_BO_USE] = {0x8000, 0x8000, SGX535_DOMAIN_PDS},
#ifdef REMESA_LAB_DEPTH1280_PROFILE
    /* Private GEN-only address/window discriminant. */
    [SGX535_BO_VERTEX_TA] = {0x20000, 0x1000, SGX535_DOMAIN_PDS},
#else
    [SGX535_BO_VERTEX_TA] = {0x1000, 0x1000, SGX535_DOMAIN_MMU},
#endif
    [SGX535_BO_BACKGROUND] = {0x1000, 0x1000, SGX535_DOMAIN_RASTGEOM},
    [SGX535_BO_CONTROL] = {0x1000, 0x1000, SGX535_DOMAIN_LOCAL},
#ifdef REMESA_LAB_DEPTH1280_PROFILE
    [SGX535_BO_COLOR] = {4096000, 0x1000, SGX535_DOMAIN_MMU},
    [SGX535_BO_SCENE_HW] = {122880, 0x1000, SGX535_DOMAIN_MMU},
#else
    [SGX535_BO_COLOR] = {0x1000, 0x1000, SGX535_DOMAIN_MMU},
    [SGX535_BO_SCENE_HW] = {0x2000, 0x1000, SGX535_DOMAIN_MMU},
#endif
    [SGX535_BO_TA_PAGE_TABLE] = {0x2000000, 0x1000, SGX535_DOMAIN_MMU},
    [SGX535_BO_TA_PARAMETER] = {0x2000000, 0x100000, SGX535_DOMAIN_RASTGEOM},
    [SGX535_BO_XHW_COMM] = {0x1000, 0x1000, SGX535_DOMAIN_LOCAL},
#ifdef REMESA_LAB_DEPTH1280_PROFILE
    [SGX535_BO_Z] = {4096000, 0x1000, SGX535_DOMAIN_MMU},
#endif
};

static const sgx535_u32 expected_relocations[72][10] = {
#include "frozen_kernel_relocations.inc"
};

struct frozen_seed {
    sgx535_u32 role;
    sgx535_u32 offset;
    sgx535_u32 value;
};

static const struct frozen_seed initial_dwords[] = {
#include "frozen_kernel_initial.inc"
};

static const sgx535_u32 expected_ta_offsets[7] = {
    0x204, 0x218, 0x23c, 0x240, 0x244, 0x250, 0x238
};

static const sgx535_u32 expected_raster_offsets[26] = {
    0x400, 0x404, 0x40c, 0x410, 0x414, 0x418, 0x41c, 0x420,
    0x424, 0x42c, 0x480, 0xcb0, 0x484, 0x488, 0x48c, 0x490,
    0x494, 0x4c4, 0x4bc, 0x4b8, 0x4c8, 0x4dc, 0x800, 0xa5c,
    0xa60, 0xa64
};

_Static_assert(sizeof(expected_relocations) == 72U * 40U,
               "the selected relocation wire must remain 72 records");

static void store_le32(sgx535_u8 *bytes, sgx535_u32 word);
static sgx535_u32 load_le32(const sgx535_u8 *bytes);

static int validate_user_views(const struct sgx535_frozen_cpu_view *views)
{
    size_t i, j;

    if (views == NULL)
        return SGX535_FROZEN_BAD_RELOCATION;
    for (i = 0; i <= SGX535_BO_COLOR; i++) {
        unsigned long start, end;
        if (views[i].bytes == NULL ||
            views[i].length != requirements[i].size)
            return SGX535_FROZEN_BAD_RELOCATION;
        start = (unsigned long)views[i].bytes;
        if (views[i].length > ~0UL - start)
            return SGX535_FROZEN_BAD_RELOCATION;
        end = start + views[i].length;
        for (j = 0; j < i; j++) {
            unsigned long other = (unsigned long)views[j].bytes;
            if (start < other + views[j].length && other < end)
                return SGX535_FROZEN_ALIAS;
        }
    }
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_initialize_user_images(
    struct sgx535_frozen_cpu_view *views)
{
    size_t i, byte;
    int result = validate_user_views(views);

    if (result != SGX535_FROZEN_OK)
        return result;
    for (i = 0; i < sizeof(initial_dwords) / sizeof(initial_dwords[0]); i++) {
        const struct frozen_seed *seed = &initial_dwords[i];
        if (seed->role > SGX535_BO_COLOR || (seed->offset & 3U) ||
            seed->offset > views[seed->role].length - 4U)
            return SGX535_FROZEN_BAD_RELOCATION;
    }
    for (i = 0; i <= SGX535_BO_COLOR; i++) {
        for (byte = 0; byte < views[i].length; byte++)
            views[i].bytes[byte] = 0;
    }
    for (i = 0; i < sizeof(initial_dwords) / sizeof(initial_dwords[0]); i++) {
        const struct frozen_seed *seed = &initial_dwords[i];
        store_le32(views[seed->role].bytes + seed->offset, seed->value);
    }
#ifdef SGX535_EXPERIMENTAL_CONSTANT_FRAGMENT
    {
        sgx535_u32 words[2];
        sgx535_experimental_fragment_constant(
            SGX535_EXPERIMENTAL_FRAGMENT_ARGB, words);
        store_le32(views[SGX535_BO_USE].bytes +
                   SGX535_EXPERIMENTAL_FRAGMENT_OFFSET, words[0]);
        store_le32(views[SGX535_BO_USE].bytes +
                   SGX535_EXPERIMENTAL_FRAGMENT_OFFSET + 4U, words[1]);
    }
#endif
#ifdef SGX535_EXPERIMENTAL_OWN_VERTEX
    {
        sgx535_u32 words[4];
        sgx535_experimental_vertex_position(words);
        for (i = 0; i < 4; ++i)
            store_le32(views[SGX535_BO_USE].bytes +
                       SGX535_EXPERIMENTAL_VERTEX_OFFSET + 4U * i, words[i]);
    }
#endif
#ifdef SGX535_EXPERIMENTAL_QUARTER_TRIANGLE
#if SGX535_EXPERIMENTAL_QUARTER_TRIANGLE != 1
#error "experimental quarter triangle must be explicitly selected as 1"
#endif
    /* Private exact raster probe. Same three records, no allocation/code or
     * relocation change. IEEE754 window 8.25/24.25 from canonical viewport. */
    {
        static const sgx535_u32 xy[3][2] = {
            {0x41040000U, 0x41040000U},
            {0x41c20000U, 0x41040000U},
            {0x41040000U, 0x41c20000U}
        };
        for (i = 0; i < 3; ++i) {
            store_le32(views[SGX535_BO_VERTEX_TA].bytes + i * 32, xy[i][0]);
            store_le32(views[SGX535_BO_VERTEX_TA].bytes + i * 32 + 4, xy[i][1]);
        }
    }
#endif
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_validate_request(const void *request, size_t length)
{
    const sgx535_u8 *fixed = request;

    if (fixed == NULL || length != sizeof(struct sgx535_frozen_request))
        return SGX535_FROZEN_BAD_REQUEST;
    /* The i386 wire is little-endian bytes; the caller's buffer need not
     * have struct alignment or effective type. Decode without typed loads. */
    if (load_le32(fixed) != 1 || load_le32(fixed + 4) != 1 ||
        load_le32(fixed + 8) != 0 || load_le32(fixed + 12) != 0)
        return SGX535_FROZEN_BAD_REQUEST;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_get_requirement(sgx535_u32 role,
                                  struct sgx535_frozen_requirement *out)
{
    if (role >= SGX535_BO_COUNT || out == NULL)
        return SGX535_FROZEN_BAD_ROLE;
    *out = requirements[role];
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_validate_bos(const struct sgx535_frozen_bo *bos,
                                size_t count, sgx535_u64 mmu_end)
{
    sgx535_u32 seen = 0;
    size_t i, j;

    if (bos == NULL || count != SGX535_BO_COUNT)
        return SGX535_FROZEN_BAD_COUNT;
    if (mmu_end <= 0x40000000ULL || mmu_end > 0x100000000ULL ||
        (mmu_end & 0xfffU) != 0)
        return SGX535_FROZEN_BAD_MMU_END;

    for (i = 0; i < count; i++) {
        const struct sgx535_frozen_bo *bo = &bos[i];
        const struct sgx535_frozen_requirement *required;
        sgx535_u64 lower, upper;

        if (bo->role >= SGX535_BO_COUNT || (seen & (1U << bo->role)))
            return SGX535_FROZEN_BAD_ROLE;
        seen |= 1U << bo->role;
        required = &requirements[bo->role];
        if (bo->size != required->size)
            return SGX535_FROZEN_BAD_SIZE;
        if (bo->domain != required->domain)
            return SGX535_FROZEN_BAD_DOMAIN;
        if (bo->owner_token == 0)
            return SGX535_FROZEN_BAD_OWNER;
        for (j = 0; j < i; j++) {
            if (bos[j].owner_token == bo->owner_token)
                return SGX535_FROZEN_BAD_OWNER;
        }

        if (bo->domain == SGX535_DOMAIN_LOCAL) {
            if (bo->gpu_va != 0)
                return SGX535_FROZEN_BAD_ADDRESS;
            continue;
        }
        if (bo->gpu_va > 0xffffffffULL)
            return SGX535_FROZEN_BAD_ADDRESS;
        /* Fixed alignments are powers of two. Avoid a 64-bit division
         * runtime dependency on i386, and reject contract drift explicitly. */
        if (required->alignment == 0 ||
            (required->alignment & (required->alignment - 1U)) != 0 ||
            (bo->gpu_va & (required->alignment - 1U)) != 0)
            return SGX535_FROZEN_BAD_ALIGNMENT;
        if (bo->domain == SGX535_DOMAIN_PDS) {
            lower = 0x20000000ULL;
            upper = 0x30000000ULL;
        } else if (bo->domain == SGX535_DOMAIN_RASTGEOM) {
            lower = 0x30000000ULL;
            upper = 0x40000000ULL;
        } else {
            lower = 0x40000000ULL;
            upper = mmu_end;
        }
        if (bo->gpu_va < lower || bo->gpu_va >= upper ||
            bo->size > upper - bo->gpu_va)
            return SGX535_FROZEN_BAD_ADDRESS;

        for (j = 0; j < i; j++) {
            const struct sgx535_frozen_bo *other = &bos[j];
            if (other->domain == SGX535_DOMAIN_LOCAL)
                continue;
            if (bo->gpu_va < other->gpu_va + other->size &&
                other->gpu_va < bo->gpu_va + bo->size)
                return SGX535_FROZEN_ALIAS;
        }
    }

    return seen == ((1U << SGX535_BO_COUNT) - 1U)
        ? SGX535_FROZEN_OK : SGX535_FROZEN_BAD_ROLE;
}

int sgx535_frozen_validate_publication_pages(
    const struct sgx535_frozen_bo *bos, size_t count, sgx535_u64 mmu_end,
    const sgx535_u32 page_counts[SGX535_BO_COUNT],
    const sgx535_u32 mapped_pages[SGX535_BO_COUNT])
{
    size_t i;
    int result = sgx535_frozen_validate_bos(bos, count, mmu_end);

    if (result != SGX535_FROZEN_OK)
        return result;
    if (page_counts == NULL || mapped_pages == NULL)
        return SGX535_FROZEN_BAD_OWNER;
    for (i = 0; i < count; i++) {
        const struct sgx535_frozen_bo *bo = &bos[i];
        sgx535_u32 expected = (sgx535_u32)(bo->size >> 12);

        if (page_counts[bo->role] != expected ||
            mapped_pages[bo->role] != (bo->domain == SGX535_DOMAIN_LOCAL
                ? 0 : expected))
            return SGX535_FROZEN_BAD_OWNER;
    }
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_validate_relocations(const sgx535_u32 *words,
                                        size_t count)
{
    size_t i, field;

    if (count != 72)
        return SGX535_FROZEN_BAD_COUNT;
    if (words == NULL)
        return SGX535_FROZEN_BAD_RELOCATION;
    for (i = 0; i < count; i++) {
        for (field = 0; field < 10; field++) {
            if (words[i * 10 + field] != expected_relocations[i][field])
                return SGX535_FROZEN_BAD_RELOCATION;
        }
    }
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_plan_use_bases(const struct sgx535_frozen_bo *bos,
                                 size_t count, sgx535_u64 mmu_end,
                                 struct sgx535_frozen_use_plan *plan)
{
    struct sgx535_frozen_use_plan candidate = {{{0}}, 0};
    const struct sgx535_frozen_bo *use_bo = NULL;
    size_t i;
    int result = sgx535_frozen_validate_bos(bos, count, mmu_end);

    if (result != SGX535_FROZEN_OK)
        return result;
    if (plan == NULL)
        return SGX535_FROZEN_BAD_RELOCATION;
    for (i = 0; i < count; i++) {
        if (bos[i].role == SGX535_BO_USE)
            use_bo = &bos[i];
    }
    if (use_bo == NULL)
        return SGX535_FROZEN_BAD_ROLE;

    /* Candidate psb_init_use_base(3, 13) starts with a free list ordered
     * 3..15. The selected program first asks for pixel DM 1, then vertex
     * DM 0. This is a plan for an otherwise idle manager, not a live claim. */
    for (i = 0; i < 72; i++) {
        const sgx535_u32 *r = expected_relocations[i];
        struct sgx535_frozen_use_entry *entry;
        sgx535_u32 dm = r[9];
        sgx535_u64 address, size, base;

        if (r[0] != 4 && r[0] != 5)
            continue;
        if (r[2] != SGX535_BO_USE || dm >= 2 || r[8] == 0 ||
            r[5] >= use_bo->size || r[8] > use_bo->size - r[5] ||
            use_bo->gpu_va > 0xffffffffULL - r[5])
            return SGX535_FROZEN_BAD_RELOCATION;
        address = use_bo->gpu_va + r[5];
        size = r[8];
        base = address & ~0x7ffffULL;
        if (address + size >= base + 0x80000ULL ||
            (base >> 7) > 0x01ffffffULL)
            return SGX535_FROZEN_BAD_RELOCATION;
        entry = &candidate.by_data_master[dm];
        if (entry->reg == 0) {
            if (candidate.assigned_count >= 13)
                return SGX535_FROZEN_BAD_RELOCATION;
            entry->reg = 3 + candidate.assigned_count++;
            entry->base = (sgx535_u32)base;
            entry->register_offset = 0x0a0cU + 4U * entry->reg;
            entry->register_word = (sgx535_u32)(base >> 7) | (dm << 25);
        } else if (entry->base != base) {
            return SGX535_FROZEN_BAD_RELOCATION;
        }
    }
    if (candidate.assigned_count != 2)
        return SGX535_FROZEN_BAD_RELOCATION;
    *plan = candidate;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_validate_register_offsets(const sgx535_u32 *ta_offsets,
                                              size_t ta_count,
                                              const sgx535_u32 *raster_offsets,
                                              size_t raster_count)
{
    size_t i;

    if (ta_count != 7 || raster_count != 26)
        return SGX535_FROZEN_BAD_COUNT;
    if (ta_offsets == NULL || raster_offsets == NULL)
        return SGX535_FROZEN_BAD_REGISTER_LIST;
    for (i = 0; i < ta_count; i++) {
        if (ta_offsets[i] != expected_ta_offsets[i])
            return SGX535_FROZEN_BAD_REGISTER_LIST;
    }
    for (i = 0; i < raster_count; i++) {
        if (raster_offsets[i] != expected_raster_offsets[i])
            return SGX535_FROZEN_BAD_REGISTER_LIST;
    }
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_scene_info32(struct sgx535_frozen_scene_info *out)
{
#ifdef REMESA_LAB_DEPTH1280_PROFILE
    struct remesa_scene_layout layout;
    struct sgx535_frozen_scene_info candidate = {0};
    if (!out || remesa_scene_layout_build(1280, 800, 0, &layout))
        return SGX535_FROZEN_BAD_REQUEST;
    candidate.width = 1280;
    candidate.height = 800;
    memcpy(candidate.cookie, layout.cookie, sizeof(layout.cookie));
    /* Cookie15 is unused software-only wire padding in this normal path.
     * Original whole-CPU traces leave itcc and never read it. Zero here is
     * deterministic host initialization, NOT a recovered hardware meaning. */
    candidate.bo_size = layout.required_bytes;
    candidate.clear_page_start = layout.clear_first_page;
    candidate.clear_page_count = layout.clear_page_count;
    *out = candidate;
    return SGX535_FROZEN_OK;
#else

    static const sgx535_u32 selected_cookie[16] = {
        0, 0x10, 0x01004004, 0x01004004,
        0x10, 0x1001, 0x1f01f, 0x1000,
        0, 0x1300, 0x1350, 0x13e0,
        0, 0, 0, 0
    };
    size_t i;

    if (!out)
        return SGX535_FROZEN_BAD_REQUEST;
    out->width = 32;
    out->height = 32;
    for (i = 0; i < 16; i++)
        out->cookie[i] = selected_cookie[i];
    out->bo_size = 0x1420;
    out->clear_page_start = 0;
    out->clear_page_count = 1;
    return SGX535_FROZEN_OK;

#endif
}

int sgx535_frozen_xhw_bind_fire_wire(
    const struct sgx535_frozen_bo *bos, size_t count, sgx535_u64 mmu_end,
    sgx535_u32 engine, sgx535_u32 hw_context,
    struct sgx535_frozen_xhw_bind_fire_wire *out)
{
    struct sgx535_frozen_xhw_bind_fire_wire candidate = {{0}};
    struct sgx535_frozen_scene_info scene_info;
    const struct sgx535_frozen_bo *scene_bo = NULL;
    size_t i;

    if (!out || engine > 1U || hw_context != 0U)
        return SGX535_FROZEN_BAD_REQUEST;
    if (sgx535_frozen_validate_bos(bos, count, mmu_end))
        return SGX535_FROZEN_BAD_ADDRESS;
    if (sgx535_frozen_scene_info32(&scene_info))
        return SGX535_FROZEN_BAD_REQUEST;
    for (i = 0; i < count; i++)
        if (bos[i].role == SGX535_BO_SCENE_HW)
            scene_bo = &bos[i];
    if (!scene_bo)
        return SGX535_FROZEN_BAD_ROLE;
    candidate.words[0] = 2U; /* PSB_XHW_SCENE_BIND_FIRE */
    for (i = 0; i < 16U; i++)
        candidate.words[4U + i] = scene_info.cookie[i];
    candidate.words[20] = 1U; /* selected normal task fire_flags */
    candidate.words[21] = hw_context;
    candidate.words[22] = (sgx535_u32)scene_bo->gpu_va;
    candidate.words[23] = engine;
    candidate.words[24] = engine ? 15U : 4U;
    *out = candidate;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_ta_cookie(const struct sgx535_frozen_bo *bos,
                            size_t count, sgx535_u64 mmu_end,
                            struct sgx535_frozen_ta_cookie *out)
{
    struct sgx535_frozen_ta_cookie candidate = {{0}, 0};
    const struct sgx535_frozen_bo *pt = NULL, *param = NULL;
    sgx535_u32 pages, start;
    size_t i;
    int result;

    if (!out)
        return SGX535_FROZEN_BAD_REQUEST;
    result = sgx535_frozen_validate_bos(bos, count, mmu_end);
    if (result != SGX535_FROZEN_OK)
        return result;
    for (i = 0; i < count; i++) {
        if (bos[i].role == SGX535_BO_TA_PAGE_TABLE)
            pt = &bos[i];
        else if (bos[i].role == SGX535_BO_TA_PARAMETER)
            param = &bos[i];
    }
    if (!pt || !param || (pt->gpu_va & 0xfffU) ||
        (param->gpu_va & 0xfffffU))
        return SGX535_FROZEN_BAD_ALIGNMENT;
    pages = (sgx535_u32)(param->size >> 12);
    start = ((sgx535_u32)param->gpu_va & 0x0fffffffU) >> 12;
    if (pages <= 0x61fU || pages > 0xffffU ||
        start + pages - 1U > 0xffffU)
        return SGX535_FROZEN_BAD_ADDRESS;
    candidate.info_bo_size = 0x620000U;
    candidate.words[0] = (sgx535_u32)pt->gpu_va;
    candidate.words[1] = (sgx535_u32)param->gpu_va & 0x0fffffffU;
    candidate.words[2] = pages;
    candidate.words[3] = pages - 0x600U;
    candidate.words[4] = pages - 0x600U;
    candidate.words[5] = pages - 0x600U;
    candidate.words[6] = pages - 0x640U;
    candidate.words[10] = start;
    candidate.words[11] = start + pages - 1U;
    candidate.words[12] = candidate.words[11] - 1U;
    *out = candidate;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_rev121_init_writes(
    sgx535_u32 raw_revision, struct sgx535_frozen_reg_write *out,
    size_t count)
{
    static const struct sgx535_frozen_reg_write selected[12] = {
        {0xca0U, 0xc07cU}, {0x13cU, 0}, {0xa7cU, 0}, {0xa80U, 0},
        {0xa74U, 0x05188200U}, {0xa78U, 0}, {0xaacU, 0x44U},
        {0xabcU, 0xcU}, {0x804U, 0xffffU}, {0xa00U, 0x7c000U},
        {0x630U, 0}, {0xa58U, 0}
    };
    size_t i;

    if (!out || raw_revision != 0x00010201U)
        return SGX535_FROZEN_BAD_REQUEST;
    if (count != 12U)
        return SGX535_FROZEN_BAD_COUNT;
    for (i = 0; i < 12U; i++)
        out[i] = selected[i];
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_ta_load_plan(const struct sgx535_frozen_bo *bos,
                               size_t count, sgx535_u64 mmu_end,
                               struct sgx535_frozen_reg_action *out,
                               size_t action_count)
{
    struct sgx535_frozen_ta_cookie cookie;
    struct sgx535_frozen_reg_action candidate[29] = {{0}};
    sgx535_u32 table_range, last_endpoint, low;
    size_t i;
    int result;
#define ACTION(_i, _kind, _off, _value, _mask) do { \
    candidate[_i].kind = (_kind); \
    candidate[_i].offset = (_off); \
    candidate[_i].value = (_value); \
    candidate[_i].mask = (_mask); \
} while (0)

    if (!out)
        return SGX535_FROZEN_BAD_REQUEST;
    if (action_count != 29U)
        return SGX535_FROZEN_BAD_COUNT;
    result = sgx535_frozen_ta_cookie(bos, count, mmu_end, &cookie);
    if (result != SGX535_FROZEN_OK)
        return result;
    table_range = (cookie.words[11] << 16) | cookie.words[10];
    last_endpoint = cookie.words[12] << 16;
    low = (cookie.words[8] | cookie.words[7]) & 0xffffU;
    ACTION(0, SGX535_REG_WRITE, 0x618U, cookie.words[0], 0);
    ACTION(1, SGX535_REG_WRITE, 0x61cU, table_range, 0);
    ACTION(2, SGX535_REG_WRITE, 0x648U, last_endpoint, 0);
    ACTION(3, SGX535_REG_WRITE, 0x638U, low, 0);
    ACTION(4, SGX535_REG_WRITE, 0x620U, cookie.words[4] & 0xffffU, 0);
    ACTION(5, SGX535_REG_WRITE, 0x624U, cookie.words[5] & 0xffffU, 0);
    ACTION(6, SGX535_REG_WRITE, 0x628U, cookie.words[3] & 0xffffU, 0);
    ACTION(7, SGX535_REG_WRITE, 0x668U, cookie.words[6] & 0xffffU, 0);
    ACTION(8, SGX535_REG_WRITE, 0x684U, 1, 0);
    ACTION(9, SGX535_REG_WRITE, 0x600U, cookie.words[0], 0);
    ACTION(10, SGX535_REG_WRITE, 0x604U, table_range, 0);
    ACTION(11, SGX535_REG_WRITE, 0x64cU, last_endpoint, 0);
    ACTION(12, SGX535_REG_WRITE, 0x660U, low, 0);
    ACTION(13, SGX535_REG_WRITE, 0x680U, 1, 0);
    ACTION(14, SGX535_REG_WRITE, 0x610U, cookie.words[0], 0);
    ACTION(15, SGX535_REG_WRITE, 0x614U, table_range, 0);
    ACTION(16, SGX535_REG_WRITE, 0x654U, last_endpoint, 0);
    ACTION(17, SGX535_REG_WRITE, 0x688U, 1, 0);
    ACTION(18, SGX535_REG_WRITE, 0x608U, cookie.words[0], 0);
    ACTION(19, SGX535_REG_WRITE, 0x60cU, table_range, 0);
    ACTION(20, SGX535_REG_WRITE, 0x650U, last_endpoint, 0);
    ACTION(21, SGX535_REG_WRITE, 0x690U, 1, 0);
    /* Four current-operation load kicks above require four completions.
     * SGX535 STATUS2 bit3 is DPM_DHOST_FREE_LOAD, not TA completion.
     * Consume it only after observing it alongside the other load bits;
     * otherwise it can leak into the scene ledger and correctly cause HOLD. */
    ACTION(22, SGX535_REG_POLL_SET, 0x118U, 15, 15);
    ACTION(23, SGX535_REG_WRITE, 0x114U, 15, 0);
    ACTION(24, SGX535_REG_POLL_CLEAR, 0x118U, 0, 15);
    ACTION(25, SGX535_REG_WRITE, 0x6a8U, 1, 0);
    ACTION(26, SGX535_REG_POLL_SET, 0x12cU, 0x400000U, 0x400000U);
    ACTION(27, SGX535_REG_WRITE, 0x134U, 0x400000U, 0);
    ACTION(28, SGX535_REG_POLL_CLEAR, 0x12cU, 0, 0x400000U);
    for (i = 0; i < 29U; i++)
        out[i] = candidate[i];
#undef ACTION
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_ta_fire_plan(const struct sgx535_frozen_bo *bos,
                               size_t count, sgx535_u64 mmu_end,
                               struct sgx535_frozen_reg_action *out,
                               size_t action_count)
{
    struct sgx535_frozen_reg_action candidate[31] = {{0}};
    struct sgx535_frozen_scene_info scene_info;
    const struct sgx535_frozen_bo *scene = NULL;
    sgx535_u32 base;
    size_t i;
    int result;
#define FIRE_ACTION(_i, _kind, _off, _value, _mask) do { \
    candidate[_i].kind = (_kind); \
    candidate[_i].offset = (_off); \
    candidate[_i].value = (_value); \
    candidate[_i].mask = (_mask); \
} while (0)

    if (!out)
        return SGX535_FROZEN_BAD_REQUEST;
    if (action_count != 31U)
        return SGX535_FROZEN_BAD_COUNT;
    result = sgx535_frozen_validate_bos(bos, count, mmu_end);
    if (result != SGX535_FROZEN_OK)
        return result;
    for (i = 0; i < count; i++) {
        if (bos[i].role == SGX535_BO_SCENE_HW)
            scene = &bos[i];
    }
#ifdef REMESA_LAB_DEPTH1280_PROFILE
    result = sgx535_frozen_scene_info32(&scene_info);
    if (result != SGX535_FROZEN_OK)
        return result;
    if (!scene || scene->size < scene_info.bo_size ||
        scene_info.cookie[7] >= scene->size ||
        scene->gpu_va > 0xffffffffULL - scene_info.cookie[7])
        return SGX535_FROZEN_BAD_ADDRESS;
#else
    if (!scene || scene->size < 0x1420U ||
        scene->gpu_va > 0xffffffffULL - 0x1000U)
        return SGX535_FROZEN_BAD_ADDRESS;
    result = sgx535_frozen_scene_info32(&scene_info);
    if (result != SGX535_FROZEN_OK)
        return result;
#endif
    base = (sgx535_u32)scene->gpu_va;
    /* The historical TTM backend sets BO offset to the MMU insertion VA.
     * This plan models the first fresh scene only: context 0, setup flag 4,
     * no OOM branch, and initial control word +0x630 == 0. */
    FIRE_ACTION(0, SGX535_REG_WRITE, 0x220U, base + scene_info.cookie[8], 0);
    FIRE_ACTION(1, SGX535_REG_WRITE, 0x65cU, 0, 0);
    FIRE_ACTION(2, SGX535_REG_WRITE, 0x694U, 2, 0);
    FIRE_ACTION(3, SGX535_REG_WRITE, 0x698U, 2, 0);
    FIRE_ACTION(4, SGX535_REG_WRITE, 0x24cU, 1, 0);
    FIRE_ACTION(5, SGX535_REG_WRITE, 0x224U, 0x80000000U, 0);
    FIRE_ACTION(6, SGX535_REG_WRITE, 0x208U, scene_info.cookie[2], 0);
    FIRE_ACTION(7, SGX535_REG_WRITE, 0x20cU, scene_info.cookie[3], 0);
    FIRE_ACTION(8, SGX535_REG_WRITE, 0x214U, scene_info.cookie[4], 0);
    FIRE_ACTION(9, SGX535_REG_WRITE, 0x210U, scene_info.cookie[5], 0);
    FIRE_ACTION(10, SGX535_REG_WRITE, 0x248U, scene_info.cookie[6], 0);
    FIRE_ACTION(11, SGX535_REG_WRITE, 0x21cU, base + scene_info.cookie[7], 0);
    FIRE_ACTION(12, SGX535_REG_WRITE, 0x274U, 0, 0);
    FIRE_ACTION(13, SGX535_REG_POLL_SET, 0x12cU, 0x100a40U, 0x100a40U);
    FIRE_ACTION(14, SGX535_REG_WRITE, 0x134U, 0x100a40U, 0);
    FIRE_ACTION(15, SGX535_REG_POLL_CLEAR, 0x12cU, 0, 0x100a40U);
    FIRE_ACTION(16, SGX535_REG_WRITE, 0xc90U, 0x30000000U, 0);
    FIRE_ACTION(17, SGX535_REG_WRITE, 0xad4U, 1, 0);
    FIRE_ACTION(18, SGX535_REG_POLL_SET, 0x138U, 0x44U, 0x44U);
    FIRE_ACTION(19, SGX535_REG_WRITE, 0x140U, 0x44U, 0);
    FIRE_ACTION(20, SGX535_REG_POLL_CLEAR, 0x138U, 0, 0x44U);
    FIRE_ACTION(21, SGX535_REG_WRITE, 0xae0U, 1, 0);
    FIRE_ACTION(22, SGX535_REG_POLL_SET, 0x138U, 1, 1);
    FIRE_ACTION(23, SGX535_REG_WRITE, 0x140U, 1, 0);
    FIRE_ACTION(24, SGX535_REG_POLL_CLEAR, 0x138U, 0, 1);
    FIRE_ACTION(25, SGX535_REG_WRITE, 0x804U, 0x1000ffffU, 0);
    FIRE_ACTION(26, SGX535_REG_POLL_SET, 0x12cU, 0x4000000U, 0x4000000U);
    FIRE_ACTION(27, SGX535_REG_WRITE, 0x134U, 0x4000000U, 0);
    FIRE_ACTION(28, SGX535_REG_POLL_CLEAR, 0x12cU, 0, 0x4000000U);
    FIRE_ACTION(29, SGX535_REG_WRITE, 0xa08U, 1, 0);
    FIRE_ACTION(30, SGX535_REG_WRITE, 0x200U, 1, 0);
    for (i = 0; i < 31U; i++)
        out[i] = candidate[i];
#undef FIRE_ACTION
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_raster_fire_plan(const struct sgx535_frozen_bo *bos,
                                   size_t count, sgx535_u64 mmu_end,
                                   struct sgx535_frozen_reg_action *out,
                                   size_t action_count)
{
    struct sgx535_frozen_reg_action candidate[20] = {{0}};
    struct sgx535_frozen_scene_info scene_info;
    const struct sgx535_frozen_bo *scene = NULL;
    sgx535_u32 base;
    size_t i;

    if (!out)
        return SGX535_FROZEN_BAD_REQUEST;
    if (action_count != 20U)
        return SGX535_FROZEN_BAD_COUNT;
    if (sgx535_frozen_validate_bos(bos, count, mmu_end) ||
        sgx535_frozen_scene_info32(&scene_info))
        return SGX535_FROZEN_BAD_ADDRESS;
    for (i = 0; i < count; i++)
        if (bos[i].role == SGX535_BO_SCENE_HW)
            scene = &bos[i];
    if (!scene || scene_info.cookie[14] != 0 ||
        scene->gpu_va > 0xffffffffULL - scene_info.cookie[7])
        return SGX535_FROZEN_BAD_ADDRESS;
    base = (sgx535_u32)scene->gpu_va;
#define RA(_i, _kind, _off, _value, _mask) do { \
    candidate[_i] = (struct sgx535_frozen_reg_action){ \
        (_kind), (_off), (_value), (_mask)}; \
} while (0)
    /* Xpsb_scene_switch_fire raster engine, normal flags 15, fresh cookie
     * word 14 zero, then Xpsb_closed_kick_render. The original helper did
     * not propagate every wait error; a live executor must fail closed. */
    RA(0, SGX535_REG_WRITE, 0x630U, 3, 0);
    RA(1, SGX535_REG_WRITE, 0x65cU, 0, 0);
    RA(2, SGX535_REG_WRITE, 0x63cU, 3, 0);
    RA(3, SGX535_REG_WRITE, 0x658U, 0, 0);
    RA(4, SGX535_REG_WRITE, 0x408U, base + scene_info.cookie[7], 0);
    RA(5, SGX535_REG_WRITE, 0xad4U, 1, 0);
    RA(6, SGX535_REG_POLL_SET, 0x138U, 0x44U, 0x44U);
    RA(7, SGX535_REG_WRITE, 0x140U, 0x44U, 0);
    RA(8, SGX535_REG_POLL_CLEAR, 0x138U, 0, 0x44U);
    RA(9, SGX535_REG_WRITE, 0xae0U, 1, 0);
    RA(10, SGX535_REG_POLL_SET, 0x138U, 1, 1);
    RA(11, SGX535_REG_WRITE, 0x140U, 1, 0);
    RA(12, SGX535_REG_POLL_CLEAR, 0x138U, 0, 1);
    RA(13, SGX535_REG_WRITE, 0x804U, 0x1000ffffU, 0);
    RA(14, SGX535_REG_POLL_SET, 0x12cU, 0x4000000U, 0x4000000U);
    RA(15, SGX535_REG_WRITE, 0x134U, 0x4000000U, 0);
    RA(16, SGX535_REG_POLL_CLEAR, 0x12cU, 0, 0x4000000U);
    RA(17, SGX535_REG_WRITE, 0x43cU, 1, 0);
    RA(18, SGX535_REG_WRITE, 0xa08U, 1, 0);
    RA(19, SGX535_REG_WRITE, 0x428U, 1, 0);
#undef RA
    for (i = 0; i < 20U; i++)
        out[i] = candidate[i];
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_ta_schedule_plan(
    const struct sgx535_frozen_command_pairs *commands,
    struct sgx535_frozen_reg_action *out, size_t action_count)
{
    struct sgx535_frozen_reg_action candidate[8];
    size_t i;

    if (!commands || !out || action_count != 8U)
        return SGX535_FROZEN_BAD_REQUEST;
    for (i = 0; i < 7U; i++) {
        if (commands->ta[2U * i] != expected_ta_offsets[i])
            return SGX535_FROZEN_BAD_REGISTER_LIST;
        candidate[i] = (struct sgx535_frozen_reg_action){
            SGX535_REG_WRITE, commands->ta[2U * i],
            commands->ta[2U * i + 1U], 0};
    }
    candidate[7] = (struct sgx535_frozen_reg_action){
        SGX535_REG_WMB, 0, 0, 0};
    for (i = 0; i < 8U; i++)
        out[i] = candidate[i];
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_raster_schedule_plan(
    const struct sgx535_frozen_command_pairs *commands,
    struct sgx535_frozen_reg_action *out, size_t action_count)
{
    struct sgx535_frozen_reg_action candidate[29];
    size_t i;

    if (!commands || !out || action_count != 29U)
        return SGX535_FROZEN_BAD_REQUEST;
    for (i = 0; i < 26U; i++)
        if (commands->raster[2U * i] != expected_raster_offsets[i])
            return SGX535_FROZEN_BAD_REGISTER_LIST;
    candidate[0] = (struct sgx535_frozen_reg_action){
        SGX535_REG_WRITE, 0x80U, 0x20U, 0};
    candidate[1] = (struct sgx535_frozen_reg_action){
        SGX535_REG_WRITE, 0x80U, 0, 0};
    for (i = 0; i < 26U; i++)
        candidate[i + 2U] = (struct sgx535_frozen_reg_action){
            SGX535_REG_WRITE, commands->raster[2U * i],
            commands->raster[2U * i + 1U], 0};
    candidate[28] = (struct sgx535_frozen_reg_action){
        SGX535_REG_WMB, 0, 0, 0};
    for (i = 0; i < 29U; i++)
        out[i] = candidate[i];
    return SGX535_FROZEN_OK;
}

static sgx535_u32 load_le32(const sgx535_u8 *bytes)
{
    return (sgx535_u32)bytes[0] | ((sgx535_u32)bytes[1] << 8) |
           ((sgx535_u32)bytes[2] << 16) | ((sgx535_u32)bytes[3] << 24);
}

int sgx535_frozen_summarize_color(const sgx535_u8 *bytes, size_t length,
                                   struct sgx535_frozen_color_summary *out)
{
    struct sgx535_frozen_color_summary candidate = {0};
    size_t i;

    if (!bytes || !out)
        return SGX535_FROZEN_BAD_REQUEST;
    if (length != 4096U)
        return SGX535_FROZEN_BAD_SIZE;
    candidate.fnv1a = 2166136261U;
    for (i = 0; i < length; i++) {
        candidate.fnv1a ^= bytes[i];
        candidate.fnv1a *= 16777619U;
    }
    for (i = 0; i < 1024U; i++) {
        if (load_le32(bytes + i * 4U)) {
            candidate.nonzero_pixels++;
            candidate.row_nonzero[i / 32U]++;
        }
    }
    *out = candidate;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_extract_commands(
    const struct sgx535_frozen_cpu_view *control,
    struct sgx535_frozen_command_pairs *out)
{
    struct sgx535_frozen_command_pairs candidate;
    size_t i;

    if (!control || !out || !control->bytes || control->length != 0x1000U)
        return SGX535_FROZEN_BAD_REQUEST;
    for (i = 0; i < 26U; i++) {
        candidate.raster[2U * i] =
            load_le32(control->bytes + 8U * i);
        candidate.raster[2U * i + 1U] =
            load_le32(control->bytes + 8U * i + 4U);
        if (candidate.raster[2U * i] != expected_raster_offsets[i])
            return SGX535_FROZEN_BAD_REGISTER_LIST;
    }
    for (i = 0; i < 7U; i++) {
        candidate.ta[2U * i] =
            load_le32(control->bytes + 208U + 8U * i);
        candidate.ta[2U * i + 1U] =
            load_le32(control->bytes + 208U + 8U * i + 4U);
        if (candidate.ta[2U * i] != expected_ta_offsets[i])
            return SGX535_FROZEN_BAD_REGISTER_LIST;
    }
    *out = candidate;
    return SGX535_FROZEN_OK;
}

static void store_le32(sgx535_u8 *bytes, sgx535_u32 word)
{
    bytes[0] = (sgx535_u8)word;
    bytes[1] = (sgx535_u8)(word >> 8);
    bytes[2] = (sgx535_u8)(word >> 16);
    bytes[3] = (sgx535_u8)(word >> 24);
}

struct frozen_patch {
    sgx535_u32 role;
    sgx535_u32 offset;
    sgx535_u32 value;
};

int sgx535_frozen_apply_relocations(const struct sgx535_frozen_bo *bos,
                                     size_t count, sgx535_u64 mmu_end,
                                     struct sgx535_frozen_cpu_view *views,
                                     const sgx535_u32 use_registers[2])
{
    const struct sgx535_frozen_bo *by_role[SGX535_BO_COUNT];
    struct frozen_patch patches[72];
    size_t i, j;
    int result = sgx535_frozen_validate_bos(bos, count, mmu_end);

    if (result != SGX535_FROZEN_OK)
        return result;
    if (views == NULL || use_registers == NULL || use_registers[0] >= 16 ||
        use_registers[1] >= 16 || use_registers[0] == use_registers[1])
        return SGX535_FROZEN_BAD_RELOCATION;
    for (i = 0; i < count; i++)
        by_role[bos[i].role] = &bos[i];
    result = validate_user_views(views);
    if (result != SGX535_FROZEN_OK)
        return result;
    /* The canonical control payload has 264 bytes before 72 wire records. */
    if (views[SGX535_BO_CONTROL].length < 264U + 72U * 40U)
        return SGX535_FROZEN_BAD_RELOCATION;

    /* Stage every result before mutating the caller's image. */
    for (i = 0; i < 72; i++) {
        const sgx535_u32 *r = expected_relocations[i];
        sgx535_u32 op = r[0], where = r[1], source = r[2];
        sgx535_u32 mask = r[3], right = r[4] >> 16, left = r[4] & 0xffffU;
        sgx535_u32 destination = r[7], old, background, value;
        sgx535_u64 address, base;

        if ((source > SGX535_BO_COLOR && source != SGX535_BO_Z) || destination > SGX535_BO_COLOR ||
            right >= 32 || left >= 32 ||
            (sgx535_u64)where * 4U + 4U > views[destination].length ||
            r[5] >= by_role[source]->size ||
            ((op == 4 || op == 5) &&
             r[8] > by_role[source]->size - r[5]) ||
            by_role[source]->gpu_va > 0xffffffffULL - r[5])
            return SGX535_FROZEN_BAD_RELOCATION;
        /* Exact private Z contributions only; all other auxiliary sources
         * remain forbidden. All11 BO ownership/PTE holds are checked before
         * this serializer by the unchanged native owner/retained adapter. */
        if (source == SGX535_BO_Z) {
            sgx535_u32 reg;
            if (op != 0 || destination != SGX535_BO_CONTROL || !where ||
                r[5] || r[6] || r[8] || r[9] || right || left ||
                by_role[source]->gpu_va != 0x403e9000ULL ||
                by_role[source]->size != 4096000ULL ||
                by_role[source]->domain != SGX535_DOMAIN_MMU ||
                !by_role[source]->owner_token)
                return SGX535_FROZEN_BAD_RELOCATION;
            reg = load_le32(views[destination].bytes + (size_t)where * 4U - 4U);
            if ((reg == 0xcb0U && mask != 0xf0000000U) ||
                ((reg == 0x484U || reg == 0x488U || reg == 0x48cU || reg == 0x490U) && mask != 0x0ffffff0U) ||
                (reg != 0xcb0U && reg != 0x484U && reg != 0x488U && reg != 0x48cU && reg != 0x490U))
                return SGX535_FROZEN_BAD_RELOCATION;
        }
        address = by_role[source]->gpu_va + r[5];
        if (by_role[source]->domain == SGX535_DOMAIN_LOCAL)
            return SGX535_FROZEN_BAD_RELOCATION;
        old = load_le32(views[destination].bytes + (size_t)where * 4U);
        for (j = 0; j < i; j++) {
            if (patches[j].role == destination &&
                patches[j].offset == where * 4U)
                old = patches[j].value;
        }
        background = r[6];
        if (op == 0) {
            value = (sgx535_u32)address;
        } else if (op == 4 || op == 5) {
            if (r[9] >= 2)
                return SGX535_FROZEN_BAD_RELOCATION;
            base = address & ~0x7ffffULL;
            if (address + r[8] >= base + 0x80000ULL)
                return SGX535_FROZEN_BAD_RELOCATION;
            value = op == 5 ? use_registers[r[9]] : (sgx535_u32)(address - base);
            if (op == 4)
                background = old;
        } else {
            return SGX535_FROZEN_BAD_RELOCATION;
        }
        patches[i].role = destination;
        patches[i].offset = where * 4U;
        patches[i].value = (background & ~mask) |
                           ((((value >> right) << left) & mask));
    }

    for (i = 0; i < 72; i++)
        store_le32(views[patches[i].role].bytes + patches[i].offset,
                   patches[i].value);
    for (i = 0; i < 72; i++) {
        for (j = 0; j < 10; j++)
            store_le32(views[SGX535_BO_CONTROL].bytes + 264U + i * 40U + j * 4U,
                       expected_relocations[i][j]);
    }
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_begin(struct sgx535_frozen_session *session,
                                const void *request, size_t request_length,
                                const struct sgx535_frozen_bo *bos,
                                size_t count, sgx535_u64 mmu_end)
{
    int result;

    if (session == NULL || session->phase != SGX535_PHASE_EMPTY)
        return SGX535_FROZEN_BAD_REQUEST;
    result = sgx535_frozen_validate_request(request, request_length);
    if (result != SGX535_FROZEN_OK)
        return result;
    result = sgx535_frozen_validate_bos(bos, count, mmu_end);
    if (result != SGX535_FROZEN_OK)
        return result;
    session->sequence = 0;
    session->timeout_ticks = 0;
    session->observed_events = 0;
    session->ta_memory_free_seen = 0;
    session->raster_started = 0;
    session->phase = SGX535_PHASE_BOS_VALIDATED;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_advance(struct sgx535_frozen_session *session,
                                  enum sgx535_frozen_phase next)
{
    if (session == NULL ||
        session->phase < SGX535_PHASE_BOS_VALIDATED ||
        session->phase >= SGX535_PHASE_DEVICE_MAINTAINED ||
        next != session->phase + 1)
        return SGX535_FROZEN_BAD_REQUEST;
    session->phase = next;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_service_ready(
    struct sgx535_frozen_session *session,
    const struct sgx535_frozen_bootstrap *boot, int use_owned)
{
    if (!session || session->phase != SGX535_PHASE_DEVICE_MAINTAINED ||
        !sgx535_frozen_bootstrap_ready(boot) || use_owned != 1)
        return SGX535_FROZEN_BAD_REQUEST;
    session->phase = SGX535_PHASE_SERVICE_READY;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_abort(struct sgx535_frozen_session *session)
{
    if (session == NULL || session->phase < SGX535_PHASE_BOS_VALIDATED ||
        session->phase >= SGX535_PHASE_SCENE_COMPLETED)
        return SGX535_FROZEN_BAD_REQUEST;
    session->phase = session->phase < SGX535_PHASE_FIRE_POSSIBLE
        ? SGX535_PHASE_ABORTED_BEFORE_SUBMIT
        : SGX535_PHASE_HELD_AFTER_FAILURE;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_enter_fire(struct sgx535_frozen_session *session,
                                     sgx535_u32 sequence,
                                     sgx535_u32 timeout_ticks)
{
    if (!session || session->phase != SGX535_PHASE_SERVICE_READY ||
        !sequence || !timeout_ticks)
        return SGX535_FROZEN_BAD_REQUEST;
    session->sequence = sequence;
    session->timeout_ticks = timeout_ticks;
    session->observed_events = 0;
    session->ta_memory_free_seen = 0;
    session->raster_started = 0;
    session->phase = SGX535_PHASE_FIRE_POSSIBLE;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_begin_raster(struct sgx535_frozen_session *session,
                                       sgx535_u32 sequence)
{
    if (!session || session->phase != SGX535_PHASE_FIRE_POSSIBLE ||
        !session->sequence || sequence != session->sequence ||
        session->observed_events != 1U || session->raster_started)
        return SGX535_FROZEN_BAD_REQUEST;
    session->raster_started = 1;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_observe(struct sgx535_frozen_session *session,
                                  sgx535_u32 sequence,
                                  enum sgx535_frozen_event event)
{
    sgx535_u32 bit;

    if (!session || session->phase != SGX535_PHASE_FIRE_POSSIBLE ||
        !session->sequence || sequence != session->sequence)
        return SGX535_FROZEN_BAD_REQUEST;
    if (event == SGX535_EVENT_SGX_MMU_FAULT) {
        session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
        return SGX535_FROZEN_OK;
    }
    if (event < SGX535_EVENT_TA_FINISHED ||
        event > SGX535_EVENT_DPM_3D_MEM_FREE ||
        (event != SGX535_EVENT_TA_FINISHED &&
         (!(session->observed_events & 1U) || !session->raster_started)))
        return SGX535_FROZEN_BAD_REQUEST;
    bit = 1U << (event - 1);
    if (session->observed_events & bit)
        return SGX535_FROZEN_BAD_REQUEST;
    session->observed_events |= bit;
    if (session->observed_events == 7U)
        session->phase = SGX535_PHASE_SCENE_COMPLETED;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_observe_status(
    struct sgx535_frozen_session *session, sgx535_u32 sequence,
    sgx535_u32 status1, sgx535_u32 status2, int exclusive_owned)
{
    const sgx535_u32 selected_hold_events =
        (1U << 28) | (1U << 25) | (1U << 12) |
        (1U << 3) | (1U << 2) | (1U << 1);
    /* The historical scheduler handles DPM_TA_MEM_FREE separately from
     * task completion. It may be observed, but cannot retire this scene. */
    const sgx535_u32 ta_memory_free = 1U << 24;
    const sgx535_u32 ta = 1U << 13;
    const sgx535_u32 raster = (1U << 18) | 1U;
    int result;

    if (!session || exclusive_owned != 1 ||
        session->phase != SGX535_PHASE_FIRE_POSSIBLE ||
        !session->sequence || sequence != session->sequence)
        return SGX535_FROZEN_BAD_REQUEST;
    if ((status1 & ~(selected_hold_events | ta_memory_free | ta | raster)) ||
        (status2 & ~(1U << 4))) {
        session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
        return SGX535_FROZEN_BAD_REQUEST;
    }
    if ((status2 & (1U << 4)) || (status1 & selected_hold_events)) {
        session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
        return SGX535_FROZEN_OK;
    }
    if (status1 & ta_memory_free) {
        if (session->ta_memory_free_seen) {
            session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
            return SGX535_FROZEN_BAD_REQUEST;
        }
        session->ta_memory_free_seen = 1;
    }
    if (!(status1 & (ta | raster)))
        return (status1 & ta_memory_free) ? SGX535_FROZEN_OK :
               SGX535_FROZEN_BAD_REQUEST;
    if (((status1 & ta) && (status1 & raster)) ||
        ((status1 & raster) && !session->raster_started)) {
        session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
        return SGX535_FROZEN_BAD_REQUEST;
    }
    if (status1 & ta) {
        result = sgx535_frozen_session_observe(
            session, sequence, SGX535_EVENT_TA_FINISHED);
        if (result)
            session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
        return result;
    }
    if (status1 & 1U) {
        result = sgx535_frozen_session_observe(
            session, sequence, SGX535_EVENT_DPM_3D_MEM_FREE);
        if (result) {
            session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
            return result;
        }
    }
    if (status1 & (1U << 18)) {
        result = sgx535_frozen_session_observe(
            session, sequence, SGX535_EVENT_PIXELBE_END_RENDER);
        if (result)
            session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
        return result;
    }
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_timeout(struct sgx535_frozen_session *session)
{
    if (!session || session->phase != SGX535_PHASE_FIRE_POSSIBLE ||
        !session->timeout_ticks)
        return SGX535_FROZEN_BAD_REQUEST;
    session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_session_retire(struct sgx535_frozen_session *session)
{
    if (!session || session->phase != SGX535_PHASE_SCENE_COMPLETED)
        return SGX535_FROZEN_BAD_REQUEST;
    session->phase = SGX535_PHASE_RETIRED;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_bootstrap_begin(struct sgx535_frozen_bootstrap *boot,
                                  sgx535_u32 raw_revision)
{
    if (!boot || boot->last_event || raw_revision != 0x00010201U)
        return SGX535_FROZEN_BAD_REQUEST;
    boot->raw_revision = raw_revision;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_bootstrap_observe(struct sgx535_frozen_bootstrap *boot,
                                    enum sgx535_frozen_boot_event event,
                                    sgx535_u32 value)
{
    if (!boot || boot->raw_revision != 0x00010201U ||
        event != (enum sgx535_frozen_boot_event)(boot->last_event + 1) ||
        event > SGX535_BOOT_SCENE_VALIDATED)
        return SGX535_FROZEN_BAD_REQUEST;
    if (event == SGX535_BOOT_LOAD_KICKS) {
        if (value != 0x1fU)
            return SGX535_FROZEN_BAD_REQUEST;
    } else if (event == SGX535_BOOT_LOAD_STATUS2) {
        /* All four requested loads, including DHOST, must have completed. */
        if ((value & 15U) != 15U)
            return SGX535_FROZEN_BAD_REQUEST;
    } else if (event == SGX535_BOOT_INITEND_STATUS) {
        if (!(value & 0x00400000U))
            return SGX535_FROZEN_BAD_REQUEST;
    } else if (value != 0) {
        return SGX535_FROZEN_BAD_REQUEST;
    }
    boot->last_event = event;
    return SGX535_FROZEN_OK;
}

int sgx535_frozen_bootstrap_ready(const struct sgx535_frozen_bootstrap *boot)
{
    return boot && boot->raw_revision == 0x00010201U &&
        boot->last_event == SGX535_BOOT_SCENE_VALIDATED;
}
