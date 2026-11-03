/* SPDX-License-Identifier: GPL-2.0-only */
/* Retain scalar PARAM policy from the existing, fully validated retired job.
 * Partial memory-only tests do not instantiate the general scene executor. */
#if !defined(REMESA_MEMORY_TEST) || defined(REMESA_SCENE_PARAMETER_BIND_TEST)
#ifndef REMESA_SCENE_PARAMETER_BIND_TEST
#include "remesa_gem_scene_job.h"
#endif
static bool scene_parameter_ticket_same(struct remesa_memory_ticket a,
 struct remesa_memory_ticket b)
{return a.context==b.context&&a.operation==b.operation&&a.slot==b.slot&&a.generation==b.generation;}
static bool scene_parameter_output_overlap(const void *a,unsigned n,const void *b,unsigned m)
{
 unsigned long x=(unsigned long)a,y=(unsigned long)b;
 if(x>~0UL-n||y>~0UL-m)return true;
 return x<=y?y-x<n:x-y<m;
}
int remesa_private_session_bind_scene_parameters(struct remesa_gem_memory *s,
 const void *owner,const struct remesa_gem_scene_job *job,unsigned draw,u32 maximum_abs_bits,
 struct remesa_parameter_patch_policy *out)
{
 struct remesa_parameter_patch_policy value={0};struct retained_job *j;
 struct backing *b;unsigned r,k,upload,view;u32 id;int error=-EACCES;
 if(!s||!owner||!job||!out||!draw||maximum_abs_bits>0x7f7fffffU)return -EINVAL;
 if(scene_parameter_output_overlap(out,sizeof(*out),s,sizeof(*s))||
    scene_parameter_output_overlap(out,sizeof(*out),job,sizeof(*job)))return -EINVAL;
 mutex_lock(&endpoint_lock);if(endpoint!=s)goto done;
 mutex_lock(&s->lock);
 if(s->private_session!=owner||job->memory!=s||job->loan.memory!=s)goto unlock;
 if(s->private_device_lost||!s->file){error=-ENODEV;goto unlock;}
 if(s->private_source_poisoned){error=-EIO;goto unlock;}
 if(!s->private_source_held||!s->private_dpm_ready||
    job->state!=REMESA_SCENE_JOB_ACKNOWLEDGED||
    !scene_parameter_ticket_same(job->snapshot.ticket,job->loan.ticket)||
    !scene_parameter_ticket_same(job->states.ticket,job->loan.ticket)||
    !scene_parameter_ticket_same(job->ta.ticket,job->loan.ticket)||
    s->private_dpm_retired_operation!=job->loan.ticket.operation){error=-ESTALE;goto unlock;}
 j=ticket_job(s,job->loan.ticket);
 if(!j||j->state!=REMESA_JOB_ACKNOWLEDGED||j->cookie!=j->operation||j->quarantined||
    j->private_scene_owner!=owner||!j->attached||!job->count||job->count>32||j->count!=job->count){
  error=-EBUSY;goto unlock;
 }
 if(draw>job->ta.draw_count||job->ta.draw_count>REMESA_TA_REFERENCE_MAX_DRAWS||
    !job->ta.reference_count||job->ta.reference_count>REMESA_TA_REFERENCE_MAX||
    !job->states.count||job->states.count>REMESA_GEM_SCENE_STATE_MAX){error=-EPROTO;goto unlock;}
 error=scene_input_objects_locked(s);if(error)goto unlock;
 r=job->ta.reference_count;
 for(k=0;k<job->ta.reference_count;k++)if(job->ta.references[k].kind==REMESA_TA_REF_DRAW_PARAMETERS&&
    job->ta.references[k].draw==draw){
  if(r!=job->ta.reference_count){error=-EPROTO;goto unlock;}r=k;
 }
 if(r==job->ta.reference_count){error=-ENODATA;goto unlock;}
 upload=job->states.count;
 for(k=0;k<job->states.count;k++)if(job->states.uploads[k].pds_id==job->ta.references[r].id&&
    job->states.uploads[k].pds_offset==job->ta.references[r].byte_offset){
  if(upload!=job->states.count&&memcmp(&job->states.uploads[upload],&job->states.uploads[k],
      sizeof(job->states.uploads[k]))){error=-EPROTO;goto unlock;}
  upload=k;
 }
 if(upload==job->states.count){error=-ENOENT;goto unlock;}
 id=job->states.uploads[upload].source_id;
 /* This is the existing owned source37/transfer40 producer, already decoded
  * by remesa_parameter_upload_* during execution. No new instruction decode.
  * The embedded TA8 extent is a CPU producer reservation, not a GPU fetch. */
 if(id!=job->states.uploads[upload].use_id||id!=job->states.uploads[upload].pds_id||
    job->states.uploads[upload].source_bytes!=160||
    (u64)job->states.uploads[upload].source_offset+160!=job->states.uploads[upload].use_offset||
    (u64)job->states.uploads[upload].use_offset+32!=job->states.uploads[upload].pds_offset||
    job->states.uploads[upload].pds_offset>0xffffffffU-64){error=-EACCES;goto unlock;}
 for(view=0;view<job->count&&job->views[view].binding.id!=id;view++);
 if(view==job->count||!(s->private_scene_input_mask&((u32)1<<view))||
    s->private_scene_input_ids[view]!=id||j->ids[view]!=id){error=-EACCES;goto unlock;}
 b=&s->bo[id&31];
 if(!id||b->generation!=id>>5||b->holds!=1||b->lease||b->cpu||
    job->views[view].binding.va!=b->va.start||job->views[view].binding.bytes!=b->gem.size||
    b->gem.size>0xffffffffU){error=-EACCES;goto unlock;}
 value.window=(struct remesa_parameter_window){.owner=s->context,.generation=b->generation,
  .gpu_va=b->va.start,.id=id,.allocation_bytes=(u32)b->gem.size,
  .source_offset=job->states.uploads[upload].source_offset,.coefficient_dwords=36,
  .effective_source_dwords=37,.transfer_dwords=40,
  .use_offset=job->states.uploads[upload].use_offset,.use_bytes=8,
  .pds_offset=job->states.uploads[upload].pds_offset,.pds_bytes=60,
  .ta_offset=job->states.uploads[upload].pds_offset+64,.ta_bytes=8};
 value.maximum_abs_bits=maximum_abs_bits;
 if(!remesa_parameter_window_valid(&value.window)){error=-ERANGE;goto unlock;}
 *out=value;error=0;
 unlock:mutex_unlock(&s->lock);
 done:mutex_unlock(&endpoint_lock);return error;
}
#endif
