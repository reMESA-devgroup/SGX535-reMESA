#define REMESA_STREAM_SOURCE_TEST
#include "../module/remesa_stream_source.c"
static unsigned legacy_calls;
static struct task_struct main_task={1},other_task={2};
struct task_struct *current=&main_task;
int sgx535_provenance_source_begin(struct sgx535_gma500_fixed_backend *,
    struct drm_device *) {legacy_calls++;return 1;}
int sgx535_provenance_source_boundary(struct sgx535_gma500_fixed_backend *,
    const struct sgx535_source_facts *) {legacy_calls++;return 1;}
void sgx535_provenance_source_producer(int) {legacy_calls++;}
void sgx535_provenance_source_attach(struct drm_device *, struct module *,
    sgx535_source_reader) {legacy_calls++;}
void sgx535_provenance_source_observe(struct drm_device *, sgx535_source_snapshot_fn) {legacy_calls++;}
void sgx535_provenance_source_startup(struct drm_device *,
    const struct sgx535_source_facts *) {legacy_calls++;}
void sgx535_provenance_source_reset_begin(struct drm_device *) {legacy_calls++;}
void sgx535_provenance_source_reset_assert(struct drm_device *, u32) {legacy_calls++;}
void sgx535_provenance_source_reset_end(struct drm_device *,
    const struct sgx535_source_facts *) {legacy_calls++;}
void sgx535_provenance_source_lifecycle(struct drm_device *) {legacy_calls++;}
void sgx535_provenance_source_pending(struct drm_device *, u32, u32) {legacy_calls++;}
int sgx535_provenance_source_write(struct sgx535_gma500_fixed_backend *) {legacy_calls++;return 1;}
void sgx535_provenance_admit(struct sgx535_gma500_fixed_backend *) {legacy_calls++;}
void sgx535_provenance_service(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int) {legacy_calls++;}
void sgx535_provenance_issued(struct sgx535_gma500_fixed_backend *) {legacy_calls++;}
void sgx535_provenance_sample(struct sgx535_gma500_fixed_backend *, u32,
    u32, u32, int) {legacy_calls++;}
void sgx535_provenance_before(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, u32, u32, u32, int) {legacy_calls++;}
void sgx535_provenance_after(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *) {legacy_calls++;}
void sgx535_provenance_terminal(struct sgx535_gma500_fixed_backend *,
    struct sgx535_fixed_service *, int) {legacy_calls++;}

static void reader(struct drm_device *d) {(void)d;}
int main(int argc,char **argv) {
 struct sgx535_source_facts facts={.sample_valid=1};struct drm_device dev={&facts};struct module mod={0};
 struct sgx535_gma500_owner owner={0};struct sgx535_gma500_fixed_backend b={0};
 struct remesa_stream_observation first={0},second={0};unsigned before;int lease_token=1;
 const char *scenario=argc>1?argv[1]:"twoframes";
 remesa_route_source_attach(&dev,&mod,reader);remesa_route_source_startup(&dev,&facts);
 remesa_route_source_reset_begin(&dev);remesa_route_source_reset_assert(&dev,0x7f);
 remesa_route_source_reset_end(&dev,&facts);
 if(!strcmp(scenario,"pending")) {remesa_route_source_pending(&dev,1,0);assert(remesa_stream_source_claim(&dev,&lease_token));return 0;}
 assert(!remesa_stream_source_claim(&dev,&lease_token));before=legacy_calls;
 for(unsigned frame=0;frame<2;frame++) {
  struct remesa_stream_observation *o=frame?&second:&first;
  memset(&owner,0,sizeof(owner));memset(&b,0,sizeof(b));
  owner.dev=&dev;owner.initialized=true;owner.device_claimed=true;
  owner.session.phase=SGX535_PHASE_HOST_IMAGE_FINAL;
  owner.fixed_service.session=&owner.session;b.owner=&owner;
  assert(!remesa_stream_source_bind(&lease_token,o,&b,frame+1,frame+1));
  assert(remesa_route_source_begin(&b,&dev));assert(remesa_route_source_boundary(&b,&facts));
  b.active=true;b.power_held=true;remesa_route_admit(&b);
  remesa_route_service(&b,&owner.fixed_service,1);
  remesa_route_source_pending(&dev,0,1);assert(remesa_route_source_write(&b)); /* owned initialization */
  remesa_route_issued(&b);
  if(!strcmp(scenario,"overflow")){for(unsigned i=0;i<65;i++)remesa_route_source_pending(&dev,1,0);assert(!remesa_route_source_write(&b));return 0;}
  if(!strcmp(scenario,"wrongseq")){remesa_route_sample(&b,frame+2,1,0,1);assert(!remesa_route_source_write(&b));return 0;}
  if(!strcmp(scenario,"producer")){remesa_route_source_producer(1);assert(!remesa_route_source_write(&b));assert(legacy_calls==before);return 0;}
  if(!strcmp(scenario,"wrongtask")){current=&other_task;assert(!remesa_route_source_write(&b));return 0;}
  remesa_route_source_pending(&dev,1,0);assert(remesa_route_source_write(&b));
  remesa_route_sample(&b,frame+1,1,0,1);remesa_route_before(&b,&owner.fixed_service,frame+1,1,0,1);
  owner.session.phase=SGX535_PHASE_RETIRED;owner.session.observed_events=7;
  remesa_route_after(&b,&owner.fixed_service);remesa_route_terminal(&b,&owner.fixed_service,0);
  if(!strcmp(scenario,"terminalpending")){remesa_route_source_pending(&dev,1,0);assert(remesa_stream_source_finish(&lease_token,o,(char[4096]){0}));return 0;}
  if(!strcmp(scenario,"nohash")) {assert(!remesa_stream_source_finish_nohash(&lease_token,o));assert(!o->evidence.color_present);assert(o->evidence.state==SGX535_CAP_CLOSED);}
  else if(!strcmp(scenario,"nohash_wrongledger")){owner.session.observed_events=3;assert(remesa_stream_source_finish_nohash(&lease_token,o));return 0;}
  else if(!strcmp(scenario,"nohash_wrongphase")){owner.session.phase=8;assert(remesa_stream_source_finish_nohash(&lease_token,o));return 0;}
  else if(!strcmp(scenario,"nohash_pending")){remesa_route_source_pending(&dev,1,0);assert(remesa_stream_source_finish_nohash(&lease_token,o));return 0;}
  else {unsigned char color[4096]={0};assert(!remesa_stream_source_finish(&lease_token,o,color));}
  b.active=false;assert(!remesa_stream_source_unbind(&lease_token,o));
  if(!strcmp(scenario,"latepending")){remesa_route_source_pending(&dev,1,0);assert(remesa_stream_source_settled(&lease_token));return 0;}
  assert(!remesa_stream_source_settled(&lease_token));
 }
 assert(legacy_calls==before);assert(!remesa_stream_source_release(&lease_token));
 assert(remesa_stream_source_claim(&dev,&lease_token)); /* no session rearm */
 before=legacy_calls;
 assert(!remesa_route_source_begin(&b,&dev));
 assert(!remesa_route_source_boundary(&b,&facts));
 assert(!remesa_route_source_write(&b));
 remesa_route_admit(&b);remesa_route_issued(&b);
 assert(legacy_calls==before); /* closed stream cannot re-enable legacy */
 puts("independent source two distinct operations, legacy untouched: PASS");return 0;
}
