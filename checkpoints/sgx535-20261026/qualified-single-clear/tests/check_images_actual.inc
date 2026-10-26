static int check_images(struct trusted_stream *s,struct trusted_operation *o,bool locked)
{
 unsigned i;int e;u8 hashes[5][32],digest[32];unsigned char *masked;
 o->input_check_calls++;
 if(o->started)return -EBUSY;
 e=locked?remesa_gem_job_check_snapshot_locked(s->memory,&o->snapshot,o->views,11):
  remesa_gem_job_check_snapshot(s->memory,&o->snapshot,o->views,11);
 if(e)return e;
 if(o->snapshot.mmu_end!=0xe0000000ULL || o->snapshot.command_bytes!=264 ||
    memcmp(o->snapshot.commands,&profile.commands,264))return -EKEYREJECTED;
 for(i=0;i<11;i++)if(o->views[i].binding.va!=profile.bindings[i].va ||
    o->views[i].binding.bytes!=profile.bindings[i].bytes)return -EKEYREJECTED;
 /* The same prepared job owns all11 holds, excludes every CPU lease and
  * has not started hardware. Constructors between calls only read inputs.
  * Recheck snapshot/commands/PTE/address/size above on EVERY call, but reuse
  * the content authentication for this operation only. New kzalloc op
  * starts unauthenticated; LAB deliberately retains every full pass. */
 if((s->fast_blake3 || s->fast_nohash) && o->inputs_authenticated)return 0;
 if(s->fast_nohash) {
  if(s->inputs_sealed)return 0;
  if(locked)return -EPROTO; /* first authentication must not relock endpoint */
  o->input_byte_passes++;
  for(i=0;i<5;i++) {
   if(o->views[i].binding.bytes!=remesa_canonical_input_sizes[i])return -EKEYREJECTED;
   if(i==SGX535_BO_PDS) {
    masked=vmalloc(o->views[i].binding.bytes);if(!masked)return -ENOMEM;
    memcpy(masked,o->views[i].cpu,o->views[i].binding.bytes);
    e=remesa_stream_mask_sources(profile.batches,profile.count,masked,o->views[i].binding.bytes,o->views[i].binding.va);
    if(!e && memcmp(masked,remesa_canonical_inputs[i],o->views[i].binding.bytes))e=-EKEYREJECTED;
    vfree(masked);if(e)return e;
   }else if(memcmp(o->views[i].cpu,remesa_canonical_inputs[i],o->views[i].binding.bytes))return -EKEYREJECTED;
  }
  e=remesa_stream_validate_geometry(profile.batches,profile.count,o->views[SGX535_BO_VERTEX_TA].cpu,o->views[SGX535_BO_VERTEX_TA].binding.bytes);
  if(!e)e=remesa_gem_stream_seal_inputs(s->memory,o->ticket,guarded_pds_write);
  if(!e){s->inputs_sealed=true;o->inputs_authenticated=true;}return e;
 }
 o->input_hash_passes++;
 for(i=0;i<5;i++) {
  e=remesa_depth1280_sha256(o->views[i].cpu,o->views[i].binding.bytes,hashes[i]);if(e)return e;
  if(i==SGX535_BO_PDS) {
   masked=vmalloc(o->views[i].binding.bytes);if(!masked)return -ENOMEM;
   memcpy(masked,o->views[i].cpu,o->views[i].binding.bytes);
   e=remesa_stream_mask_sources(profile.batches,profile.count,masked,o->views[i].binding.bytes,o->views[i].binding.va);
   if(!e)e=remesa_depth1280_sha256(masked,o->views[i].binding.bytes,digest);
   vfree(masked);if(e)return e;
  } else memcpy(digest,hashes[i],32);
  if(memcmp(digest,profile.images[i],32))return -EKEYREJECTED;
 }
 e=remesa_stream_validate_geometry(profile.batches,profile.count,o->views[SGX535_BO_VERTEX_TA].cpu,
  o->views[SGX535_BO_VERTEX_TA].binding.bytes);if(e)return e;
 e=remesa_depth1280_sha256(hashes,sizeof(hashes),o->image_digest);
 if(!e)o->inputs_authenticated=true;return e;
}
