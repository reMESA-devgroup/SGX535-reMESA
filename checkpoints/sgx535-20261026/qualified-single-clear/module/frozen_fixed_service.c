#include "frozen_fixed_service.h"

static int step(struct sgx535_fixed_service *service,
                const struct sgx535_fixed_ops *ops, void *context,
                enum sgx535_fixed_stage stage, const void *payload,
                size_t count, struct sgx535_fixed_observation *observation)
{
    struct sgx535_fixed_diagnostic *diagnostic = &service->diagnostic;
    int ret;

    diagnostic->stage_reached = stage;
    if (!ops || !ops->run)
        ret = SGX535_FROZEN_BAD_REQUEST;
    else
        ret = ops->run(context, stage, payload, count, observation);
    diagnostic->raw_result = ret;
    if (observation && (stage == SGX535_FIXED_TA_LOAD ||
                       stage == SGX535_FIXED_STATUS_BASELINE)) {
        diagnostic->observation_stage = stage;
        diagnostic->observation_load_flags = observation->load_flags;
        diagnostic->observation_status1 = observation->status1;
        diagnostic->observation_status2 = observation->status2;
        diagnostic->observation_initend = observation->initend;
    }
    if (ret) {
        diagnostic->failure_stage = stage;
        diagnostic->failure_source = SGX535_FIXED_FAILURE_BACKEND_CALLBACK;
    }
    return ret;
}

static int fail(struct sgx535_fixed_service *service)
{
    if (service->diagnostic.failure_source == SGX535_FIXED_FAILURE_NONE) {
        service->diagnostic.failure_stage =
            service->diagnostic.stage_reached;
        service->diagnostic.failure_source = SGX535_FIXED_FAILURE_SERVICE_CHECK;
        service->diagnostic.raw_result = SGX535_FROZEN_BAD_REQUEST;
    }
    if (service->unsafe_possible ||
        service->session->phase >= SGX535_PHASE_FIRE_POSSIBLE)
        service->session->phase = SGX535_PHASE_HELD_AFTER_FAILURE;
    else
        (void)sgx535_frozen_session_abort(service->session);
    return SGX535_FROZEN_BAD_REQUEST;
}

int sgx535_fixed_service_bind(struct sgx535_fixed_service *service,
                              struct sgx535_frozen_session *session,
                              const struct sgx535_frozen_bo *bos,
                              size_t count, sgx535_u64 mmu_end,
                              const struct sgx535_frozen_command_pairs *commands)
{
    struct sgx535_frozen_reg_action ta[8], raster[29];
    struct sgx535_fixed_service candidate = {0};
    size_t i;

    if (!service || service->session || !session ||
        session->phase != SGX535_PHASE_HOST_IMAGE_FINAL || !commands ||
        sgx535_frozen_validate_bos(bos, count, mmu_end) ||
        sgx535_frozen_ta_schedule_plan(commands, ta, 8) ||
        sgx535_frozen_raster_schedule_plan(commands, raster, 29))
        return SGX535_FROZEN_BAD_REQUEST;
    for (i = 0; i < count; i++)
        candidate.bos[i] = bos[i];
    candidate.commands = *commands;
    candidate.session = session;
    candidate.mmu_end = mmu_end;
    if (sgx535_frozen_plan_use_bases(candidate.bos, count, mmu_end,
                                     &candidate.use_plan) ||
        sgx535_frozen_xhw_bind_fire_wire(candidate.bos, count, mmu_end,
                                         0, 0, &candidate.ta_wire) ||
        sgx535_frozen_xhw_bind_fire_wire(candidate.bos, count, mmu_end,
                                         1, 0, &candidate.raster_wire))
        return SGX535_FROZEN_BAD_REQUEST;
    *service = candidate;
    return SGX535_FROZEN_OK;
}

int sgx535_fixed_service_prepare(struct sgx535_fixed_service *service,
                                  const struct sgx535_fixed_ops *ops,
                                  void *context, sgx535_u32 raw_revision)
{
    struct sgx535_frozen_reg_write init[12];
    struct sgx535_frozen_reg_action load[29];
    struct sgx535_frozen_scene_info scene_info;
    struct sgx535_frozen_ta_cookie ta_info;
    struct sgx535_fixed_observation observation = {0};
    struct sgx535_frozen_session *session;

    if (!service || !service->session || service->started ||
        !ops || !ops->run || raw_revision != 0x00010201U)
        return SGX535_FROZEN_BAD_REQUEST;
    session = service->session;
    if (session->phase != SGX535_PHASE_HOST_IMAGE_FINAL)
        return SGX535_FROZEN_BAD_REQUEST;
    service->started = 1;
    if (step(service, ops, context, SGX535_FIXED_PUBLISH_CPU, 0, 0, &observation) ||
        sgx535_frozen_session_advance(session, SGX535_PHASE_CPU_PUBLISHED) ||
        step(service, ops, context, SGX535_FIXED_PUBLISH_TRANSLATIONS,
             0, 0, &observation) ||
        sgx535_frozen_session_advance(session,
                                      SGX535_PHASE_TRANSLATIONS_PUBLISHED))
        return fail(service);
    /* Device maintenance can affect retained translations. Hold the owner
     * after this point if any later stage fails or its result is ambiguous. */
    service->unsafe_possible = 1;
    if (step(service, ops, context, SGX535_FIXED_DEVICE_MAINTAIN,
             0, 0, &observation) ||
        sgx535_frozen_session_advance(session, SGX535_PHASE_DEVICE_MAINTAINED) ||
        sgx535_frozen_bootstrap_begin(&service->boot, raw_revision) ||
        sgx535_frozen_rev121_init_writes(raw_revision, init, 12) ||
        step(service, ops, context, SGX535_FIXED_INIT_WRITES,
             init, 12, &observation) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_INIT_WRITES_RETURN, 0) ||
        step(service, ops, context, SGX535_FIXED_XHW_INIT,
             0, 0, &observation) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_XHW_INIT_REPLY, 0) ||
        sgx535_frozen_ta_cookie(service->bos, SGX535_BO_COUNT,
                                 service->mmu_end, &ta_info) ||
        step(service, ops, context, SGX535_FIXED_TA_INFO,
             &ta_info, 1, &observation) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_TA_INFO_REPLY, 0) ||
        sgx535_frozen_scene_info32(&scene_info) ||
        step(service, ops, context, SGX535_FIXED_SCENE_INFO,
             &scene_info, 1, &observation) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_SCENE_INFO_REPLY, 0) ||
        sgx535_frozen_ta_load_plan(service->bos, SGX535_BO_COUNT,
                                    service->mmu_end, load, 29) ||
        step(service, ops, context, SGX535_FIXED_TA_LOAD,
             load, 29, &observation) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_LOAD_KICKS, observation.load_flags) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_LOAD_STATUS2, observation.status2) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_INITEND_STATUS, observation.initend) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_TA_LOAD_REPLY, 0) ||
        step(service, ops, context, SGX535_FIXED_SCENE_VALIDATE,
             0, 0, &observation) ||
        sgx535_frozen_bootstrap_observe(&service->boot,
             SGX535_BOOT_SCENE_VALIDATED, 0) ||
        step(service, ops, context, SGX535_FIXED_USE_RESERVE,
             &service->use_plan, 1, &observation))
        return fail(service);
    service->use_owned = 1;
    if (step(service, ops, context, SGX535_FIXED_USE_PROGRAM,
             &service->use_plan, 1, &observation))
        return fail(service);
    observation = (struct sgx535_fixed_observation){0};
    if (step(service, ops, context, SGX535_FIXED_STATUS_BASELINE,
             0, 0, &observation) ||
        (observation.status1 & ((1U << 28) | (1U << 25) |
             (1U << 24) | (1U << 18) | (1U << 13) |
             (1U << 12) | (1U << 3) | (1U << 2) |
             (1U << 1) | 1U)) ||
        /* No unconsumed load, fault or unknown STATUS2 event may precede TA. */
        observation.status2 ||
        sgx535_frozen_session_service_ready(session, &service->boot, 1))
        return fail(service);
    return SGX535_FROZEN_OK;
}

int sgx535_fixed_service_fire_ta(struct sgx535_fixed_service *service,
                                  const struct sgx535_fixed_ops *ops,
                                  void *context, sgx535_u32 sequence,
                                  sgx535_u32 timeout_ticks)
{
    struct sgx535_frozen_reg_action actions[8];
    struct sgx535_fixed_fire_payload fire = {0};
    struct sgx535_fixed_observation observation = {0};

    if (!service || !service->session || !service->use_owned ||
        !sgx535_frozen_bootstrap_ready(&service->boot) ||
        sgx535_frozen_ta_schedule_plan(&service->commands, actions, 8) ||
        sgx535_frozen_ta_fire_plan(service->bos, SGX535_BO_COUNT,
                                   service->mmu_end, fire.actions, 31) ||
        sgx535_frozen_session_enter_fire(service->session,
                                          sequence, timeout_ticks))
        return SGX535_FROZEN_BAD_REQUEST;
    fire.wire = service->ta_wire;
    fire.action_count = 31;
    if (step(service, ops, context, SGX535_FIXED_TA_SCHEDULE,
             actions, 8, &observation) ||
        step(service, ops, context, SGX535_FIXED_TA_FIRE,
             &fire, 1, &observation))
        return fail(service);
    return SGX535_FROZEN_OK;
}

static int fixed_service_status_impl(struct sgx535_fixed_service *service,
                                 const struct sgx535_fixed_ops *ops,
                                 void *context, sgx535_u32 sequence,
                                 sgx535_u32 status1, sgx535_u32 status2,
                                 int exclusive_owned)
{
    struct sgx535_frozen_reg_action actions[29];
    struct sgx535_fixed_fire_payload fire = {0};
    struct sgx535_fixed_observation observation = {0};
    int result;

    if (!service || !service->session)
        return SGX535_FROZEN_BAD_REQUEST;
    service->diagnostic.stage_reached = SGX535_FIXED_STATUS_PROCESS;
    service->diagnostic.observation_stage = SGX535_FIXED_STATUS_PROCESS;
    result = sgx535_frozen_session_observe_status(service->session,
             sequence, status1, status2, exclusive_owned);
    if (result) {
        service->diagnostic.failure_stage = SGX535_FIXED_STATUS_PROCESS;
        service->diagnostic.failure_source = SGX535_FIXED_FAILURE_SERVICE_CHECK;
        service->diagnostic.raw_result = result;
        service->diagnostic.observation_status1 = status1;
        service->diagnostic.observation_status2 = status2;
        if (service->session->phase == SGX535_PHASE_FIRE_POSSIBLE)
            (void)fail(service);
        return result;
    }
    if (service->session->phase == SGX535_PHASE_HELD_AFTER_FAILURE)
        return SGX535_FROZEN_BAD_REQUEST;
    if (status1 & (1U << 13)) {
        if (sgx535_frozen_session_begin_raster(service->session,
                                                sequence) ||
            sgx535_frozen_raster_schedule_plan(&service->commands,
                                                actions, 29) ||
            sgx535_frozen_raster_fire_plan(service->bos, SGX535_BO_COUNT,
                                           service->mmu_end, fire.actions, 20))
            return fail(service);
        fire.wire = service->raster_wire;
        fire.action_count = 20;
        /* Keep the historical ISP-bit bracket visible to failure injection.
         * An error after assertion leaves ownership held; no reset recovery
         * is inferred from a failed callback. */
        if (step(service, ops, context, SGX535_FIXED_ISP_RESET_ASSERT,
                 actions, 1, &observation) ||
            step(service, ops, context, SGX535_FIXED_ISP_RESET_CLEAR,
                 actions + 1, 1, &observation) ||
            step(service, ops, context, SGX535_FIXED_RASTER_SCHEDULE,
                 actions + 2, 27, &observation) ||
            step(service, ops, context, SGX535_FIXED_RASTER_FIRE,
                 &fire, 1, &observation))
            return fail(service);
    }
    return SGX535_FROZEN_OK;
}

int sgx535_fixed_service_status(struct sgx535_fixed_service *service,
    const struct sgx535_fixed_ops *ops, void *context, sgx535_u32 sequence,
    sgx535_u32 status1, sgx535_u32 status2, int exclusive_owned)
{
    int result;
    const struct sgx535_fixed_service_observer *tap = service ? service->observer : 0;
    if (tap && tap->before)
        tap->before(service->observer_context, service, sequence, status1, status2, exclusive_owned);
    result = fixed_service_status_impl(service, ops, context, sequence, status1, status2, exclusive_owned);
    if (tap && tap->after)
        tap->after(service->observer_context, service);
    return result;
}

int sgx535_fixed_service_timeout(struct sgx535_fixed_service *service)
{
    if (!service || !service->session)
        return SGX535_FROZEN_BAD_REQUEST;
    return sgx535_frozen_session_timeout(service->session);
}

int sgx535_fixed_service_retire(struct sgx535_fixed_service *service,
                                 const struct sgx535_fixed_ops *ops,
                                 void *context)
{
    struct sgx535_fixed_observation observation = {0};

    if (!service || !service->session || !service->use_owned ||
        service->session->phase != SGX535_PHASE_SCENE_COMPLETED)
        return SGX535_FROZEN_BAD_REQUEST;
    if (step(service, ops, context, SGX535_FIXED_USE_RELEASE,
             &service->use_plan, 1, &observation))
        return fail(service);
    service->use_owned = 0;
    return sgx535_frozen_session_retire(service->session);
}

int sgx535_fixed_service_may_release(
    const struct sgx535_fixed_service *service)
{
    enum sgx535_frozen_phase phase;

    if (!service || !service->session)
        return 0;
    phase = service->session->phase;
    if (phase == SGX535_PHASE_RETIRED)
        return 1;
    return !service->unsafe_possible &&
        (phase == SGX535_PHASE_HOST_IMAGE_FINAL ||
         phase == SGX535_PHASE_ABORTED_BEFORE_SUBMIT);
}

static int fixed_service_run_once_impl(
    struct sgx535_fixed_service *service,
    const struct sgx535_fixed_ops *ops,
    const struct sgx535_fixed_status_source *source,
    void *context, sgx535_u32 raw_revision,
    sgx535_u32 sequence, sgx535_u32 timeout_ticks,
    sgx535_u32 max_samples)
{
    sgx535_u32 i;

    if (!source || !source->sample_and_ack || !max_samples ||
        max_samples > 300000U || !sequence || !timeout_ticks)
        return SGX535_FROZEN_BAD_REQUEST;
    if (sgx535_fixed_service_prepare(service, ops, context, raw_revision) ||
        sgx535_fixed_service_fire_ta(service, ops, context,
                                      sequence, timeout_ticks))
        return SGX535_FROZEN_BAD_REQUEST;
    for (i = 0; i < max_samples; i++) {
        sgx535_u32 status1 = 0, status2 = 0;
        int exclusive_owned = 0;

        int sample_ret;

        service->diagnostic.stage_reached = SGX535_FIXED_STATUS_SAMPLE;
        sample_ret = source->sample_and_ack(context, sequence, &status1,
                                             &status2, &exclusive_owned);
        service->diagnostic.raw_result = sample_ret;
        service->diagnostic.observation_status1 = status1;
        service->diagnostic.observation_status2 = status2;
        service->diagnostic.observation_stage = SGX535_FIXED_STATUS_SAMPLE;
        if (sample_ret) {
            service->diagnostic.failure_stage = SGX535_FIXED_STATUS_SAMPLE;
            service->diagnostic.failure_source = SGX535_FIXED_FAILURE_STATUS_SOURCE;
            (void)fail(service);
            return SGX535_FROZEN_BAD_REQUEST;
        }
        if (!status1 && !status2)
            continue;
        if (sgx535_fixed_service_status(service, ops, context, sequence,
                                        status1, status2, exclusive_owned))
            return SGX535_FROZEN_BAD_REQUEST;
        if (service->session->phase == SGX535_PHASE_SCENE_COMPLETED)
            return sgx535_fixed_service_retire(service, ops, context);
    }
    service->diagnostic.stage_reached = SGX535_FIXED_TIMEOUT;
    service->diagnostic.failure_stage = SGX535_FIXED_TIMEOUT;
    service->diagnostic.failure_source = SGX535_FIXED_FAILURE_TIMEOUT;
    service->diagnostic.raw_result = SGX535_FROZEN_BAD_REQUEST;
    (void)sgx535_fixed_service_timeout(service);
    return SGX535_FROZEN_BAD_REQUEST;
}

int sgx535_fixed_service_run_once(
    struct sgx535_fixed_service *service,
    const struct sgx535_fixed_ops *ops,
    const struct sgx535_fixed_status_source *source, void *context,
    sgx535_u32 raw_revision, sgx535_u32 sequence, sgx535_u32 timeout_ticks,
    sgx535_u32 max_samples)
{
    const struct sgx535_fixed_service_observer *tap =
        service ? service->observer : 0;
    int result;

    if (tap && tap->begin)
        tap->begin(service->observer_context, service, ops, source, context);
    result = fixed_service_run_once_impl(service, ops, source, context,
        raw_revision, sequence, timeout_ticks, max_samples);
    if (tap && tap->terminal)
        tap->terminal(service->observer_context, service, result);
    return result;
}
