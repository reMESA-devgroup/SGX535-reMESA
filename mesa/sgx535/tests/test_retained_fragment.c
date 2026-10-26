/* SPDX-License-Identifier: MIT
 * Actual Gallium/NIR -> production Linux token/staging/retained submit.
 * ALL device calls are interposed. Completion/capture/pixels below are explicit
 * fixtures, never hardware evidence. Opaque retained carrier input comes only
 * from stdin and is compared, not embedded or executed by this test.
 */
#include "remesa_linux.h"
#include "remesa_linux_memory.h"
#include "remesa_screen.h"
#include "remesa_shader_program.h"
#include "remesa_fragment_body.h"
#include "nir_fixture.h"
#include "vertex_nir_fixture.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "gallium/auxiliary/util/u_upload_mgr.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,created,retained_calls,records,auth_calls,use_writes;
static bool vertex_case,vertex_bad,arrays_case;
static unsigned compared_images;
static unsigned post_info_mask,post_program_reads,post_geometry_reads;
static bool authenticating;
static FILE *memory_journal;
static const char *capture_path;
static uint64_t journal_records,submit_begin,submit_end;
static struct remesa_device *active_device;
static const char *scenario;
static struct object {size_t size;uint64_t va;unsigned char *data;bool live,pinned,leased,write;} objects[32];
static unsigned char *baseline[6];
static const size_t sizes[10]={0x20000,0x8000,0x1000,0x1000,0x1000,0x1000,0x2000,0x2000000,0x2000000,0x1000};
static const uint64_t addresses[10]={0x20000000,0x20080000,0x40000000,0x30000000,0x46000000,
 0x40001000,0x40002000,0x42000000,0x31000000,0x46001000};
#define CHECK(c) do {assert(c);++checks;} while(0)
int __wrap_ioctl(int,unsigned long,...);
int __wrap_open(const char *,int,...);
int __wrap_open64(const char *,int,...);
int __wrap_close(int);
int __wrap_open(const char *path,int flags,...)
{(void)flags;CHECK(!strcmp(path,"INTERPOSED-NO-DEVICE"));return 100;}
int __wrap_open64(const char *p,int f,...){return __wrap_open(p,f);}
int __wrap_close(int fd){CHECK(fd==100);return 0;}
int __wrap_ioctl(int fd,unsigned long command,...)
{
 va_list ap;va_start(ap,command);void *arg=va_arg(ap,void *);va_end(ap);
 if(command==DRM_IOCTL_VERSION){CHECK(fd==100);struct drm_version *v=arg;memcpy(v->name,"gma500",6);v->name_len=6;return 0;}
 CHECK(fd==99); /* No native draw ioctl or second SGX endpoint is allowed. */
 if(command==DRM_IOCTL_REMESA_RETAINED) {
  struct remesa_retained_request *r=arg;CHECK(++retained_calls==1 && r->context==1234);
  CHECK(use_writes>0 && records==1);
  for(unsigned i=0;i<10;i++)CHECK(r->bindings[i].id==32+i&&r->bindings[i].va==addresses[i]&&r->bindings[i].bytes==sizes[i]);
  CHECK(!memcmp(r->commands,baseline[4]+208,56)&&!memcmp(r->commands+56,baseline[4],208));
  for(unsigned i=0;i<6;i++) {
   if(i==1&&!strcmp(scenario,"green-candidate")) {
    struct remesa_fragment_constant fs;
    CHECK(!remesa_fragment_constant_decode(objects[i].data,8,&fs)&&fs.argb==UINT32_C(0xff00ff00));
    CHECK(memcmp(objects[i].data,baseline[i],8));
    CHECK(!memcmp(objects[i].data+8,baseline[i]+8,sizes[i]-8));
    unsigned differences=0;
    for(unsigned j=0;j<8;j++)if(objects[i].data[j]!=baseline[i][j]) {
     CHECK(j==0||j==1||j==2||j==4);differences++;
    }
    CHECK(differences==4);
   } else CHECK(!memcmp(objects[i].data,baseline[i],sizes[i]));
   compared_images++;
  }
  if(!strcmp(scenario,"ioctl-fail")){errno=EIO;return -1;}
  r->operation=1;r->serial=1;r->generation=1;r->state=6;r->admitted=1;
  r->observer_request_bytes=1;r->observer_request[0]=0xa1;
  r->observer_response_bytes=4140;
  /* Explicit unrelated fixture data, NOT a triangle simulation/reference. */
  memset(objects[5].data,0x7b,4096);memset(r->observer_response+44,0x7b,4096);
  if(!strcmp(scenario,"color-mismatch"))r->observer_response[44]^=1;
  if(!strcmp(scenario,"capture-post-source"))objects[10].data[0]^=1;
  return 0;
 }
 CHECK(command==DRM_IOCTL_REMESA_MEMORY);
 struct remesa_memory_request *r=arg;
 CHECK(r->version==1 && !r->reserved);
 if(r->action==REMESA_MEM_OPEN){CHECK(!r->context);r->context=1234;return 0;}
 CHECK(r->context==1234);
 if(r->action==REMESA_MEM_CREATE) {
  CHECK(created<32 && !r->id);unsigned n=created++;
  objects[n].size=(size_t)r->bytes;objects[n].data=calloc(1,(size_t)r->bytes);
  CHECK(objects[n].data);objects[n].live=true;r->id=32+n;return 0;
 }
 if(r->action==REMESA_MEM_CLOSE){for(unsigned i=0;i<created;i++)CHECK(!objects[i].live);return 0;}
 CHECK(r->id>=32 && r->id<32+created);
 unsigned slot=r->id-32;struct object *b=&objects[slot];CHECK(b->live);
 switch(r->action) {
 case REMESA_MEM_PIN:CHECK(!b->pinned);b->pinned=true;return 0;
 case REMESA_MEM_UNPIN:CHECK(b->pinned&&!b->leased);b->pinned=false;return 0;
 case REMESA_MEM_MAP:CHECK(b->pinned&&!b->va);b->va=r->va;return 0;
 case REMESA_MEM_UNMAP:CHECK(b->va&&!b->leased);b->va=0;return 0;
 case REMESA_MEM_DESTROY:CHECK(!b->pinned&&!b->va&&!b->leased);b->live=false;free(b->data);b->data=NULL;return 0;
 case REMESA_MEM_LEASE:CHECK(b->pinned&&!b->leased);b->leased=true;b->write=r->flags!=0;return 0;
 case REMESA_MEM_UNLEASE:CHECK(b->leased);b->leased=false;b->write=false;return 0;
 case REMESA_MEM_READ:case REMESA_MEM_WRITE:
  CHECK(b->leased && r->offset<=b->size && r->length<=b->size-r->offset);
  if(r->action==REMESA_MEM_READ)memcpy(r->data,b->data+(size_t)r->offset,r->length);
  else {
   CHECK(b->write);
   if(slot==1&&created>10){use_writes++;if(!strcmp(scenario,"write-fail")){errno=EIO;return -1;}}
   memcpy(b->data+(size_t)r->offset,r->data,r->length);
   if(slot==10&&!strcmp(scenario,"source-corrupt"))b->data[0]^=1;
  }
  return 0;
 case REMESA_MEM_INFO: {
  struct remesa_memory_info info={0};
  for(unsigned i=0;i<created;i++)if(objects[i].live) {
   info.objects++;info.allocated_bytes+=objects[i].size;
   info.pinned_objects+=objects[i].pinned;
   if(objects[i].va)info.mapped_pages+=(uint32_t)(objects[i].size/4096);
   info.leases+=objects[i].leased;
  }
  info.object_bytes=b->size;info.gpu_va=b->va;info.object_pinned=b->pinned;
  info.object_lease=b->leased;
  info.object_mapped_pages=info.verified_pages=b->va?(uint32_t)(b->size/4096):0;
  if(created>10&&!strcmp(scenario,"capture-inventory"))info.objects++;
  if(created>10&&slot==1&&!strcmp(scenario,"info-stale"))info.gpu_va+=4096;
  if(retained_calls&&slot==1&&!strcmp(scenario,"capture-post-info"))info.gpu_va+=4096;
  memcpy(r->data,&info,sizeof(info));return 0;
 }
 default:CHECK(!"unexpected memory operation");return -1;
 }
}
static int memory_record(void *p,const struct remesa_memory_request *before,
 const struct remesa_memory_request *after,int result,int error,bool entering)
{
 (void)p;CHECK(!authenticating);
 if(memory_journal) {
  /* Same raw request/response journal layout as the retained probe. */
  struct {unsigned before;int rc,error;unsigned reserved;
   struct remesa_memory_request request,response;} raw={0};
  _Static_assert(sizeof(raw)==8320,"native/i386 raw memory journal");
  raw.before=entering;raw.rc=result;raw.error=error;raw.request=*before;
  if(after)raw.response=*after;
  CHECK(fwrite(&raw,sizeof(raw),1,memory_journal)==1);journal_records++;
 }
 if(!entering&&after&&!result&&!error&&records==2) {
  if(before->action==REMESA_MEM_INFO) {
   CHECK(before->id>=32&&before->id<44);
   post_info_mask|=1u<<(before->id-32);
  }
  if(before->action==REMESA_MEM_READ&&before->id==42)post_program_reads++;
  if(before->action==REMESA_MEM_READ&&before->id==43)post_geometry_reads++;
 }
 return 0;
}
static int record(void *p,const struct remesa_retained_request *before,
 const struct remesa_retained_request *after,int result,int error)
{(void)p;(void)result;(void)error;CHECK(before->context==1234);records++;
 if(after){CHECK(records==2);submit_end=journal_records;}
 else submit_begin=journal_records;
 return 0;}
static int authenticate(void *p,const struct remesa_retained_request *r,struct remesa_linux_fragment_evidence *out)
{
 (void)p;auth_calls++;if(!strcmp(scenario,"auth-fail"))return EACCES;
 CHECK(remesa_linux_close_checked(active_device)==EBUSY);
 memset(out,0,sizeof(*out));
 struct remesa_evidence_expected *e=&out->expected;
 struct remesa_evidence_response *s=&out->response;
 struct remesa_evidence_observer *o=&out->observer;
 struct remesa_evidence_ownership *g=&out->ownership;
 e->id=(struct remesa_evidence_id){r->context,r->operation,r->serial,r->slot,r->generation};
 memset(e->boot,1,16);memset(e->source,2,32);memset(e->request_sha256,3,32);
 e->request_bytes=500;e->resources=10;s->id=g->id=e->id;
 memcpy(s->request_sha256,e->request_sha256,32);memcpy(o->request_sha256,e->request_sha256,32);
 memset(s->response_sha256,4,32);memcpy(o->response_sha256,s->response_sha256,32);
 memcpy(o->boot,e->boot,16);memcpy(o->source,e->source,32);
 s->request_bytes=o->request_bytes=500;s->response_bytes=o->response_bytes=4140;
 s->records=s->attempted=s->ta_complete=s->end_render=s->memory_free=s->retired=s->visible=s->acknowledged=1;
 o->native_records=o->capsule_records=o->source_records=1;
 o->request_present=o->response_present=o->closed=o->syscall_exit_seen=o->source_valid=1;
 o->ta_complete=o->end_render=o->memory_free=o->retired=1;
 g->records=g->pfn_valid=g->mappings_valid=g->held_through_completion=1;
 g->visibility_ordered=g->detached=g->released=1;g->resources=10;
 return 0; /* TEST-ONLY synthetic normalized evidence. */
}
static int authenticate_fragment(void *p,const struct remesa_retained_request *r,
 const struct remesa_linux_fragment_capture *capture,
 struct remesa_linux_fragment_evidence *out)
{
 CHECK(post_info_mask==0xfff&&post_program_reads&&post_geometry_reads);
 CHECK(capture->owner==r->context&&capture->generation);
 CHECK(capture->program_handle>=42&&capture->geometry_handle>=42&&
       capture->program_handle!=capture->geometry_handle);
 CHECK(capture->program_bytes==4096&&capture->geometry_bytes==4096);
 CHECK(capture->source_offset<=capture->program_bytes-8);
 if(!strcmp(scenario,"green-candidate")) {
  struct remesa_fragment_constant fs;
  CHECK(capture->argb==UINT32_C(0xff00ff00));
  CHECK(!remesa_fragment_constant_decode(capture->source,8,&fs)&&fs.argb==capture->argb);
  CHECK(memcmp(capture->source,baseline[1],8));
 } else CHECK(!memcmp(capture->source,baseline[1],8)&&capture->argb==UINT32_C(0xffff00ff));
 CHECK(capture->program_changed);
 if(!strcmp(scenario,"green-candidate"))CHECK(!memcmp(capture->preimage,baseline[1],8));
 else for(unsigned i=0;i<8;i++)CHECK(capture->preimage[i]==0xa5);
 CHECK(objects[capture->program_handle-32].live&&objects[capture->geometry_handle-32].live);
 CHECK(objects[capture->program_handle-32].pinned&&objects[capture->geometry_handle-32].pinned);
 CHECK(!objects[capture->program_handle-32].va&&!objects[capture->geometry_handle-32].va);
 if(arrays_case) {
  /* The draw source has no index BO. Its immutable geometry snapshot
   * contains only the selected 48-byte vertex source and zero padding. */
  const unsigned char *g=objects[capture->geometry_handle-32].data;
  for(unsigned n=48;n<4096;n++)CHECK(g[n]==0);
 }
 if(vertex_case) {
  CHECK(capture->vertex_staged&&capture->vertex_source_handle==capture->program_handle&&capture->vertex_use_offset==0x40);
  CHECK(!memcmp(capture->vertex_source,objects[1].data+0x40,16));
  CHECK(!memcmp(capture->vertex_source,objects[capture->program_handle-32].data+capture->vertex_source_offset,16));
  CHECK(!memcmp(capture->vertex_source,capture->vertex_preimage,16));
 }
 if(capture_path) {
  CHECK(memory_journal&&fflush(memory_journal)==0);
  FILE *file=fopen(capture_path,"wx");CHECK(file);
  CHECK(fprintf(file,"{\"scope\":\"INTERPOSED_CPU_ONLY\",\"submit_begin\":%llu,\"submit_end\":%llu,\"capture_end\":%llu,\"capture\":{\"owner\":%llu,\"generation\":%llu,\"program_handle\":%u,\"geometry_handle\":%u,\"source_offset\":%u,\"argb\":%u,\"program_bytes\":%llu,\"geometry_bytes\":%llu,\"program_changed\":true,\"source\":\"",
   (unsigned long long)submit_begin,(unsigned long long)submit_end,
   (unsigned long long)journal_records,(unsigned long long)capture->owner,
   (unsigned long long)capture->generation,capture->program_handle,
   capture->geometry_handle,capture->source_offset,capture->argb,
   (unsigned long long)capture->program_bytes,(unsigned long long)capture->geometry_bytes)>0);
  for(unsigned i=0;i<8;i++)CHECK(fprintf(file,"%02x",capture->source[i])==2);
  CHECK(fputs("\",\"preimage\":\"",file)>=0);
  for(unsigned i=0;i<8;i++)CHECK(fprintf(file,"%02x",capture->preimage[i])==2);
  CHECK(fputc('"',file)!=EOF);
  if(capture->vertex_staged) {
   CHECK(fprintf(file,",\"vertex_staged\":true,\"vertex_source_handle\":%u,\"vertex_source_offset\":%u,\"vertex_use_offset\":%u,\"vertex_source\":\"",
    capture->vertex_source_handle,capture->vertex_source_offset,capture->vertex_use_offset)>0);
   for(unsigned i=0;i<16;i++)CHECK(fprintf(file,"%02x",capture->vertex_source[i])==2);
   CHECK(fputs("\",\"vertex_preimage\":\"",file)>=0);
   for(unsigned i=0;i<16;i++)CHECK(fprintf(file,"%02x",capture->vertex_preimage[i])==2);
   CHECK(fputc('"',file)!=EOF);
  }
  CHECK(fputs("},\"roles\":[",file)>=0);
  for(unsigned i=0;i<10;i++) {
   const struct remesa_retained_binding *b=&r->bindings[i];
   CHECK(fprintf(file,"%s{\"id\":%u,\"va\":%llu,\"bytes\":%llu}",i?",":"",
    b->id,(unsigned long long)b->va,(unsigned long long)b->bytes)>0);
  }
  CHECK(fputs("]}\n",file)>=0);CHECK(!fclose(file));
  CHECK(!fclose(memory_journal));memory_journal=NULL;
 }
 authenticating=true;
 int error=authenticate(p,r,out); /* Synthetic; no physical authentication. */
 authenticating=false;return error;
}
static struct pipe_resource *buffer(struct pipe_screen *s,unsigned bytes,unsigned bind)
{struct pipe_resource t={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,.width0=bytes,.height0=1,.depth0=1,.array_size=1,.bind=bind};return s->resource_create(s,&t);}
static void upload(struct pipe_context *c,struct pipe_resource *r,const void *p,unsigned bytes)
{struct pipe_box box={.width=(int)bytes,.height=1,.depth=1};struct pipe_transfer *t=NULL;void *m=c->buffer_map(c,r,0,PIPE_MAP_WRITE,&box,&t);CHECK(m);memcpy(m,p,bytes);c->buffer_unmap(c,t);}
static void drop(struct pipe_resource *r){if(!__atomic_sub_fetch(&r->reference.count,1,__ATOMIC_ACQ_REL))r->screen->resource_destroy(r->screen,r);}
int main(int argc,char **argv)
{
 CHECK(argc==2||argc==4);scenario=argv[1];
 bool gl_quarter=!strcmp(scenario,"gl-quarter")||!strcmp(scenario,"gl-quarter-wrong-geometry");
 bool gl_wrong_geometry=!strcmp(scenario,"gl-quarter-wrong-geometry");
 bool preserve_case=!strcmp(scenario,"arrays-preserve");
 arrays_case=!strcmp(scenario,"arrays-candidate")||!strcmp(scenario,"arrays-preimage")||preserve_case||gl_quarter;
 vertex_case=!strcmp(scenario,"vertex-candidate")||!strcmp(scenario,"vertex-preimage")||arrays_case;
 vertex_bad=!strcmp(scenario,"vertex-preimage")||!strcmp(scenario,"arrays-preimage");
 bool uploaded=!strcmp(scenario,"upload-green")||!strcmp(scenario,"upload-index")||!strcmp(scenario,"upload-position");
 bool bad_index=!strcmp(scenario,"upload-index"),bad_position=!strcmp(scenario,"upload-position");
 const char *requested=scenario;
 if(uploaded||vertex_case)scenario="green-candidate";
 if(argc==4) {
  CHECK(!strcmp(scenario,"green-candidate"));
  memory_journal=fopen(argv[2],"wbx");CHECK(memory_journal);capture_path=argv[3];
 }
 struct remesa_device d={0};struct remesa_linux_memory *m=NULL;
 active_device=&d;
 CHECK(!remesa_linux_open(&d,"INTERPOSED-NO-DEVICE"));
 const struct remesa_memory_recorder recorder={.record=memory_record};
 CHECK(!remesa_linux_memory_begin_recorded(99,&m,&recorder));
 const struct remesa_persistent_ops *ops=remesa_linux_memory_ops();
 struct remesa_retained_request job={.version=1,.context=1234};
 for(unsigned i=0;i<10;i++) {
  uint32_t id=0;CHECK(!ops->create(m,sizes[i],&id));CHECK(!ops->pin(m,id));
  CHECK(!ops->gpu_map(m,id,addresses[i],sizes[i]));
  job.bindings[i]=(struct remesa_retained_binding){id,0,addresses[i],sizes[i]};
  if(i<6) {
   baseline[i]=malloc(sizes[i]);CHECK(baseline[i]);CHECK(fread(baseline[i],1,sizes[i],stdin)==sizes[i]);
   if(i==1&&vertex_case) {
    nir_shader *n=vertex_fixture();struct remesa_shader_ir ir;struct remesa_shader_program v;
    CHECK(!remesa_shader_ir_lower(n,&ir)&&!remesa_shader_program_prepare(&ir,&v));ralloc_free(n);
    struct remesa_usse_vertex_mapping mapping={0};
    for(unsigned c=0;c<4;c++){mapping.input[c]=(struct remesa_usse_cpu_register){2,c};mapping.output[c]=(struct remesa_usse_cpu_register){1,c};}
    size_t length=0;CHECK(!remesa_usse_vertex_candidate(&v.usse,&mapping,baseline[i]+0x40,16,&length)&&length==16);
    if(vertex_bad)baseline[i][0x40]^=1;
   }
   void *ptr=NULL;CHECK(!ops->cpu_map(m,id,true,&ptr));memcpy(ptr,baseline[i],sizes[i]);
   if(i==1&&strcmp(scenario,"unchanged")&&strcmp(scenario,"green-candidate"))memset(ptr,0xa5,8);
   CHECK(!ops->cpu_unmap(m,id,true));
   CHECK(!ops->cpu_map(m,id,false,&ptr));
   CHECK(!memcmp(ptr,objects[i].data,sizes[i]));
   CHECK(!ops->cpu_unmap(m,id,false));
  }
 }
 CHECK(fgetc(stdin)==EOF);
 /* The exact retained CONTROL layout stores raster then TA; the internal
  * command snapshot is TA then raster. No instruction bytes are defined. */
 memcpy(job.commands,baseline[4]+208,56);memcpy(job.commands+56,baseline[4],208);
 struct remesa_persistent_config cfg={.ops=ops,.priv=m,.context=1234,
  .va_start=0x23000000,.va_end=0x23010000,.byte_budget=16384,.resource_only_backend=true};
 CHECK(!remesa_linux_enable_shader_staging(&d,&cfg));
 struct remesa_linux_fragment_carrier carrier={.memory=m,.job=&job,
  .record=record,.authenticate=authenticate};
 struct remesa_linux_fragment_carrier invalid=carrier;invalid.authenticate=NULL;
 CHECK(remesa_linux_enable_retained_fragment(&d,&invalid)==EINVAL&&!d.retained_fragment_carrier);
 invalid=carrier;invalid.authenticate_fragment=authenticate_fragment;
 CHECK(remesa_linux_enable_retained_fragment(&d,&invalid)==EINVAL&&!d.retained_fragment_carrier);
 if(!strcmp(scenario,"capture")||!strcmp(scenario,"capture-inventory")||!strcmp(scenario,"green-candidate")||!strcmp(scenario,"green-preimage")||
    !strcmp(scenario,"capture-post-source")||!strcmp(scenario,"capture-post-info")) {
  carrier.authenticate=NULL;carrier.authenticate_fragment=authenticate_fragment;
 }
 if(!strcmp(scenario,"unchanged"))carrier.require_program_change=true;
 if(!strcmp(scenario,"green-candidate")||!strcmp(scenario,"green-preimage")) {
  carrier.fragment_argb=UINT32_C(0xff00ff00);
  CHECK(remesa_linux_enable_retained_fragment(&d,&carrier)==ENOTSUP);
  carrier.require_program_change=true;
 }
 if(gl_quarter) {
  carrier.gl_fbo_half_pixel_center=true;
  CHECK(remesa_linux_enable_retained_fragment(&d,&carrier)==ENOTSUP&&!d.retained_gl_fbo_half_pixel_center);
 }
 if(vertex_case) {
  carrier.independent_vertex=true;
  CHECK(remesa_linux_enable_retained_fragment(&d,&carrier)==ENOTSUP);
  struct remesa_usse_vertex_mapping mapping={0};
  for(unsigned c=0;c<4;c++){mapping.input[c]=(struct remesa_usse_cpu_register){2,c};mapping.output[c]=(struct remesa_usse_cpu_register){1,c};}
  CHECK(!remesa_linux_set_vertex_candidate_mapping(&d,&mapping));
  if(gl_quarter) {
   mapping.input[0].number=4;
   CHECK(!remesa_linux_set_vertex_candidate_mapping(&d,&mapping));
   CHECK(remesa_linux_enable_retained_fragment(&d,&carrier)==ENOTSUP&&!d.retained_gl_fbo_half_pixel_center);
   mapping.input[0].number=0;
   CHECK(!remesa_linux_set_vertex_candidate_mapping(&d,&mapping));
   invalid=carrier;invalid.independent_vertex=false;
   CHECK(remesa_linux_enable_retained_fragment(&d,&invalid)==ENOTSUP&&!d.retained_gl_fbo_half_pixel_center);
   invalid=carrier;invalid.fragment_argb=UINT32_C(0xffff00ff);
   CHECK(remesa_linux_enable_retained_fragment(&d,&invalid)==ENOTSUP&&!d.retained_gl_fbo_half_pixel_center);
   invalid=carrier;invalid.require_program_change=false;
   CHECK(remesa_linux_enable_retained_fragment(&d,&invalid)==ENOTSUP&&!d.retained_gl_fbo_half_pixel_center);
  }
 }
 if(strcmp(scenario,"disabled")&&strcmp(scenario,"late"))CHECK(!remesa_linux_enable_retained_fragment(&d,&carrier));
 CHECK(d.retained_gl_fbo_half_pixel_center==gl_quarter);
 struct pipe_screen *s=remesa_screen_create(&d);CHECK(s);struct pipe_context *c=s->context_create(s,NULL,0);CHECK(c);
 if(!strcmp(scenario,"late"))CHECK(!remesa_linux_enable_retained_fragment(&d,&carrier));
 if(strcmp(scenario,"disabled"))memset(&job,0xa5,sizeof(job)); /* config owns its copy */
 if(strcmp(scenario,"disabled")&&strcmp(scenario,"late")){CHECK(strstr(s->get_name(s),"retained VS/PDS"));CHECK(!s->caps.accelerated);}
 struct pipe_resource *vb=NULL,*ib=NULL;unsigned vo=0,io=0;
 float positions[3][4]={{-.5f,-.5f,0,1},{.5f,-.5f,0,1},{-.5f,.5f,0,1}};
 if(gl_quarter&&!gl_wrong_geometry)for(unsigned v=0;v<3;v++)for(unsigned c=0;c<2;c++)positions[v][c]+=1.f/64.f;
 if(!strcmp(scenario,"shape"))positions[0][0]=-.25f;
 uint16_t indices[3]={0,1,2};
 if(bad_position)positions[0][0]=-.25f;
 if(bad_index){indices[0]=1;indices[1]=0;}
 if(uploaded) {
  u_upload_data(c->stream_uploader,16,48,16,positions,&vo,&vb);
  u_upload_data(c->stream_uploader,0,6,16,indices,&io,&ib);
  CHECK(vb&&ib&&vb==ib&&vo==16&&io==64);
  u_upload_unmap(c->stream_uploader);
 } else {
  vb=buffer(s,48,PIPE_BIND_VERTEX_BUFFER);ib=buffer(s,6,PIPE_BIND_INDEX_BUFFER);CHECK(vb&&ib);
  upload(c,vb,positions,48);upload(c,ib,indices,6);
 }
 struct pipe_resource rt={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,.width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *target=s->resource_create(s,&rt);CHECK(target);
 struct pipe_framebuffer_state fb={.width=32,.height=32,.nr_cbufs=1};fb.cbufs[0].texture=target;fb.cbufs[0].format=target->format;c->set_framebuffer_state(c,&fb);
 struct pipe_vertex_buffer binding={.buffer_offset=vo,.buffer.resource=vb};++vb->reference.count;c->set_vertex_buffers(c,1,&binding);
 struct pipe_vertex_element element={.src_format=PIPE_FORMAT_R32G32B32A32_FLOAT,.src_stride=16};
 void *ve=c->create_vertex_elements_state(c,1,&element);CHECK(ve);c->bind_vertex_elements_state(c,ve);
 struct pipe_shader_state state={.type=PIPE_SHADER_IR_NIR,.ir.nir=vertex_fixture()};
 void *vs=c->create_vs_state(c,&state);CHECK(vs);c->bind_vs_state(c,vs);
 state.ir.nir=fixture(!strcmp(scenario,"green")||!strcmp(scenario,"green-candidate")||!strcmp(scenario,"green-preimage")?0xff00ff00:0xffff00ff);
 void *fs=c->create_fs_state(c,&state);CHECK(fs);c->bind_fs_state(c,fs);
 struct pipe_blend_state blend={0};blend.rt[0].colormask=PIPE_MASK_RGBA;
 struct pipe_depth_stencil_alpha_state depth={0};
 struct pipe_rasterizer_state raster={.fill_front=PIPE_POLYGON_MODE_FILL,.fill_back=PIPE_POLYGON_MODE_FILL};
 if(gl_quarter) {
  raster.half_pixel_center=1;raster.clamp_vertex_color=1;
  struct pipe_rasterizer_state bad=raster;bad.bottom_edge_rule=1;
  CHECK(!c->create_rasterizer_state(c,&bad)&&remesa_context_error(c)==ENOTSUP);
  bad=raster;bad.cull_face=PIPE_FACE_BACK;
  CHECK(!c->create_rasterizer_state(c,&bad)&&remesa_context_error(c)==ENOTSUP);
  bad=raster;bad.multisample=1;
  CHECK(!c->create_rasterizer_state(c,&bad)&&remesa_context_error(c)==ENOTSUP);
 }
 void *bs=c->create_blend_state(c,&blend),*ds=c->create_depth_stencil_alpha_state(c,&depth),*rs=c->create_rasterizer_state(c,&raster);CHECK(bs&&ds&&rs);
 c->bind_blend_state(c,bs);c->bind_depth_stencil_alpha_state(c,ds);c->bind_rasterizer_state(c,rs);
 struct pipe_viewport_state viewport={.scale={16,16,.5f},.translate={16,16,.5f}};c->set_viewport_states(c,0,1,&viewport);
 struct pipe_draw_info info={.mode=MESA_PRIM_TRIANGLES,.index_size=2,.instance_count=1};info.index.resource=ib;
 if(arrays_case){info.index_size=0;info.index.resource=NULL;}
 struct pipe_draw_start_count_bias draw={.start=io/2,.count=3};CHECK(!remesa_context_error(c));
 if(preserve_case) {
  union pipe_color_union black={.f={0,0,0,1}};
  c->clear(c,PIPE_CLEAR_COLOR0,NULL,&black,1.0,0);CHECK(!remesa_context_error(c));
 }
 c->draw_vbo(c,&info,0,NULL,&draw,1);
 int error=remesa_context_error(c);
 if(uploaded)fprintf(stderr,"OFFLINE uploader: vertex_byte_offset=%u index_byte_offset=%u errno=%d retained_calls=%u\n",vo,io,error,retained_calls);
 struct remesa_linux_fragment_receipt receipt={0};
 int receipt_error=remesa_linux_last_retained_fragment(&d,&receipt);
 bool good=!strcmp(scenario,"good")||!strcmp(scenario,"capture")||!strcmp(scenario,"green-candidate");
 if(bad_index||bad_position||vertex_bad||preserve_case||gl_wrong_geometry)good=false;
 if(good) {
  CHECK(!error && retained_calls==1 && records==2 && auth_calls==1);
  CHECK(!receipt_error&&receipt.staged&&receipt.outside_fragment_unchanged&&receipt.authenticated&&!receipt.quarantined&&receipt.program_changed);
  CHECK(!memcmp(receipt.source,objects[1].data,8));
  if(vertex_case) {
   CHECK(receipt.vertex_staged&&receipt.vertex_source_handle>=42&&receipt.vertex_use_offset==0x40);
   CHECK(!memcmp(receipt.vertex_source,objects[1].data+0x40,16));
   CHECK(!memcmp(receipt.vertex_preimage,receipt.vertex_source,16));
  } else CHECK(!receipt.vertex_staged);
  if(!strcmp(scenario,"green-candidate"))CHECK(memcmp(receipt.source,baseline[1],8));
  if(!strcmp(scenario,"green-candidate"))CHECK(!memcmp(receipt.preimage,baseline[1],8));
  else for(unsigned i=0;i<8;i++)CHECK(receipt.preimage[i]==0xa5);
  struct pipe_fence_handle *f=NULL;c->flush(c,&f,0);CHECK(f&&s->fence_finish(s,c,f,0));
  struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *t=NULL;
  const unsigned char *ptr=c->texture_map(c,target,0,PIPE_MAP_READ,&box,&t);CHECK(ptr);
  for(unsigned i=0;i<4096;i++)CHECK(ptr[i]==0x7b);
  c->texture_unmap(c,t);s->fence_reference(s,&f,NULL);
 } else {
  CHECK(error);
  if(preserve_case) {
   CHECK(error==ENOTSUP&&!retained_calls&&!auth_calls&&!use_writes);
   struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *t=NULL;
   const unsigned char *p=c->texture_map(c,target,0,PIPE_MAP_READ,&box,&t);CHECK(p&&t);
   for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++) {
    uint32_t pixel;memcpy(&pixel,p+y*t->stride+x*4,4);CHECK(pixel==UINT32_C(0xff000000));
   }
   c->texture_unmap(c,t);
  }
  if(vertex_bad)CHECK(error==ESTALE&&!retained_calls&&!auth_calls&&!use_writes&&!receipt.vertex_staged);
  if(bad_index||bad_position||gl_wrong_geometry)CHECK(error==ENOTSUP&&!retained_calls&&!auth_calls&&!use_writes);
  if(!strcmp(scenario,"disabled")||!strcmp(scenario,"late")||!strcmp(scenario,"green")||!strcmp(scenario,"shape")||!strcmp(scenario,"source-corrupt"))CHECK(!retained_calls);
  if(!strcmp(scenario,"unchanged")) {
   CHECK(error==ENOTSUP&&!retained_calls&&!auth_calls&&!use_writes);
   CHECK(!receipt_error&&!receipt.program_changed&&!receipt.staged&&!receipt.quarantined);
   CHECK(!memcmp(objects[1].data,baseline[1],sizes[1]));
  }
  if(!strcmp(scenario,"green-preimage")) {
   CHECK(error==ESTALE&&!retained_calls&&!auth_calls&&!use_writes);
   CHECK(!receipt_error&&!receipt.staged&&!receipt.quarantined);
   for(unsigned i=0;i<8;i++)CHECK(objects[1].data[i]==0xa5);
   CHECK(!memcmp(objects[1].data+8,baseline[1]+8,sizes[1]-8));
  }
  if(!strcmp(scenario,"capture-inventory"))CHECK(!retained_calls&&!auth_calls);
  if(!strcmp(scenario,"capture-post-source")||!strcmp(scenario,"capture-post-info")) {
   CHECK(retained_calls==1&&records==2&&!auth_calls);
   CHECK(!receipt_error&&receipt.quarantined&&!receipt.authenticated);
   struct pipe_box box={.width=32,.height=32,.depth=1};struct pipe_transfer *t=NULL;
   CHECK(!c->texture_map(c,target,0,PIPE_MAP_READ,&box,&t));
  }
  if(!strcmp(scenario,"auth-fail")||!strcmp(scenario,"color-mismatch"))CHECK(retained_calls==1&&auth_calls==1&&receipt.quarantined&&!receipt.authenticated);
 }
 c->bind_fs_state(c,NULL);c->delete_fs_state(c,fs);c->bind_vs_state(c,NULL);c->delete_vs_state(c,vs);
 c->bind_vertex_elements_state(c,NULL);c->delete_vertex_elements_state(c,ve);
 c->bind_blend_state(c,NULL);c->delete_blend_state(c,bs);c->bind_depth_stencil_alpha_state(c,NULL);c->delete_depth_stencil_alpha_state(c,ds);c->bind_rasterizer_state(c,NULL);c->delete_rasterizer_state(c,rs);
 c->destroy(c);drop(vb);drop(ib);drop(target);s->destroy(s);
 if(receipt.quarantined) {
  CHECK(remesa_linux_close_checked(&d)==EBUSY);
  /* Context teardown must leave the original source inventory owned even
   * after the Gallium token is abandoned on an uncertain retained attempt. */
  CHECK(created==12);
  for(unsigned i=10;i<12;i++)CHECK(objects[i].live&&objects[i].pinned&&!objects[i].va);
 }
 else {
  CHECK(!remesa_linux_close_checked(&d));
  for(unsigned i=10;i<created;i++)CHECK(!objects[i].live);
  for(unsigned i=10;i;i--){unsigned j=i-1;CHECK(!ops->gpu_unmap(m,32+j,addresses[j],sizes[j]));CHECK(!ops->unpin(m,32+j));CHECK(!ops->destroy(m,32+j));}
  CHECK(!remesa_linux_memory_detach(m));
 }
 if(memory_journal)CHECK(!fclose(memory_journal));
 printf("PASS %s: %u checks; actual NIR/draw_vbo/production token -> retained ioctl calls=%u; carrier images compared=%u; fixture data only, GPU0\n",requested,checks,retained_calls,compared_images);
 return 0;
}
