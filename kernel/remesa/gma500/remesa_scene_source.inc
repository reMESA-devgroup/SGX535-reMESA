/* SPDX-License-Identifier: GPL-2.0-only */
/* Same real source lock, boundary and capsule as the qualified route. No
 * fallback to legacy provenance; all identities belong to the retained job. */
#ifdef REMESA_GENERAL_SCENE
static bool scene_source_matches(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s)
{return valid(b)&&b->general_scene&&s==&b->general_scene->service&&s->session==&b->general_scene->session;}
void remesa_route_scene_service(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s,int matched)
{
 unsigned long f;spin_lock_irqsave(&source_lock,f);
 if(!scene_source_matches(b,s))lost();
 else {event(1,0,matched,0,s->session->observed_events,s->session->phase);sgx535_capsule_service(&operation->evidence,b,s,matched);}
 spin_unlock_irqrestore(&source_lock,f);
}
void remesa_route_scene_before(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s,u32 q,u32 a,u32 d,int exclusive)
{
 unsigned long f;spin_lock_irqsave(&source_lock,f);
 if(!scene_source_matches(b,s))lost();
 else {event(5,q,a,d,s->session->observed_events,s->session->phase);sgx535_capsule_before(&operation->evidence,b,s,q,a,d,exclusive);}
 spin_unlock_irqrestore(&source_lock,f);
}
void remesa_route_scene_after(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s)
{
 unsigned long f;spin_lock_irqsave(&source_lock,f);
 if(!scene_source_matches(b,s))lost();
 else {event(6,operation->operation,0,0,s->session->observed_events,s->session->phase);sgx535_capsule_after(&operation->evidence,b,s,s->session->observed_events,s->session->phase);}
 spin_unlock_irqrestore(&source_lock,f);
}
void remesa_route_scene_terminal(struct sgx535_gma500_fixed_backend *b,struct remesa_scene_service *s,int result)
{
 unsigned long f;spin_lock_irqsave(&source_lock,f);
 if(!scene_source_matches(b,s))lost();
 else {event(7,operation->operation,result,0,s->session->observed_events,s->session->phase);sgx535_capsule_terminal(&operation->evidence,b,s,result,s->session->phase,s->session->observed_events);}
 spin_unlock_irqrestore(&source_lock,f);
}
#endif
