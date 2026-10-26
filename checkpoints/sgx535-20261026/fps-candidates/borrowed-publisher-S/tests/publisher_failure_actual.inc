static void quarantine(void *ctx)
{
 struct trusted_stream *s=ctx;s->quarantined=true;
 remesa_stream_source_poison(s);
 if(s->op && s->op->ticket.operation)remesa_gem_job_quarantine(s->memory,s->op->ticket);
 /* Retain owner/aliases/provider/module/source lease on every ambiguity. */
}
static int close_stream(void *ctx)
{
 struct trusted_stream *s=ctx;int e;
 if(!s->claimed || s->quarantined || s->op)return -EBUSY;
 e=remesa_stream_source_release(s);if(e)return e;
 e=remesa_gem_stream_release(s->memory);if(e)return e;
 s->claimed=false;s->inputs_sealed=false;s->memory=NULL;return 0;
}
