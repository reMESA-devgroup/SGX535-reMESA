/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef REMESA_STREAM_CONTRACT_API_H
#define REMESA_STREAM_CONTRACT_API_H
#include "frozen_kernel_contract.h"
#include "frozen_fixed_service.h"
#include "remesa_borrowed_owner.h"
int remesa_stream_sgx535_frozen_summarize_color(const sgx535_u8 *bytes, size_t length,
                                   struct sgx535_frozen_color_summary *out);
int remesa_stream_sgx535_frozen_extract_commands(
    const struct sgx535_frozen_cpu_view *control,
    struct sgx535_frozen_command_pairs *out);
int remesa_stream_sgx535_frozen_scene_info32(struct sgx535_frozen_scene_info *out);
int remesa_stream_sgx535_frozen_xhw_bind_fire_wire(
    const struct sgx535_frozen_bo *bos, size_t count, sgx535_u64 mmu_end,
    sgx535_u32 engine, sgx535_u32 hw_context,
    struct sgx535_frozen_xhw_bind_fire_wire *out);
int remesa_stream_sgx535_frozen_ta_cookie(const struct sgx535_frozen_bo *bos,
                            size_t count, sgx535_u64 mmu_end,
                            struct sgx535_frozen_ta_cookie *out);
int remesa_stream_sgx535_frozen_rev121_init_writes(
    sgx535_u32 raw_revision, struct sgx535_frozen_reg_write *out,
    size_t count);
int remesa_stream_sgx535_frozen_ta_load_plan(const struct sgx535_frozen_bo *bos,
                               size_t count, sgx535_u64 mmu_end,
                               struct sgx535_frozen_reg_action *out,
                               size_t action_count);
int remesa_stream_sgx535_frozen_ta_fire_plan(const struct sgx535_frozen_bo *bos,
                               size_t count, sgx535_u64 mmu_end,
                               struct sgx535_frozen_reg_action *out,
                               size_t action_count);
int remesa_stream_sgx535_frozen_raster_fire_plan(const struct sgx535_frozen_bo *bos,
                                   size_t count, sgx535_u64 mmu_end,
                                   struct sgx535_frozen_reg_action *out,
                                   size_t action_count);
int remesa_stream_sgx535_frozen_raster_schedule_plan(
    const struct sgx535_frozen_command_pairs *commands,
    struct sgx535_frozen_reg_action *out, size_t action_count);
int remesa_stream_sgx535_frozen_ta_schedule_plan(
    const struct sgx535_frozen_command_pairs *commands,
    struct sgx535_frozen_reg_action *out, size_t action_count);
int remesa_stream_sgx535_frozen_initialize_user_images(
    struct sgx535_frozen_cpu_view *views);
int remesa_stream_sgx535_frozen_validate_request(const void *request, size_t length);
int remesa_stream_sgx535_frozen_get_requirement(sgx535_u32 role,
                                  struct sgx535_frozen_requirement *out);
int remesa_stream_sgx535_frozen_validate_bos(const struct sgx535_frozen_bo *bos,
                                size_t count, sgx535_u64 mmu_end);
int remesa_stream_sgx535_frozen_validate_publication_pages(
    const struct sgx535_frozen_bo *bos, size_t count, sgx535_u64 mmu_end,
    const sgx535_u32 page_counts[SGX535_BO_COUNT],
    const sgx535_u32 mapped_pages[SGX535_BO_COUNT]);
int remesa_stream_sgx535_frozen_plan_use_bases(const struct sgx535_frozen_bo *bos,
                                 size_t count, sgx535_u64 mmu_end,
                                 struct sgx535_frozen_use_plan *plan);
int remesa_stream_sgx535_frozen_validate_relocations(const sgx535_u32 *words,
                                        size_t count);
int remesa_stream_sgx535_frozen_validate_register_offsets(const sgx535_u32 *ta_offsets,
                                              size_t ta_count,
                                              const sgx535_u32 *raster_offsets,
                                              size_t raster_count);
int remesa_stream_sgx535_frozen_apply_relocations(const struct sgx535_frozen_bo *bos,
                                     size_t count, sgx535_u64 mmu_end,
                                     struct sgx535_frozen_cpu_view *views,
                                     const sgx535_u32 use_registers[2]);
int remesa_stream_sgx535_frozen_session_begin(struct sgx535_frozen_session *session,
                                const void *request, size_t request_length,
                                const struct sgx535_frozen_bo *bos,
                                size_t count, sgx535_u64 mmu_end);
int remesa_stream_sgx535_frozen_session_advance(struct sgx535_frozen_session *session,
                                  enum sgx535_frozen_phase next);
int remesa_stream_sgx535_frozen_session_service_ready(
    struct sgx535_frozen_session *session,
    const struct sgx535_frozen_bootstrap *boot, int use_owned);
int remesa_stream_sgx535_frozen_session_abort(struct sgx535_frozen_session *session);
int remesa_stream_sgx535_frozen_session_enter_fire(struct sgx535_frozen_session *session,
                                     sgx535_u32 sequence,
                                     sgx535_u32 timeout_ticks);
int remesa_stream_sgx535_frozen_session_begin_raster(struct sgx535_frozen_session *session,
                                       sgx535_u32 sequence);
int remesa_stream_sgx535_frozen_session_observe(struct sgx535_frozen_session *session,
                                  sgx535_u32 sequence,
                                  enum sgx535_frozen_event event);
int remesa_stream_sgx535_frozen_session_observe_status(
    struct sgx535_frozen_session *session, sgx535_u32 sequence,
    sgx535_u32 status1, sgx535_u32 status2, int exclusive_owned);
int remesa_stream_sgx535_frozen_session_timeout(struct sgx535_frozen_session *session);
int remesa_stream_sgx535_frozen_session_retire(struct sgx535_frozen_session *session);
int remesa_stream_sgx535_frozen_bootstrap_begin(struct sgx535_frozen_bootstrap *boot,
                                  sgx535_u32 raw_revision);
int remesa_stream_sgx535_frozen_bootstrap_observe(struct sgx535_frozen_bootstrap *boot,
                                    enum sgx535_frozen_boot_event event,
                                    sgx535_u32 value);
int remesa_stream_sgx535_frozen_bootstrap_ready(const struct sgx535_frozen_bootstrap *boot);
int remesa_stream_sgx535_fixed_service_bind(struct sgx535_fixed_service *service,
                              struct sgx535_frozen_session *session,
                              const struct sgx535_frozen_bo *bos,
                              size_t count, sgx535_u64 mmu_end,
                              const struct sgx535_frozen_command_pairs *commands);
int remesa_stream_sgx535_fixed_service_prepare(struct sgx535_fixed_service *service,
                                  const struct sgx535_fixed_ops *ops,
                                  void *context, sgx535_u32 raw_revision);
int remesa_stream_sgx535_fixed_service_fire_ta(struct sgx535_fixed_service *service,
                                  const struct sgx535_fixed_ops *ops,
                                  void *context, sgx535_u32 sequence,
                                  sgx535_u32 timeout_ticks);
int remesa_stream_sgx535_fixed_service_status(struct sgx535_fixed_service *service,
                                 const struct sgx535_fixed_ops *ops,
                                 void *context, sgx535_u32 sequence,
                                 sgx535_u32 status1, sgx535_u32 status2,
                                 int exclusive_owned);
int remesa_stream_sgx535_fixed_service_timeout(struct sgx535_fixed_service *service);
int remesa_stream_sgx535_fixed_service_retire(struct sgx535_fixed_service *service,
                                 const struct sgx535_fixed_ops *ops,
                                 void *context);
int remesa_stream_sgx535_fixed_service_may_release(
    const struct sgx535_fixed_service *service);
int remesa_stream_sgx535_fixed_service_run_once(
    struct sgx535_fixed_service *service,
    const struct sgx535_fixed_ops *ops,
    const struct sgx535_fixed_status_source *source,
    void *context, sgx535_u32 raw_revision,
    sgx535_u32 sequence, sgx535_u32 timeout_ticks,
    sgx535_u32 max_samples);
int remesa_stream_remesa_borrowed_owner_init(struct remesa_borrowed_owner *,
 struct sgx535_gma500_owner *,struct drm_device *,struct remesa_memory_ticket,
 const struct remesa_memory_view *,unsigned,u64,
 const struct sgx535_frozen_command_pairs *,remesa_borrowed_verify,void *);
int remesa_stream_remesa_borrowed_owner_deinit(struct remesa_borrowed_owner *);
int remesa_stream_owner_flush(struct sgx535_gma500_owner *);
#endif
