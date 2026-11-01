/* SPDX-License-Identifier: GPL-2.0-only. Include after retained GEM core. */
#include "remesa_gem_dpm.h"
int remesa_gem_job_claim_zero_dpm(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,u32 table_id,u32 parameter_id)
{
 struct retained_job *j;struct backing *b;u32 ids[2]={table_id,parameter_id},mask=0;
 unsigned role,i;int error=-ENOENT;
 if(!s||!table_id||!parameter_id||table_id==parameter_id)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->quarantined){error=-EIO;goto done;}
 if(j->state!=REMESA_JOB_PREPARED||j->attached||j->cookie||s->stream_bound||s->ta_cache_domains){error=-EBUSY;goto done;}
 if(j->dpm_mask){error=-EALREADY;goto done;}
 for(role=0;role<2;role++){
  for(i=0;i<j->count&&j->ids[i]!=ids[role];i++);
  if(i==j->count){error=-ENOENT;goto done;}
  b=&s->bo[ids[role]&31];
  if(!b->live||b->generation!=ids[role]>>5||b->holds!=1||b->lease||b->quarantine||
     !b->pinned||!b->reserved||!b->pages||b->inserted!=8192||b->gem.size!=33554432ULL){error=-EACCES;goto done;}
  if(!b->initialized||!b->zero_pristine||b->cached_job_view||j->cpu_views[i]||atomic_read(&b->color_vmas)){error=-ENODATA;goto done;}
  if(role==0){
   if(b->va.start<0x40000000ULL||b->va.start>0x100000000ULL-b->gem.size){error=-ERANGE;goto done;}
  }else if(b->va.start<0x30000000ULL||b->va.start>0x40000000ULL-b->gem.size||
           b->va.start%0x100000ULL){error=-ERANGE;goto done;}
  mask|=(u32)1<<i;
 }
 /* Authenticate both before reserving either. Holds freeze backing/leases. */
 j->dpm_mask=mask;error=0;
 done:mutex_unlock(&s->lock);return error;
}
int remesa_gem_job_snapshot_selected(struct remesa_gem_memory *s,
 struct remesa_memory_ticket t,struct remesa_memory_snapshot *out,
 struct remesa_memory_view *views,unsigned capacity,u32 cpu_mask)
{
 struct retained_job *j;struct backing *b;struct remesa_memory_binding uses[REMESA_GEM_JOB_BINDINGS];
 bool added[REMESA_GEM_JOB_BINDINGS]={false};unsigned i;int error=-ENOENT;
 if(!s||!out||!views)return -EINVAL;
 mutex_lock(&s->lock);j=ticket_job(s,t);if(!j)goto done;
 if(j->state!=REMESA_JOB_PREPARED){error=-EBUSY;goto done;}
 if(capacity<j->count){error=-ENOSPC;goto done;}
 if(j->count<32&&(cpu_mask>>j->count)){error=-EINVAL;goto done;}
 if(cpu_mask&j->dpm_mask){error=-EACCES;goto done;}
 for(i=0;i<j->count;i++)if(!(cpu_mask&((u32)1<<i))&&j->cpu_views[i]){error=-EBUSY;goto done;}
 error=verify_job_bindings(s,j,uses);if(error){error=job_fault(s,j,error);goto done;}
 for(i=0;i<j->count;i++)if((cpu_mask&((u32)1<<i))&&!j->cpu_views[i]){
  b=&s->bo[j->ids[i]&31];
  if(!b->cached_job_view){
   b->cached_job_view=vmap(b->pages,b->gem.size/PAGE_SIZE,VM_MAP,PAGE_KERNEL);
   if(!b->cached_job_view){error=-ENOMEM;goto rollback;}
   added[i]=true;
  }
  j->cpu_views[i]=b->cached_job_view;
 }
 for(i=0;i<j->count;i++){
  b=&s->bo[j->ids[i]&31];
  if(cpu_mask&((u32)1<<i))b->zero_pristine=false;
  views[i]=(struct remesa_memory_view){uses[i],b->pages,b->gem.size/PAGE_SIZE,
   cpu_mask&((u32)1<<i)?j->cpu_views[i]:NULL};
 }
 *out=(struct remesa_memory_snapshot){s->dev,t,s->root.end+1,j->commands,j->command_bytes,true,true};
 error=j->count;goto done;
 rollback:
 for(i=0;i<j->count;i++)if(added[i]){
  b=&s->bo[j->ids[i]&31];vunmap(b->cached_job_view);b->cached_job_view=NULL;j->cpu_views[i]=NULL;
 }
 done:mutex_unlock(&s->lock);return error;
}
