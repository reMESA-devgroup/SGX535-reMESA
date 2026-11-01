#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Reuse the qualified service order/lifecycle with owned general plans.
No MMIO, registration or installation. Frozen input and canonical service stay
unchanged. Every changed construction site is a checked unique replacement.
"""
import hashlib
from pathlib import Path

def once(s,a,b):
    if s.count(a)!=1:raise ValueError('scene service anchor missing/ambiguous: '+a[:80])
    return s.replace(a,b,1)

def scene_service_components(root,native):
    source=(native/'frozen_fixed_service.c').read_bytes()
    if hashlib.sha256(source).hexdigest()!='ac4acc665ccd46b27dbf61fc1ccf07ba56fba2153dc0ce3820f0e2361880dd2e':
        raise ValueError('unreviewed fixed service input')
    s=source.decode().replace('sgx535_fixed_service','remesa_scene_service').replace('sgx535_fixed_fire_payload','remesa_scene_fire_payload')
    s=once(s,'#include "frozen_fixed_service.h"','#include "remesa_scene_service.h"\n#ifdef __KERNEL__\n#include <linux/string.h>\n#include "remesa_stream_contract_api.h"\n#else\n#include <string.h>\n#endif')
    start=s.index('int remesa_scene_service_bind(');end=s.index('int remesa_scene_service_prepare(',start)
    s=s[:start]+'''int remesa_scene_service_bind(struct remesa_scene_service *service,
 struct sgx535_frozen_session *session,const struct remesa_scene_service_plan *plan)
{
 struct sgx535_frozen_reg_action ta[8],raster[29];unsigned i;
 if(!service||service->session||!session||session->phase!=SGX535_PHASE_HOST_IMAGE_FINAL||!plan||
    !plan->binding_count||plan->binding_count>32||plan->load_count!=29||plan->ta_count!=31||plan->raster_count!=20||
    !plan->use.mask||(plan->use.mask&~0xfff8u)||
    sgx535_frozen_ta_schedule_plan(&plan->commands,ta,8)||
    sgx535_frozen_raster_schedule_plan(&plan->commands,raster,29))return SGX535_FROZEN_BAD_REQUEST;
 for(i=3;i<16;i++)if((plan->use.mask&(1u<<i))&&plan->use.banks[i].offset!=0xa0cu+4*i)return SGX535_FROZEN_BAD_REQUEST;
 service->session=session;service->plan=plan;service->commands=plan->commands;service->use_plan=plan->use;
 return SGX535_FROZEN_OK;
}

'''+s[end:]
    s=once(s,'    struct sgx535_frozen_scene_info scene_info;\n    struct sgx535_frozen_ta_cookie ta_info;\n','')
    s=once(s,'''        sgx535_frozen_ta_cookie(service->bos, SGX535_BO_COUNT,
                                 service->mmu_end, &ta_info) ||
        step(service, ops, context, SGX535_FIXED_TA_INFO,
             &ta_info, 1, &observation) ||''','''        step(service, ops, context, SGX535_FIXED_TA_INFO,
             service->plan->load, 29, &observation) ||''')
    s=once(s,'''        sgx535_frozen_scene_info32(&scene_info) ||
        step(service, ops, context, SGX535_FIXED_SCENE_INFO,
             &scene_info, 1, &observation) ||''','''        step(service, ops, context, SGX535_FIXED_SCENE_INFO,
             service->plan, 1, &observation) ||''')
    s=once(s,'''        sgx535_frozen_ta_load_plan(service->bos, SGX535_BO_COUNT,
                                    service->mmu_end, load, 29) ||''','''        (memcpy(load, service->plan->load, sizeof(load)), 0) ||''')
    s=once(s,'''        sgx535_frozen_ta_fire_plan(service->bos, SGX535_BO_COUNT,
                                   service->mmu_end, fire.actions, 31) ||''','''        (memcpy(fire.actions, service->plan->ta, sizeof(service->plan->ta)), 0) ||''')
    s=once(s,'    fire.wire = service->ta_wire;\n','')
    s=once(s,'''            sgx535_frozen_raster_fire_plan(service->bos, SGX535_BO_COUNT,
                                           service->mmu_end, fire.actions, 20))''','''            (memcpy(fire.actions, service->plan->raster, sizeof(service->plan->raster)), 0))''')
    s=once(s,'        fire.wire = service->raster_wire;\n','')
    if any(x in s for x in ['service->bos','SGX535_BO_COUNT','service->mmu_end','fire.wire']):
        raise ValueError('fixed-role dependency remains in general service')
    # Only contract helpers get the existing kernel namespace, not new APIs.
    names=['session_abort','session_advance','bootstrap_begin','rev121_init_writes','bootstrap_observe','session_service_ready','bootstrap_ready','ta_schedule_plan','raster_schedule_plan','session_enter_fire','session_observe_status','session_begin_raster','session_timeout','session_retire']
    aliases='\n#ifdef __KERNEL__\n'+''.join('#define sgx535_frozen_'+n+' remesa_stream_sgx535_frozen_'+n+'\n' for n in names)+'#endif\n'
    s=once(s,'static int step(',aliases+'\nstatic int step(')
    return {'remesa_scene_service.c':s.encode(),'remesa_scene_service.h':(root/'kernel/remesa/gma500/remesa_scene_service.h').read_bytes()}
