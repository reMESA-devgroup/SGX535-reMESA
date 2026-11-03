/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual software-orphan fragments and memory-close body; reduced metadata,
 * modeled mutexes/resource callbacks. No GPU/MMIO/backing is released. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "remesa_private_file_lost.h"
#define OBJECTS 32
#define THIS_MODULE NULL
enum {REMESA_JOB_PREPARED=1,REMESA_JOB_QUARANTINED=9,
 STREAM_IDLE=1,STREAM_STAGING=2,STREAM_ISSUED=3,STREAM_QUARANTINED=5};
struct drm_device {unsigned id;};struct drm_file {unsigned id;};
struct mutex {unsigned rank;bool held;};
static unsigned rank,cleanup_calls,source_poison_calls,prior_rank[4];
static struct mutex stream_lock={1,false},endpoint_lock={2,false};
static void mutex_lock(struct mutex *m)
{assert(!m->held);assert(m->rank==3?(rank==0||rank==2):!rank);
 prior_rank[m->rank]=rank;m->held=true;rank=m->rank;}
static void mutex_unlock(struct mutex *m)
{assert(m->held&&rank==m->rank);m->held=false;rank=prior_rank[m->rank];}
struct retained_job {
 bool live,quarantined,attached;unsigned state,count,ids[16];void *commands,*cpu_views[16];
 uint64_t operation,cookie;
};
struct backing {bool live,quarantine,lease;unsigned color_vmas,holds;void *pages,*cpu;};
struct resource {unsigned token;};
struct remesa_gem_memory {
 struct drm_device *dev;struct drm_file *file;struct mutex lock;
 const void *private_session;
 struct retained_job jobs[8];struct backing bo[OBJECTS];bool private_device_lost,stream_bound,excluded,private_matrix_constructing;
 struct resource exclude;unsigned device_refs,module_refs,power_refs;
};
struct trusted_stream {
 struct drm_device *dev;struct drm_file *file;struct remesa_gem_memory *memory;
 void *op;bool claimed,quarantined;uint64_t session,sequence,generation;
};
struct fake_ops {int (*close)(void *);void (*quarantine)(void *);};
struct remesa_stream {unsigned state;struct fake_ops ops;void *backend;};
static struct remesa_gem_memory *endpoint;
static struct trusted_stream stream;
static struct remesa_stream *active;
static struct drm_file *owner;
static struct drm_device *owner_device;
static bool permanently_quarantined;
static void remesa_stream_source_poison(const void *who)
{assert(who==&stream&&rank==1);source_poison_calls++;}
#include "remesa_private_trusted_file_lost.inc"
#include "remesa_private_memory_file_lost.inc"
#include "remesa_private_stream_file_lost.inc"
static int atomic_read(unsigned *value){return *value;}
static void drop_job_views(struct retained_job *j){(void)j;cleanup_calls++;}
static int unmap(struct remesa_gem_memory *s,struct backing *b){(void)s;(void)b;cleanup_calls++;return 0;}
static void discard(struct remesa_gem_memory *s,struct backing *b){(void)s;(void)b;cleanup_calls++;}
static void release_resource(struct resource *r){(void)r;cleanup_calls++;}
static void sgx535_gma500_exclusive_release(struct drm_device *d){(void)d;cleanup_calls++;}
static void gma_power_end(struct drm_device *d){(void)d;cleanup_calls++;}
static void drm_dev_put(struct drm_device *d){(void)d;cleanup_calls++;}
static void module_put(void *m){(void)m;cleanup_calls++;}
static void kfree(void *p){(void)p;cleanup_calls++;}
#include "memory_close_actual.inc"
static int native_close_error;
static unsigned native_closes,native_quarantines;
static int modeled_close(void *who)
{assert(who==&stream&&rank==1);native_closes++;if(native_close_error)return native_close_error;
 assert(!stream.op);stream.claimed=false;stream.memory=NULL;return 0;}
static void modeled_quarantine(void *who)
{assert(who==&stream&&rank==1);native_quarantines++;stream.quarantined=true;remesa_stream_source_poison(who);}
static void remesa_gem_memory_postclose(struct drm_device *d,struct drm_file *f)
{assert(!rank);if(endpoint&&endpoint->dev==d&&endpoint->file==f){
 if(!native_close_error&&!stream.claimed)endpoint=NULL;else endpoint->file=NULL;
}}
#include "native_postclose_actual.inc"
static int forbidden_close(void *p){(void)p;assert(0);return -EIO;}
static void forbidden_quarantine(void *p){(void)p;assert(0);}
static struct drm_device dev={1},otherdev={2};
static struct drm_file file={1},otherfile={2};
static struct remesa_gem_memory memory;
static struct remesa_stream bridge;
static void fixture(bool resources)
{
 assert(!rank&&!stream_lock.held&&!endpoint_lock.held);
 cleanup_calls=source_poison_calls=0;permanently_quarantined=false;
 native_close_error=0;native_closes=native_quarantines=0;
 memory=(struct remesa_gem_memory){.dev=&dev,.file=&file,.lock={3,false},
  .device_refs=1,.module_refs=1,.power_refs=1};
 if(resources){
  memory.jobs[0]=(struct retained_job){.live=true,.attached=true,.state=3,
    .count=1,.ids={32},.commands=(void*)0x1234,.cpu_views={(void*)0x5678},.operation=11,.cookie=12};
  memory.bo[0]=(struct backing){.live=true,.lease=true,.holds=1,.pages=(void*)0x2345,.cpu=(void*)0x4567};
 }
 endpoint=&memory;
 stream=(struct trusted_stream){.dev=&dev,.file=&file,.memory=&memory,.op=(void*)0x7890,
  .claimed=true,.session=1,.sequence=2,.generation=3};
 bridge=(struct remesa_stream){.state=2,.ops={forbidden_close,forbidden_quarantine},.backend=&stream};
 active=&bridge;owner=&file;owner_device=&dev;
}
int main(void)
{
 fixture(true);struct remesa_gem_memory before=memory;
 struct trusted_stream trusted_before=stream;struct remesa_stream bridge_before=bridge;
 remesa_private_stream_file_lost(&otherdev,&file);
 remesa_private_stream_file_lost(&dev,&otherfile);
 remesa_private_stream_file_lost(&dev,NULL);
 assert(!memcmp(&memory,&before,sizeof memory)&&!memcmp(&stream,&trusted_before,sizeof stream));
 assert(!memcmp(&bridge,&bridge_before,sizeof bridge)&&!source_poison_calls&&!cleanup_calls);
 remesa_private_stream_file_lost(&dev,&file);
 before.file=NULL;before.private_device_lost=true;before.jobs[0].quarantined=true;
 before.jobs[0].state=REMESA_JOB_QUARANTINED;before.bo[0].quarantine=true;
 trusted_before.file=NULL;trusted_before.quarantined=true;
 bridge_before.state=STREAM_QUARANTINED;
 assert(!memcmp(&memory,&before,sizeof memory)&&!memcmp(&stream,&trusted_before,sizeof stream));
 assert(!memcmp(&bridge,&bridge_before,sizeof bridge)&&active==&bridge);
 assert(!owner&&!owner_device&&permanently_quarantined&&source_poison_calls==1&&!cleanup_calls);
 assert(remesa_gem_memory_close(&memory)==-ENODEV&&!cleanup_calls);
 remesa_private_stream_file_lost(&dev,&file);
 assert(source_poison_calls==1&&!cleanup_calls&&!rank);
 fixture(false);active=NULL;owner=NULL;owner_device=NULL;
 remesa_private_stream_file_lost(&dev,&file);
 assert(!stream.file&&stream.quarantined&&!memory.file&&memory.private_device_lost);
 assert(remesa_gem_memory_close(&memory)==-ENODEV&&!cleanup_calls);
 fixture(false);assert(!remesa_gem_memory_close(&memory)&&cleanup_calls&&!rank);
 /* Device loss must revoke open files without waiting for postclose. */
 fixture(true);before=memory;trusted_before=stream;bridge_before=bridge;
 assert(!remesa_private_memory_device_retained(&dev));
 assert(!remesa_private_memory_rebind_blocked());
 remesa_private_stream_device_lost(NULL);
 remesa_private_stream_device_lost(&otherdev);
 assert(!memcmp(&memory,&before,sizeof memory)&&!memcmp(&stream,&trusted_before,sizeof stream));
 assert(!memcmp(&bridge,&bridge_before,sizeof bridge)&&!source_poison_calls&&!cleanup_calls);
 remesa_private_stream_device_lost(&dev);
 assert(!owner&&!owner_device&&!memory.file&&!stream.file);
 assert(memory.private_device_lost&&stream.quarantined&&permanently_quarantined);
 assert(remesa_private_memory_device_retained(&dev));
 assert(!remesa_private_memory_device_retained(&otherdev));
 assert(!remesa_private_memory_device_retained(NULL));
 assert(remesa_private_memory_rebind_blocked());
 assert(memory.jobs[0].quarantined&&memory.bo[0].quarantine&&source_poison_calls==1);
 before.file=NULL;before.private_device_lost=true;before.jobs[0].quarantined=true;
 before.jobs[0].state=REMESA_JOB_QUARANTINED;before.bo[0].quarantine=true;
 assert(!memcmp(&memory,&before,sizeof memory));
 trusted_before.file=NULL;trusted_before.quarantined=true;
 assert(!memcmp(&stream,&trusted_before,sizeof stream));
 remesa_private_stream_device_lost(&dev);
 remesa_private_stream_file_lost(&dev,&file);
 assert(source_poison_calls==1&&!cleanup_calls&&!rank);
 assert(remesa_gem_memory_close(&memory)==-ENODEV&&!cleanup_calls);
 /* Bridge may already lack its owner while endpoint/trusted state survives. */
 fixture(true);owner=NULL;owner_device=NULL;
 remesa_private_stream_device_lost(&dev);
 assert(!memory.file&&!stream.file&&memory.private_device_lost&&stream.quarantined);
 assert(source_poison_calls==1&&!cleanup_calls);
 /* A normally closed stream retains stale dev/file fields. */
 fixture(false);stream.claimed=false;
 remesa_private_stream_device_lost(&dev);
 assert(!source_poison_calls&&!stream.quarantined&&!stream.file);
 /* Native postclose retains file identities on an issued/quarantined stream.
  * The actual follow-up helper removes them without releasing ownership. */
 fixture(true);bridge.state=STREAM_ISSUED;bridge.ops.close=modeled_close;bridge.ops.quarantine=modeled_quarantine;
 remesa_stream_postclose(&dev,&file);
 assert(owner==&file&&owner_device==&dev&&stream.file==&file&&!memory.file);
 assert(native_quarantines==1&&!native_closes&&permanently_quarantined&&!cleanup_calls);
 remesa_private_stream_file_lost(&dev,&file);
 assert(!owner&&!owner_device&&!stream.file&&stream.claimed&&stream.quarantined);
 assert(memory.device_refs==1&&memory.module_refs==1&&memory.power_refs==1&&!cleanup_calls);
 /* A failed close from IDLE must retain backing and drop file identities too. */
 fixture(true);bridge.state=STREAM_IDLE;bridge.ops.close=modeled_close;bridge.ops.quarantine=modeled_quarantine;native_close_error=-EIO;
 remesa_stream_postclose(&dev,&file);
 assert(native_closes==1&&native_quarantines==1&&owner==&file&&stream.file==&file&&!memory.file);
 remesa_private_stream_file_lost(&dev,&file);
 assert(active==&bridge&&!owner&&!owner_device&&!stream.file&&stream.claimed&&stream.quarantined);
 assert(memory.device_refs==1&&memory.module_refs==1&&memory.power_refs==1&&!cleanup_calls);
 /* A successful native close leaves only stale trusted metadata. Clearing
  * that identity must not poison or quarantine a released source. */
 fixture(false);bridge.state=STREAM_IDLE;bridge.ops.close=modeled_close;bridge.ops.quarantine=modeled_quarantine;stream.op=NULL;
 remesa_stream_postclose(&dev,&file);
 assert(!active&&!owner&&!owner_device&&!endpoint&&!stream.claimed&&stream.file==&file);
 remesa_private_stream_file_lost(&dev,&file);
 assert(!stream.file&&!stream.quarantined&&!source_poison_calls&&!native_quarantines&&native_closes==1);
 fixture(false);memory.private_session=&memory;memory.private_matrix_constructing=true;
 remesa_private_memory_file_lost(&dev,&file);
 assert(!memory.private_matrix_constructing&&memory.private_session==&memory&&memory.private_device_lost);
 assert(remesa_gem_memory_close(&memory)==-ENODEV&&!cleanup_calls);
 puts("PASS actual file-lost fragments retain handles/views/refs/cookies, revoke CPU construction, clear file identities, quarantine without backend/cleanup callbacks, deny late close even empty endpoint; mutex/backing modeled, GPU0");
}
