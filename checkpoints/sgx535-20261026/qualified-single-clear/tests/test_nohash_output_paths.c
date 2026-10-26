#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include "../module/stream_uapi.h"
typedef uint64_t u64;
struct fake_owner {struct {unsigned phase,observed_events;}session;};
struct trusted_operation {struct remesa_stream_receipt receipt;struct {unsigned operation;}ticket;struct {void*cpu;}views[11];struct fake_owner owner;struct {bool finished;}observation;bool acknowledged,attached;unsigned char source_digest[32],image_digest[32];u64 issue_ns,publish_ns,visible_gate_ns,hash_ns,source_ns;};
struct trusted_stream {struct trusted_operation*op;void*memory;bool fast_nohash,fast_blake3;u64 sequence,generation;int output_hasher;};
static unsigned sha_calls,b3_calls,legacy_finish,nohash_finish,gate_calls,settled_calls,job_calls;static int gate_error;
static u64 ktime_get_ns(void){static u64 n;return ++n;}
static int remesa_gem_job_issue(void*a,typeof(((struct trusted_operation*)0)->ticket)t){(void)a;(void)t;job_calls++;return 0;}
#define remesa_gem_job_publish remesa_gem_job_issue
static int remesa_gem_job_wait(void*a,typeof(((struct trusted_operation*)0)->ticket)t,unsigned n){assert(n==10000);return remesa_gem_job_issue(a,t);}
static int remesa_gears_visible_gate(void*c,void*z){(void)c;(void)z;gate_calls++;return gate_error;}
static void remesa_blake3_output(void*h,void*p,unsigned char*out){(void)h;(void)p;b3_calls++;memset(out,3,32);}
static int remesa_depth1280_sha256(const void*p,unsigned n,unsigned char*out){(void)p;assert(n);sha_calls++;memset(out,2,32);return 0;}
static int remesa_stream_source_finish_nohash(void*s,void*o){(void)s;(void)o;nohash_finish++;return 0;}
static int remesa_stream_source_finish(void*s,void*o,void*c){(void)s;(void)o;(void)c;legacy_finish++;return 0;}
static int remesa_stream_source_settled(void*s){(void)s;settled_calls++;return 0;}
#define kfree free
static u64 diagnostic_clock(const struct trusted_stream*s){return s->fast_nohash?0:ktime_get_ns();}
#include "nohash_output_actual.inc"
static void setup(struct trusted_stream*s,struct remesa_stream_receipt*r){memset(r,0,sizeof(*r));s->op=calloc(1,sizeof(*s->op));assert(s->op);s->sequence=s->generation=r->sequence=r->generation=1;s->op->ticket.operation=17;s->op->owner.session.phase=9;s->op->owner.session.observed_events=7;}
int main(void){struct trusted_stream s={.fast_nohash=true};struct remesa_stream_receipt r;setup(&s,&r);memset(r.color_hash,9,192);assert(!execute(&s,&r));assert(r.proof==47&&r.operation==17&&r.phase==9&&r.ledger==7);assert(job_calls==3&&gate_calls==0&&nohash_finish==1&&!sha_calls&&!b3_calls&&!legacy_finish);s.op->acknowledged=true;s.op->observation.finished=true;r.proof|=64;assert(!seal(&s,&r)&&!s.op);for(unsigned i=0;i<192;i++)assert(!((unsigned char*)&r)[48+i]);assert(!sha_calls&&!b3_calls&&settled_calls==1);
 setup(&s,&r);s.fast_nohash=false;gate_error=EIO;assert(execute(&s,&r)==-EIO&&gate_calls==1&&nohash_finish==1);free(s.op);gate_error=0;
 for(unsigned mode=0;mode<2;mode++){s.fast_nohash=false;s.fast_blake3=mode;setup(&s,&r);assert(!execute(&s,&r));assert(r.proof==(mode?191:63));s.op->acknowledged=true;s.op->observation.finished=true;r.proof|=64;assert(!seal(&s,&r));}assert(sha_calls==4&&b3_calls==2&&legacy_finish==2);puts("PASS actual execute/seal: NOHASH zero SHA/B3, six ZERO arrays, proof47/111; visible failure retained; LAB/FAST7 hashes retained; fake lifecycle/GPU0");}
