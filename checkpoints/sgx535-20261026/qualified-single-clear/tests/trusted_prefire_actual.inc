int remesa_stream_trusted_prefire_cache(struct sgx535_gma500_owner *owner,bool *handled)
{
 struct trusted_stream *s=&stream;struct trusted_operation *o=s->op;int e;
 if(!handled || !o || &o->owner!=owner || !s->claimed)return -EACCES;
 *handled=false;if(!s->fast_nohash)return 0;
 e=remesa_gem_job_prefire_cache_locked(s->memory,o->ticket);
 if(!e)*handled=true;return e;
}
