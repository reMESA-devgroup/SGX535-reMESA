#ifndef SGX535_FROZEN_KERNEL_CONTRACT_H
#define SGX535_FROZEN_KERNEL_CONTRACT_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u32 sgx535_u32;
typedef u64 sgx535_u64;
typedef u8 sgx535_u8;
#else
#include <stddef.h>
#include <stdint.h>
typedef uint32_t sgx535_u32;
typedef uint64_t sgx535_u64;
typedef uint8_t sgx535_u8;
#endif

#if defined(REMESA_LAB_DEPTH1280_PROFILE) && REMESA_LAB_DEPTH1280_PROFILE != 1
#error "depth1280 LAB profile must be deliberately selected as1"
#endif

/* Offline port core. This header does not install a DRM ioctl. */
enum sgx535_frozen_role {
    SGX535_BO_PDS,
    SGX535_BO_USE,
    SGX535_BO_VERTEX_TA,
    SGX535_BO_BACKGROUND,
    SGX535_BO_CONTROL,
    SGX535_BO_COLOR,
    SGX535_BO_SCENE_HW,
    SGX535_BO_TA_PAGE_TABLE,
    SGX535_BO_TA_PARAMETER,
    SGX535_BO_XHW_COMM,
#ifdef REMESA_LAB_DEPTH1280_PROFILE
    SGX535_BO_Z, /* appended: existing role indices0..9 remain unchanged */
#endif
    SGX535_BO_COUNT
};

enum sgx535_frozen_domain {
    SGX535_DOMAIN_LOCAL,
    SGX535_DOMAIN_PDS,
    SGX535_DOMAIN_RASTGEOM,
    SGX535_DOMAIN_MMU
};

enum sgx535_frozen_error {
    SGX535_FROZEN_OK = 0,
    SGX535_FROZEN_BAD_REQUEST = -1,
    SGX535_FROZEN_BAD_COUNT = -2,
    SGX535_FROZEN_BAD_ROLE = -3,
    SGX535_FROZEN_BAD_SIZE = -4,
    SGX535_FROZEN_BAD_DOMAIN = -5,
    SGX535_FROZEN_BAD_ADDRESS = -6,
    SGX535_FROZEN_BAD_ALIGNMENT = -7,
    SGX535_FROZEN_ALIAS = -8,
    SGX535_FROZEN_BAD_OWNER = -9,
    SGX535_FROZEN_BAD_MMU_END = -10,
    SGX535_FROZEN_BAD_RELOCATION = -11,
    SGX535_FROZEN_BAD_REGISTER_LIST = -12
};

/* The only proposed userspace request. No pointers or variable-length tail. */
struct sgx535_frozen_request {
    sgx535_u32 abi_version;
    sgx535_u32 operation;
    sgx535_u32 flags;
    sgx535_u32 reserved;
};

/* Kernel-internal descriptors; never copied directly from userspace. */
struct sgx535_frozen_bo {
    sgx535_u32 role;
    sgx535_u32 domain;
    sgx535_u64 size;
    sgx535_u64 gpu_va;
    sgx535_u64 owner_token;
};

struct sgx535_frozen_requirement {
    sgx535_u64 size;
    sgx535_u32 alignment;
    sgx535_u32 domain;
};

/* Internal mapped CPU views. LOCAL objects have no GPU VA but may have bytes. */
struct sgx535_frozen_cpu_view {
    sgx535_u8 *bytes;
    sgx535_u64 length;
};

/* Diagnostic summary of the fixed 32x32 linear ARGB8888 color BO. This is
 * not proof that a GPU-to-CPU readback is coherent on the target. */
struct sgx535_frozen_color_summary {
    sgx535_u32 fnv1a;
    sgx535_u32 nonzero_pixels;
    sgx535_u32 row_nonzero[32];
};

int sgx535_frozen_summarize_color(const sgx535_u8 *bytes, size_t length,
                                   struct sgx535_frozen_color_summary *out);

/* Immutable kernel-owned copy of the selected register pairs. The CONTROL
 * CPU mapping may be released after this copy and final publication. */
struct sgx535_frozen_command_pairs {
    sgx535_u32 ta[14];
    sgx535_u32 raster[52];
};

int sgx535_frozen_extract_commands(
    const struct sgx535_frozen_cpu_view *control,
    struct sgx535_frozen_command_pairs *out);

/* Candidate historical psb_regman.c allocation order for this fixed scene.
 * These are planned register writes, never a claim that hardware was set. */
struct sgx535_frozen_use_entry {
    sgx535_u32 reg;
    sgx535_u32 base;
    sgx535_u32 register_offset;
    sgx535_u32 register_word;
};

struct sgx535_frozen_use_plan {
    struct sgx535_frozen_use_entry by_data_master[2];
    sgx535_u32 assigned_count;
};

/* Xpsb's selected 32x32 CPU reply. Cookie word 15 is clean-room zero;
 * the retained producer did not write it and the initial normal fire path
 * does not consume it. This is not an XHW device-ready claim. */
struct sgx535_frozen_scene_info {
    sgx535_u32 width;
    sgx535_u32 height;
    sgx535_u32 cookie[16];
    sgx535_u32 bo_size;
    sgx535_u32 clear_page_start;
    sgx535_u32 clear_page_count;
};

int sgx535_frozen_scene_info32(struct sgx535_frozen_scene_info *out);

/* CPU-only selected 132-byte PSB_XHW_SCENE_BIND_FIRE argument image.
 * This does not reserve context 0 or perform any XHW work. */
struct sgx535_frozen_xhw_bind_fire_wire {
    sgx535_u32 words[33];
};

int sgx535_frozen_xhw_bind_fire_wire(
    const struct sgx535_frozen_bo *bos, size_t count, sgx535_u64 mmu_end,
    sgx535_u32 engine, sgx535_u32 hw_context,
    struct sgx535_frozen_xhw_bind_fire_wire *out);

/* Selected 0x1f TA-memory-load cookie, assembled from kernel-owned VAs.
 * The info reply contributes words 2..12 and size 0x620000; the selected
 * load step fills words 0, 1, 10..12. This contains no TA table contents. */
struct sgx535_frozen_ta_cookie {
    sgx535_u32 words[13];
    sgx535_u32 info_bo_size;
};

int sgx535_frozen_ta_cookie(const struct sgx535_frozen_bo *bos,
                            size_t count, sgx535_u64 mmu_end,
                            struct sgx535_frozen_ta_cookie *out);

/* Exact selected Xpsb 0x3820 CPU write order, for offline review only.
 * No register is accessed by this function. Unknown register semantics and
 * target operating conditions remain a separate Gate B requirement. */
struct sgx535_frozen_reg_write {
    sgx535_u32 offset;
    sgx535_u32 value;
};

int sgx535_frozen_rev121_init_writes(
    sgx535_u32 raw_revision, struct sgx535_frozen_reg_write *out,
    size_t count);

/* Data-only transcription of the selected 0x1f TA load path. A future
 * executor must use bounded polls and fail closed; this plan is not a
 * hardware authorization or a general-purpose MMIO interface. */
enum sgx535_frozen_reg_action_kind {
    SGX535_REG_WRITE = 1,
    SGX535_REG_POLL_SET,
    SGX535_REG_POLL_CLEAR,
    SGX535_REG_WMB
};

struct sgx535_frozen_reg_action {
    sgx535_u32 kind;
    sgx535_u32 offset;
    sgx535_u32 value;
    sgx535_u32 mask;
};

int sgx535_frozen_ta_load_plan(const struct sgx535_frozen_bo *bos,
                               size_t count, sgx535_u64 mmu_end,
                               struct sgx535_frozen_reg_action *out,
                               size_t action_count);

/* Fresh scene, hardware context 0, normal TA flags, rev121 zero-option
 * branch. This is a data-only historical write/wait plan. */
int sgx535_frozen_ta_fire_plan(const struct sgx535_frozen_bo *bos,
                               size_t count, sgx535_u64 mmu_end,
                               struct sgx535_frozen_reg_action *out,
                               size_t action_count);
int sgx535_frozen_raster_fire_plan(const struct sgx535_frozen_bo *bos,
                                   size_t count, sgx535_u64 mmu_end,
                                   struct sgx535_frozen_reg_action *out,
                                   size_t action_count);

/* Historical psb_schedule_raster ordering for one scene, through
 * psb_reg_submit's trailing wmb. Data only; does not authorize MMIO. */
int sgx535_frozen_raster_schedule_plan(
    const struct sgx535_frozen_command_pairs *commands,
    struct sgx535_frozen_reg_action *out, size_t action_count);
int sgx535_frozen_ta_schedule_plan(
    const struct sgx535_frozen_command_pairs *commands,
    struct sgx535_frozen_reg_action *out, size_t action_count);

/* Deterministic pre-relocation CPU image for the six user BO roles. */
int sgx535_frozen_initialize_user_images(
    struct sgx535_frozen_cpu_view *views);

/* Kernel-internal bookkeeping. Events are claims by a future backend, not
 * hardware evidence produced by this offline core. */
enum sgx535_frozen_phase {
    SGX535_PHASE_EMPTY,
    SGX535_PHASE_BOS_VALIDATED,
    SGX535_PHASE_HOST_IMAGE_FINAL,
    SGX535_PHASE_CPU_PUBLISHED,
    SGX535_PHASE_TRANSLATIONS_PUBLISHED,
    SGX535_PHASE_DEVICE_MAINTAINED,
    SGX535_PHASE_SERVICE_READY,
    SGX535_PHASE_FIRE_POSSIBLE,
    SGX535_PHASE_SCENE_COMPLETED,
    SGX535_PHASE_RETIRED,
    SGX535_PHASE_ABORTED_BEFORE_SUBMIT,
    SGX535_PHASE_HELD_AFTER_FAILURE
};

struct sgx535_frozen_session {
    enum sgx535_frozen_phase phase;
    sgx535_u32 sequence;
    sgx535_u32 timeout_ticks;
    sgx535_u32 observed_events;
    sgx535_u32 ta_memory_free_seen;
    sgx535_u32 raster_started;
};

/* The *historical driver* continuation predicate for the one rev121 scene.
 * These events must be supplied by an actual, ordered internal service; this
 * ledger does not perform MMIO or establish architectural device readiness. */
enum sgx535_frozen_boot_event {
    SGX535_BOOT_INIT_WRITES_RETURN = 1,
    SGX535_BOOT_XHW_INIT_REPLY,
    SGX535_BOOT_TA_INFO_REPLY,
    SGX535_BOOT_SCENE_INFO_REPLY,
    SGX535_BOOT_LOAD_KICKS,
    SGX535_BOOT_LOAD_STATUS2,
    SGX535_BOOT_INITEND_STATUS,
    SGX535_BOOT_TA_LOAD_REPLY,
    SGX535_BOOT_SCENE_VALIDATED
};

struct sgx535_frozen_bootstrap {
    sgx535_u32 raw_revision;
    sgx535_u32 last_event;
};

/* Events must be attributed by a future exclusive SGX service backend to
 * this one TA task. The model does not itself read an IRQ or schedule a timer. */
enum sgx535_frozen_event {
    SGX535_EVENT_TA_FINISHED = 1,
    SGX535_EVENT_PIXELBE_END_RENDER = 2,
    SGX535_EVENT_DPM_3D_MEM_FREE = 3,
    SGX535_EVENT_SGX_MMU_FAULT = 4
};

int sgx535_frozen_validate_request(const void *request, size_t length);
int sgx535_frozen_get_requirement(sgx535_u32 role,
                                  struct sgx535_frozen_requirement *out);
int sgx535_frozen_validate_bos(const struct sgx535_frozen_bo *bos,
                                size_t count, sgx535_u64 mmu_end);
/* Require complete SGX mappings for non-local BOs and none for LOCAL BOs.
 * This checks bookkeeping only; it does not prove a live PTE or visibility. */
int sgx535_frozen_validate_publication_pages(
    const struct sgx535_frozen_bo *bos, size_t count, sgx535_u64 mmu_end,
    const sgx535_u32 page_counts[SGX535_BO_COUNT],
    const sgx535_u32 mapped_pages[SGX535_BO_COUNT]);
int sgx535_frozen_plan_use_bases(const struct sgx535_frozen_bo *bos,
                                 size_t count, sgx535_u64 mmu_end,
                                 struct sgx535_frozen_use_plan *plan);
/* Internal canonical wire records: count records of ten 32-bit words. */
int sgx535_frozen_validate_relocations(const sgx535_u32 *words,
                                        size_t count);
int sgx535_frozen_validate_register_offsets(const sgx535_u32 *ta_offsets,
                                              size_t ta_count,
                                              const sgx535_u32 *raster_offsets,
                                              size_t raster_count);
/* Synthetic/offline deterministic patcher. A future kernel owner must supply
 * already validated and exclusively owned CPU views and GPU VAs. */
int sgx535_frozen_apply_relocations(const struct sgx535_frozen_bo *bos,
                                     size_t count, sgx535_u64 mmu_end,
                                     struct sgx535_frozen_cpu_view *views,
                                     const sgx535_u32 use_registers[2]);
int sgx535_frozen_session_begin(struct sgx535_frozen_session *session,
                                const void *request, size_t request_length,
                                const struct sgx535_frozen_bo *bos,
                                size_t count, sgx535_u64 mmu_end);
int sgx535_frozen_session_advance(struct sgx535_frozen_session *session,
                                  enum sgx535_frozen_phase next);
/* USE ownership is an internal provider claim, not a user flag. */
int sgx535_frozen_session_service_ready(
    struct sgx535_frozen_session *session,
    const struct sgx535_frozen_bootstrap *boot, int use_owned);
int sgx535_frozen_session_abort(struct sgx535_frozen_session *session);
/* Call under the exclusive service lock, with a bounded timer already armed,
 * immediately BEFORE the first operation that could enqueue/fire work. Any
 * error after this point is uncertain and must retain every scene resource. */
int sgx535_frozen_session_enter_fire(struct sgx535_frozen_session *session,
                                     sgx535_u32 sequence,
                                     sgx535_u32 timeout_ticks);
/* Claim the sole raster stage after the attributed TA-finished event, before
 * any reset/register/XHW action. Any subsequent failure retains resources. */
int sgx535_frozen_session_begin_raster(struct sgx535_frozen_session *session,
                                       sgx535_u32 sequence);
int sgx535_frozen_session_observe(struct sgx535_frozen_session *session,
                                  sgx535_u32 sequence,
                                  enum sgx535_frozen_event event);
/* Decode only the selected SGX event bits under an exclusive service claim.
 * The caller must obtain real status from an attributed target IRQ; passing
 * a raw status word alone does not prove ownership or completion. */
int sgx535_frozen_session_observe_status(
    struct sgx535_frozen_session *session, sgx535_u32 sequence,
    sgx535_u32 status1, sgx535_u32 status2, int exclusive_owned);
int sgx535_frozen_session_timeout(struct sgx535_frozen_session *session);
int sgx535_frozen_session_retire(struct sgx535_frozen_session *session);
int sgx535_frozen_bootstrap_begin(struct sgx535_frozen_bootstrap *boot,
                                  sgx535_u32 raw_revision);
int sgx535_frozen_bootstrap_observe(struct sgx535_frozen_bootstrap *boot,
                                    enum sgx535_frozen_boot_event event,
                                    sgx535_u32 value);
int sgx535_frozen_bootstrap_ready(const struct sgx535_frozen_bootstrap *boot);

#endif
