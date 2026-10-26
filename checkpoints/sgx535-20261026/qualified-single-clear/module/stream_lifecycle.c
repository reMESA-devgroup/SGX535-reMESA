/* SPDX-License-Identifier: GPL-2.0-only */
#include "stream_lifecycle.h"
#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/errno.h>
#else
#include <string.h>
#include <errno.h>
#endif
static int nonzero(const unsigned char *p,unsigned n) { unsigned i; for(i=0;i<n;i++) if(p[i]) return 1; return 0; }
static int poison(struct remesa_stream *s,int e) { s->state=STREAM_QUARANTINED; s->ops.quarantine(s->backend); return e?e:-EIO; }
int remesa_stream_open(struct remesa_stream *s,const struct remesa_stream_request *r,
 const struct remesa_stream_ops *o,void *b) {
 unsigned i,j; int e;
 if(!s||!r||!o||!o->claim||!o->freeze||!o->execute||!o->settle||!o->seal||!o->quarantine||!o->close) return -EINVAL;
 if(s->state!=STREAM_FRESH) return -EBUSY;
 if(r->version!=REMESA_STREAM_VERSION||(r->action!=REMESA_STREAM_OPEN&&r->action!=REMESA_STREAM_OPEN_FAST_BLAKE3&&r->action!=REMESA_STREAM_OPEN_FAST_NOHASH)||!r->context||!r->session||r->sequence||r->width!=1280||r->height!=800||!nonzero(r->profile_sha256,32)) return -EINVAL;
 for(i=0;i<11;i++) {
  const struct remesa_stream_binding *a=&r->bindings[i];
  if(!a->id||a->reserved||!a->bytes||(a->va&4095)||(a->bytes&4095)||a->va+a->bytes<a->va) return -EINVAL;
  for(j=0;j<i;j++) if(a->id==r->bindings[j].id || (a->va<r->bindings[j].va+r->bindings[j].bytes && r->bindings[j].va<a->va+a->bytes)) return -EINVAL;
 }
 e=o->claim(b,r); if(e) return e;
 s->ops=*o; s->backend=b; s->profile=*r; s->next_sequence=1; s->next_generation=1; s->state=STREAM_IDLE; return 0;
}
static int valid_batch(const struct remesa_stream_batch *b) {
 __u64 n=b->normal_variant?160:176; __u32 d=b->normal_variant?37:41;
 if(b->normal_variant>1||!b->vertex_count||b->vertex_count>65535||!b->index_count||b->index_count%3||b->index_count>65535||b->vertex_stride!=(b->normal_variant?32u:16u)) return 0;
 if(b->vertex_bytes!=(__u64)b->vertex_count*b->vertex_stride||b->index_bytes!=(__u64)b->index_count*2||b->source_capacity!=n||b->source_effective_dwords!=d||b->source_dma_dwords!=d+3||!b->image_generation) return 0;
 if((b->vertex_offset&15)||(b->index_offset&1)||(b->source_offset&15)) return 0;
 /* private GEN128K profile; capacity is distinct from effective vertex data */
 if(b->vertex_offset>131072||b->vertex_bytes>131072-b->vertex_offset||b->index_offset>131072||b->index_bytes>131072-b->index_offset) return 0;
 if(b->vertex_offset<b->index_offset+b->index_bytes && b->index_offset<b->vertex_offset+b->vertex_bytes) return 0;
 return 1;
}
int remesa_stream_action(struct remesa_stream *s,struct remesa_stream_request *r) {
 struct remesa_stream_receipt *p; int e;
 if(!s||!r||r->version!=REMESA_STREAM_VERSION||r->context!=s->profile.context||r->session!=s->profile.session) return -EINVAL;
 if(s->state==STREAM_QUARANTINED) return -EIO;
 if(r->action==REMESA_STREAM_CLOSE) {
  if(s->state!=STREAM_IDLE) return -EBUSY;
  e=s->ops.close(s->backend); if(e) return poison(s,e);
  s->state=STREAM_CLOSED; return 0;
 }
 if(r->sequence!=s->next_sequence) return -ESTALE;
 switch(r->action) {
 case REMESA_STREAM_BEGIN:
  if(s->state!=STREAM_IDLE) return -EBUSY;
  s->count=0; s->state=STREAM_STAGING; return 0;
 case REMESA_STREAM_STAGE:
  if(s->state!=STREAM_STAGING) return -EBUSY;
  if(s->count==REMESA_STREAM_BATCHES) return -ENOSPC;
  if(!valid_batch(&r->batch)) return -EINVAL;
  s->batches[s->count++]=r->batch; return 0;
 case REMESA_STREAM_FINISH:
  if(s->state!=STREAM_STAGING||!s->count) return -EBUSY;
  if(!s->next_generation||s->next_sequence==~(__u64)0) return poison(s,-EOVERFLOW);
  /* freeze failures may hold producers: never infer safe cancellation. */
  e=s->ops.freeze(s->backend,s); if(e) return poison(s,e);
  s->state=STREAM_ISSUED;
  p=&s->history[s->completed%REMESA_STREAM_HISTORY]; memset(p,0,sizeof(*p));
  p->session=s->profile.session; p->sequence=s->next_sequence; p->generation=s->next_generation++;
  if(s->completed) memcpy(p->previous_sha256,s->history[(s->completed-1)%REMESA_STREAM_HISTORY].receipt_sha256,32);
  e=s->ops.execute(s->backend,p); if(e) return poison(s,e);
  if(p->session!=s->profile.session||p->sequence!=s->next_sequence||p->generation!=s->next_generation-1||!p->operation||p->phase!=9||p->ledger!=7||p->error||(s->profile.action==REMESA_STREAM_OPEN_FAST_NOHASH?(nonzero(p->color_hash,32)||nonzero(p->depth_hash,32)):(!nonzero(p->color_hash,32)||!nonzero(p->depth_hash,32)))||(p->proof&~64u)!=(remesa_stream_proof_for_open(s->profile.action)&~64u)) return poison(s,-EPROTO);
  e=s->ops.settle(s->backend); if(e) return poison(s,e);
  p->proof|=64u;
  e=s->ops.seal(s->backend,p); if(e) return poison(s,e);
  if(s->profile.action==REMESA_STREAM_OPEN_FAST_NOHASH) {
   if(!remesa_stream_receipt_hashes_zero(p))return poison(s,-EPROTO);
  } else if(!nonzero(p->receipt_sha256,32)||!nonzero(p->source_sha256,32)||!nonzero(p->images_sha256,32)) return poison(s,-EPROTO);
  s->completed++; if(s->history_count<64) s->history_count++;
  s->state=STREAM_WAIT_ACK; r->receipt=*p; return 0;
 case REMESA_STREAM_ACK:
  if(s->state!=STREAM_WAIT_ACK) return -EBUSY;
  p=&s->history[(s->completed-1)%64];
  if(memcmp(&r->receipt,p,sizeof(*p))) return -ESTALE;
  s->acknowledged++; s->next_sequence++; s->state=STREAM_IDLE; return 0;
 default: return -EINVAL;
 }
}
