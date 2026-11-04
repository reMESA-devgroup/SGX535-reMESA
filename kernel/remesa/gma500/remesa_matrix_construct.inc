/* SPDX-License-Identifier: GPL-2.0-only */
/* Memory lock held. This is only an idle CPU construction phase of the
 * concrete ordinary clear/draw owner, never GPU or source authorization.
 * All normal act() ID/generation/range/PTE/lease/permission checks still run. */
static bool matrix_color_protected_write(const struct remesa_gem_memory *s,
 const struct remesa_memory_request *r)
{
 if(!s->private_session||!s->private_matrix_color||
    (r->id!=s->private_matrix_color&&(!s->private_matrix_depth||r->id!=s->private_matrix_depth)))return false;
 return r->action!=REMESA_MEM_READ&&r->action!=REMESA_MEM_UNLEASE&&r->action!=REMESA_MEM_SYNC&&
  !(r->action==REMESA_MEM_LEASE&&!r->flags);
}
static bool matrix_cpu_construct(const struct remesa_gem_memory *s,
 const struct remesa_memory_request *r)
{
 unsigned i;
 if(!s->private_matrix_constructing||!s->private_matrix_color||!s->private_session||
    s->private_device_lost||!s->file||s->private_source_poisoned||s->stream_bound||
    s->executor||s->ta_cache_domains)return false;
 for(i=0;i<8;i++)if(s->jobs[i].live)return false;
 if(r->id==s->private_matrix_color||(s->private_matrix_depth&&r->id==s->private_matrix_depth)){
  /* Keep COLOR and the retired Z backing/content until source closure.
   * Z becomes protected only after a successfully retired matrix draw.
   * Read leases remain ordinary serialized leases, not writable mappings. */
  return (r->action==REMESA_MEM_LEASE&&!r->flags)||r->action==REMESA_MEM_READ||
   r->action==REMESA_MEM_UNLEASE||r->action==REMESA_MEM_SYNC;
 }
 switch(r->action){
 case REMESA_MEM_CREATE:case REMESA_MEM_PIN:case REMESA_MEM_MAP:
 case REMESA_MEM_UNMAP:case REMESA_MEM_UNPIN:case REMESA_MEM_DESTROY:
 case REMESA_MEM_LEASE:case REMESA_MEM_READ:case REMESA_MEM_WRITE:
 case REMESA_MEM_UNLEASE:case REMESA_MEM_SYNC:return true;
 default:return false;
 }
}
