#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""General retained identity plumbing for the existing private backend/source.
Called only after the original stager's pinned source checks and private guards.
No registration, admission, MMIO operation or changes to the frozen inputs.
"""
from stage_scene_service import once

def backend_sources(fixed,header):
    header=once(header,'    bool prefire_load_active; /* fixed_irq_lock, not last_stage */','    bool prefire_load_active; /* fixed_irq_lock, not last_stage */\n    struct remesa_gem_scene_job *general_scene;')
    header=once(header,'\n#endif\n','''
#ifdef REMESA_GENERAL_SCENE
struct remesa_gem_scene_job;
int remesa_scene_backend_begin(struct sgx535_gma500_fixed_backend *,struct remesa_gem_scene_job *);
extern const struct sgx535_fixed_ops remesa_scene_backend_ops;
#endif
#endif
''')
    fixed=once(fixed,'#define SGX535_FIXED_POLL_READS','#include "remesa_scene_backend.h"\n\n#define SGX535_FIXED_POLL_READS')
    # Shared IRQ/poll/read/write paths select only the real device/session.
    fixed=fixed.replace('backend->owner->dev','remesa_backend_device(backend)')
    fixed=once(fixed,'backend->owner->session.sequence','remesa_backend_session(backend)->sequence')
    fixed=once(fixed,'!sgx535_fixed_service_may_release(&backend->owner->fixed_service)','!remesa_backend_may_release(backend)')
    fixed=once(fixed,'#include "remesa_fixed_private.inc"','#include "remesa_fixed_private.inc"\n#include "remesa_scene_backend.inc"')
    # Old entry never accepts a backend already associated with a general job.
    fixed=once(fixed,'if (!backend || backend->active || !owner || !owner->initialized ||','if (!backend || backend->active || backend->general_scene || !owner || !owner->initialized ||')
    header=once(header,'#ifdef REMESA_GENERAL_SCENE\nstruct remesa_gem_scene_job;','''#ifdef REMESA_GENERAL_SCENE
struct remesa_scene_service;
void remesa_route_scene_service(struct sgx535_gma500_fixed_backend *,struct remesa_scene_service *,int);
void remesa_route_scene_before(struct sgx535_gma500_fixed_backend *,struct remesa_scene_service *,u32,u32,u32,int);
void remesa_route_scene_after(struct sgx535_gma500_fixed_backend *,struct remesa_scene_service *);
void remesa_route_scene_terminal(struct sgx535_gma500_fixed_backend *,struct remesa_scene_service *,int);
struct remesa_gem_scene_job;''')
    return fixed,header

def source_code(source):
    source=once(source,'static DEFINE_SPINLOCK(source_lock);','#include "remesa_scene_backend.h"\nstatic DEFINE_SPINLOCK(source_lock);')
    source=once(source,' sgx535_capsule_admit(&operation->evidence,current,b,&o->fixed_service,o,\n o->session.phase,o->session.observed_events,o->session.sequence,',' sgx535_capsule_admit(&operation->evidence,current,b,remesa_backend_service(b),remesa_backend_owner(b),\n remesa_backend_session(b)->phase,remesa_backend_session(b)->observed_events,remesa_backend_session(b)->sequence,')
    source=once(source,'else {struct sgx535_gma500_owner *o=b->owner;\n event(0,0,0,0,o->session.observed_events,o->session.phase);','else {\n event(0,0,0,0,remesa_backend_session(b)->observed_events,remesa_backend_session(b)->phase);')
    source=once(source,'b->owner->session.observed_events,b->owner->session.phase','remesa_backend_session(b)->observed_events,remesa_backend_session(b)->phase')
    source=once(source,'c->owner!=o->backend->owner','c->owner!=remesa_backend_owner(o->backend)')
    source=once(source,'o->backend->owner->session.phase!=SGX535_PHASE_RETIRED','remesa_backend_session(o->backend)->phase!=SGX535_PHASE_RETIRED')
    source=once(source,'o->backend->owner->session.observed_events!=7','remesa_backend_session(o->backend)->observed_events!=7')
    # Legacy prefix COLOR evidence remains specific to its old owner contract.
    source=once(source,'  sgx535_capsule_color(&o->evidence,o->backend->owner,','''#ifdef REMESA_GENERAL_SCENE
  if(o->backend->general_scene){lost();spin_unlock_irqrestore(&source_lock,f);return -EINVAL;}
#endif
  sgx535_capsule_color(&o->evidence,o->backend->owner,''')
    # Concrete ordinary clear predecessor; old first-owner claim/producer
    # implementations remain unchanged. The ordinary full-clear owner is the
    # only endpoint that invokes the new internal claim.
    source=once(source,'static bool valid(struct sgx535_gma500_fixed_backend *b)',
        'static void lost(void);\n#include "remesa_owned_fill_source.inc"\nstatic bool valid(struct sgx535_gma500_fixed_backend *b)')
    source=once(source,'!lifetime.reasons && !lifetime.producers_active;',
        '!source_lineage()->reasons && !source_lineage()->producers_active && source_available();')
    source=once(source,'if(!lease || lease!=who || operation || poisoned)e=-EBUSY;',
        'if(!lease || lease!=who || operation || poisoned || !source_available())e=-EBUSY;')
    source=once(source,' else lease=NULL;',' else {lease=NULL;source_released();}')
    start=source.index('int remesa_stream_source_bind(')
    end=source.index('int remesa_stream_source_finish(',start)
    bind=once(source[start:end],'!sgx535_source_boot_valid(&lifetime) || lifetime.producers_active',
        '!sgx535_source_boot_valid(source_lineage()) || lifetime.producers_active || !source_available()')
    bind=once(bind,'o->boundary=lifetime;','o->boundary=*source_lineage();')
    source=source[:start]+bind+source[end:]
    source=once(source,' if(dev!=device)lost();\n if(!operation || operation->boundary.state!=SGX535_SOURCE_BOUNDARY)',
        ' if(dev!=device)lost();\n owned_fill_pending(dev,a,d);\n if(!operation || operation->boundary.state!=SGX535_SOURCE_BOUNDARY)')
    source=once(source,'!sgx535_source_boot_valid(&lifetime) || lifetime.producers_active)e=-EIO;',
        '!sgx535_source_boot_valid(source_lineage()) || lifetime.producers_active || !source_available())e=-EIO;')
    return source+'\n#include "remesa_scene_source.inc"\n'
