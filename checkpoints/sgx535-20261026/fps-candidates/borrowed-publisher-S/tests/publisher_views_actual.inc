static int bind_publisher_images(struct trusted_stream *s,struct trusted_operation *o)
{
 unsigned i;
 if(s->fast_nohash){
  if(!s->claimed || s->quarantined || s->op!=o || o->borrowed_images ||
     !o->owner.borrowed_backing || !o->owner.initialized ||
     o->owner.user_images_flushed ||
     o->owner.session.phase!=SGX535_PHASE_HOST_IMAGE_FINAL)return -EACCES;
  /* Validate ALL roles first: a rejected loan publishes no partial pointers. */
  for(i=0;i<=SGX535_BO_COLOR;i++){
   struct sgx535_gma500_bo *b=&o->owner.objects[i];
   const struct remesa_memory_view *v=&o->views[i];
   if(b->cpu || o->owner.views[i].bytes || o->owner.views[i].length ||
      !v->cpu || !v->pages || b->pages!=v->pages ||
      b->page_count!=v->page_count || b->gem.size!=v->binding.bytes)
    return -EACCES;
  }
  for(i=0;i<=SGX535_BO_COLOR;i++){
   o->owner.objects[i].cpu=o->views[i].cpu;
   o->owner.views[i].bytes=o->views[i].cpu;
   o->owner.views[i].length=o->views[i].binding.bytes;
  }
  o->borrowed_images=true;return 0;
 }
 for(i=0;i<=SGX535_BO_COLOR;i++) {
  o->owner.objects[i].cpu=vmap(o->views[i].pages,o->views[i].page_count,VM_MAP,PAGE_KERNEL);
  if(!o->owner.objects[i].cpu)return -ENOMEM;
  o->owner.views[i].bytes=o->owner.objects[i].cpu;o->owner.views[i].length=o->views[i].binding.bytes;
 }
 return 0;
}
int remesa_stream_trusted_consume_images(struct sgx535_gma500_owner *owner,bool *handled)
{
 struct trusted_stream *s=&stream;struct trusted_operation *o=s->op;unsigned i;
 if(!handled)return -EINVAL;
 *handled=false;
 if(!o || &o->owner!=owner || !s->claimed)return -EACCES;
 if(!s->fast_nohash)return 0;
 *handled=true;
 if(s->quarantined || !o->borrowed_images || !owner->borrowed_backing ||
    !owner->initialized || owner->user_images_flushed || o->completed ||
    o->visible || o->acknowledged ||
    owner->session.phase!=SGX535_PHASE_HOST_IMAGE_FINAL)return -EACCES;
 /* Called only after publication/PFN validation, held-provider cache transfer
  * and dma_wmb in stream_owner_flush, under the existing GEM job mutex.
  * Validate every loan before revoking any pointer. No extra PFN walk. */
 for(i=0;i<=SGX535_BO_COLOR;i++){
  struct sgx535_gma500_bo *b=&owner->objects[i];
  const struct remesa_memory_view *v=&o->views[i];
  if(!v->cpu || !v->pages || b->pages!=v->pages ||
     b->page_count!=v->page_count || b->gem.size!=v->binding.bytes ||
     b->cpu!=v->cpu || owner->views[i].bytes!=v->cpu ||
     owner->views[i].length!=v->binding.bytes)return -EACCES;
 }
 for(i=0;i<=SGX535_BO_COLOR;i++){
  owner->objects[i].cpu=NULL;
  owner->views[i].bytes=NULL;owner->views[i].length=0;
 }
 o->borrowed_images=false;return 0;
}
