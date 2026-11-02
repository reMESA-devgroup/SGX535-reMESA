/* SPDX-License-Identifier: GPL-2.0-only */
/* After retained ta_for_cpu/cache helpers, before memory requests. These are
 * ownership transitions, not validation or a warm-initialization credential.
 * Callers hold memory lock; publication also authenticates the complete loan. */
static bool private_dpm_domain(const struct remesa_gem_memory *s,const struct backing *b)
{
 unsigned i;
 if(!s->private_session||!s->private_dpm_ids[0]||!s->private_dpm_ids[1]||
    s->private_dpm_ids[0]==s->private_dpm_ids[1])return false;
 for(i=0;i<2;i++){
  u32 id=s->private_dpm_ids[i];
  if(b==&s->bo[id&31]&&b->live&&b->generation==id>>5&&b->reserved&&b->pinned&&
     b->pages&&b->gem.size==33554432ULL&&b->inserted==8192)return true;
 }
 return false;
}
static void private_dpm_publish(struct remesa_gem_memory *s,struct backing *b,bool gpu_write)
{
 if(!private_dpm_domain(s,b)){
  drm_clflush_pages(b->pages,b->inserted);return;
 }
 /* Preserve the donor policy even for clean CPU read-cache lines. A CPU
  * lease always sets cached again, so its next GPU consumer flushes once. */
 if(b->ta_cpu_cached||b->ta_cpu_dirty){
  drm_clflush_pages(b->pages,b->inserted);ta_cache_flushed(b);
 }
 /* Before possible device use; failures retain the job/session, not authority
  * to read, retry, unseal or infer retirement. */
 if(gpu_write){s->private_dpm_ready=false;b->ta_gpu_dirty=true;}
}
static void private_dpm_complete(struct remesa_gem_memory *s,struct backing *b)
{
 /* Successful retirement does not require transferring pool ownership to
  * CPU when no CPU consumer exists. An actual lease or final unseal uses
  * the retained ta_for_cpu transition. Other GPU-write roles stay unchanged. */
 if(!private_dpm_domain(s,b))drm_clflush_pages(b->pages,b->inserted);
}
