static int execute(void *ctx,struct remesa_stream_receipt *r)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;int e;u64 mark;
 if(!o || r->sequence!=s->sequence || r->generation!=s->generation)return -EINVAL;
 mark=diagnostic_clock(s);e=remesa_gem_job_issue(s->memory,o->ticket);if(e)return e;
 o->issue_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);e=remesa_gem_job_wait(s->memory,o->ticket,10000);if(e)return e;
 e=remesa_gem_job_publish(s->memory,o->ticket);if(e)return e;
 o->publish_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);
 /* FULL image hashes are taken from live held provider views after actual
  * retirement/publication. No client digest or prefix is completion evidence. */
 e=s->fast_nohash?0:remesa_gears_visible_gate(o->views[5].cpu,o->views[10].cpu);o->visible_gate_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);if(e)return -e;
 if(s->fast_nohash){
  memset(r->color_hash,0,32);memset(r->depth_hash,0,32);
 }else if(s->fast_blake3){
  remesa_blake3_output(&s->output_hasher,o->views[5].cpu,r->color_hash);
  remesa_blake3_output(&s->output_hasher,o->views[10].cpu,r->depth_hash);
 }else{
 e=remesa_depth1280_sha256(o->views[5].cpu,4096000,r->color_hash);if(e)return e;
 e=remesa_depth1280_sha256(o->views[10].cpu,4096000,r->depth_hash);if(e)return e;
 }
 o->hash_ns=diagnostic_clock(s)-mark;mark=diagnostic_clock(s);e=s->fast_nohash?remesa_stream_source_finish_nohash(s,&o->observation):remesa_stream_source_finish(s,&o->observation,o->views[5].cpu);if(e)return e;
 o->source_ns=diagnostic_clock(s)-mark;
 r->operation=o->ticket.operation;r->phase=o->owner.session.phase;
 r->ledger=o->owner.session.observed_events;r->proof=s->fast_nohash?(REMESA_STREAM_PROOF_NOHASH&~64u):(63|(s->fast_blake3?REMESA_STREAM_PROOF_BLAKE3:0));o->receipt=*r;
 return 0;
}

static int seal(void *ctx,struct remesa_stream_receipt *r)
{
 struct trusted_stream *s=ctx;struct trusted_operation *o=s->op;int e;
 struct remesa_stream_receipt canonical;
 if(!o || !o->acknowledged || o->attached || !o->observation.finished || r->proof!=(s->fast_nohash?REMESA_STREAM_PROOF_NOHASH:(REMESA_STREAM_PROOF_ALL|(s->fast_blake3?REMESA_STREAM_PROOF_BLAKE3:0))))return -EINVAL;
 e=remesa_stream_source_settled(s);if(e)return e;
 if(s->fast_nohash){
  memset(r->color_hash,0,32);memset(r->depth_hash,0,32);
  memset(r->previous_sha256,0,32);memset(r->source_sha256,0,32);
  memset(r->images_sha256,0,32);memset(r->receipt_sha256,0,32);
  kfree(o);s->op=NULL;return 0;
 }
 memcpy(r->source_sha256,o->source_digest,32);memcpy(r->images_sha256,o->image_digest,32);
 canonical=*r;memset(canonical.receipt_sha256,0,32);
 e=remesa_depth1280_sha256(&canonical,sizeof(canonical),r->receipt_sha256);
 if(!e){kfree(o);s->op=NULL;}return e;
}
