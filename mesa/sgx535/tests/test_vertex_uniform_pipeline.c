/* SPDX-License-Identifier: MIT. Real binding/ownership callbacks with a host
 * recorder; source bytes only, no draw, simulated retirement or GPU work. */
#include "gears_nir_fixture.h"
int inherited_vertex_binding_driver_main(void);
#define main inherited_vertex_binding_driver_main
#include "test_driver.c"
#undef main
static bool fail_next_malloc;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t);
void *__wrap_malloc(size_t n){if(fail_next_malloc){fail_next_malloc=false;return NULL;}return __real_malloc(n);}
static void reject_source(struct pipe_context*c,const struct remesa_pds_parameter_source_request*r,int error,size_t cap){
 unsigned char out[192],old[192];memset(out,0xa5,sizeof(out));memcpy(old,out,sizeof(out));size_t n=777;
 assert(remesa_context_vertex_parameter_source(c,r,out,cap,&n)==error&&n==777&&!memcmp(out,old,sizeof(out)));
}
static void source_matches(struct pipe_context*c,const struct remesa_pds_parameter_source_request*r,const void*data){
 unsigned char out[192],expected[192];memset(out,0xa5,sizeof(out));memset(expected,0xa5,sizeof(expected));size_t n=777,expected_n=888;
 assert(!remesa_pds_structure_encode_parameter_source(r,data,r->coefficient_dwords*4,expected,sizeof(expected),&expected_n));
 assert(!remesa_context_vertex_parameter_source(c,r,out,sizeof(out),&n)&&n==expected_n&&!memcmp(out,expected,sizeof(out)));
 assert(n==r->transfer_dwords*4&&out[0]==0&&out[1]==0x64&&out[2]==0&&out[3]==0x20);
 assert(!memcmp(out+4,data,r->coefficient_dwords*4));
 for(unsigned i=(r->coefficient_dwords+1)*4;i<n;i++)assert(!out[i]);
}
int main(void){
 struct recorder rec={0};struct remesa_device d={.ops=&ops,.priv=&rec,.offline_test_backend=true};struct pipe_screen*s=remesa_screen_create(&d);assert(s);
 for(unsigned attr=0;attr<2;attr++){
  struct pipe_context*c=s->context_create(s,NULL,0);assert(c);struct pipe_shader_state sh={.type=PIPE_SHADER_IR_NIR,.ir.nir=gears_nir_fixture(attr)};
  void*vs=c->create_vs_state(c,&sh);assert(vs&&!remesa_context_error(c));c->bind_vs_state(c,vs);
  struct remesa_pds_parameter_source_request req={0x20000000,131072,0x6400,attr?36:40,attr?40:44};
  reject_source(c,&req,ENODATA,192);
  unsigned char user[160],original[160];for(unsigned i=0;i<160;i++)user[i]=(i*37+11)&255;memcpy(original,user,160);
  struct pipe_constant_buffer binding={.user_buffer=user,.buffer_size=160};
  fail_next_malloc=true;c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,&binding);assert(remesa_context_error(c)==ENOMEM&&!fail_next_malloc);reject_source(c,&req,ENOTSUP,192);
  c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,&binding);assert(!remesa_context_error(c));memset(user,0x7f,sizeof(user));source_matches(c,&req,original);
  reject_source(c,&req,ENOSPC,req.transfer_dwords*4-1);struct remesa_pds_parameter_source_request bad=req;bad.transfer_dwords--;reject_source(c,&bad,ENOTSUP,192);bad=req;bad.coefficient_dwords--;reject_source(c,&bad,ENOTSUP,192);
  c->set_constant_buffer(c,PIPE_SHADER_VERTEX,1,false,&binding);assert(remesa_context_error(c)==ENOTSUP);reject_source(c,&req,ENOTSUP,192);c->set_constant_buffer(c,PIPE_SHADER_VERTEX,1,false,NULL);source_matches(c,&req,original);
  binding.buffer_size=144;c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,&binding);assert(!remesa_context_error(c));
  if(attr)source_matches(c,&req,user);else reject_source(c,&req,ENOSPC,192);
  c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,NULL);reject_source(c,&req,ENODATA,192);
  struct pipe_resource*cb=buffer(s,192,PIPE_BIND_CONSTANT_BUFFER);assert(cb);c->buffer_subdata(c,cb,0,16,160,original);assert(!remesa_context_error(c));binding=(struct pipe_constant_buffer){.buffer=cb,.buffer_offset=16,.buffer_size=160};
  c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,&binding);assert(cb->reference.count==2);resource_drop(cb);assert(cb->reference.count==1);unsigned freed=rec.frees;
  c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,&binding);assert(!remesa_context_error(c)&&cb->reference.count==1&&rec.frees==freed);source_matches(c,&req,original);
  c->buffer_subdata(c,cb,0,16,160,user);source_matches(c,&req,user); /* resource resolved at use */
  struct pipe_box box={.x=16,.width=16,.height=1,.depth=1};struct pipe_transfer*t=NULL;void*mapped=c->buffer_map(c,cb,0,PIPE_MAP_READ,&box,&t);assert(mapped&&t);reject_source(c,&req,EBUSY,192);c->buffer_unmap(c,t);
  for(unsigned mode=1;mode<=2;mode++){rec.map_mode=mode;reject_source(c,&req,mode==1?EPROTO:EIO,192);assert(rec.maps==rec.unmaps);}rec.map_mode=0;
  binding.buffer_offset=1;c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,&binding);assert(remesa_context_error(c)==ENOTSUP&&rec.frees==freed+1);reject_source(c,&req,ENOTSUP,192);
  c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,false,NULL);
  cb=buffer(s,160,PIPE_BIND_CONSTANT_BUFFER);assert(cb);c->buffer_subdata(c,cb,0,0,160,original);binding=(struct pipe_constant_buffer){.buffer=cb,.buffer_size=160};c->set_constant_buffer(c,PIPE_SHADER_VERTEX,0,true,&binding);assert(cb->reference.count==1&&!remesa_context_error(c));source_matches(c,&req,original);
  c->delete_vs_state(c,vs);reject_source(c,&req,ENOTSUP,192);c->destroy(c);
 }
 s->destroy(s);assert(rec.allocs==rec.frees&&rec.maps==rec.unmaps&&!rec.draws&&!rec.shader_prepares&&!rec.shader_submits);
 puts("PASS both VS constant profiles: owned user snapshots, resource late reads, borrowed sole-owner rebind, transferred refs, bounds/shortage/sticky rejection, map failures, rollback/zero extension and teardown; ZERO GPU/draws");return 0;
}
