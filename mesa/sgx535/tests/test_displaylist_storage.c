/* SPDX-License-Identifier: MIT */
/* Storage regression for Mesa vbo_save_api's unconditional VERTEX_STATE hint.
 * Does not claim draw_vertex_state support or submit any GPU commands. */
int inherited_cpu_depth24_tests(void);
#define main inherited_cpu_depth24_tests
#include "test_cpu_depth24_lab.c"
#undef main
int main(void)
{
 struct remesa_device d={.ops=&ops,.offline_test_backend=true};
 struct pipe_screen*s=remesa_screen_create(&d);assert(s);
 struct pipe_context*c=s->context_create(s,NULL,0);assert(c);
 const unsigned roles[]={PIPE_BIND_VERTEX_BUFFER,PIPE_BIND_INDEX_BUFFER,
                         PIPE_BIND_VERTEX_BUFFER|PIPE_BIND_INDEX_BUFFER};
 for(unsigned k=0;k<3;k++){
  struct pipe_resource t={.target=PIPE_BUFFER,.format=PIPE_FORMAT_R8_UNORM,
   .width0=1024*1024,.height0=1,.depth0=1,.array_size=1,
   .bind=roles[k]|PIPE_BIND_VERTEX_STATE};
  assert(s->is_format_supported(s,t.format,t.target,0,0,t.bind));
  struct pipe_resource*b=s->resource_create(s,&t);assert(b);
  unsigned char data[1224];for(unsigned i=0;i<sizeof(data);i++)data[i]=(unsigned char)(i*13);
  c->buffer_subdata(c,b,0,0,sizeof(data),data);assert(!remesa_context_error(c));
  c->buffer_subdata(c,b,0,t.width0-sizeof(data),sizeof(data),data);assert(!remesa_context_error(c));
  struct pipe_transfer*tr=NULL;struct pipe_box box={.x=t.width0-sizeof(data),.width=sizeof(data),.height=1,.depth=1};
  void*p=c->buffer_map(c,b,0,PIPE_MAP_READ,&box,&tr);assert(p&&tr&&!memcmp(p,data,sizeof(data)));c->buffer_unmap(c,tr);
  unsigned before=maps;c->buffer_subdata(c,b,0,t.width0-sizeof(data)+1,sizeof(data),data);assert(remesa_context_error(c)&&maps==before);
  remesa_resource_reference(&b,NULL);
  t.bind=PIPE_BIND_VERTEX_STATE;assert(!s->resource_create(s,&t));
  t.bind=roles[k]|PIPE_BIND_VERTEX_STATE|PIPE_BIND_SHADER_BUFFER;assert(!s->resource_create(s,&t));
  t.bind=roles[k]|PIPE_BIND_VERTEX_STATE;t.format=PIPE_FORMAT_R32G32B32A32_FLOAT;assert(!s->resource_create(s,&t));
 }
 assert(!s->create_vertex_state&&!c->draw_vertex_state); /* no new execution capability */
 c->destroy(c);s->destroy(s);assert(!allocations&&!submits);
 puts("PASS display-list 1MiB vertex/index storage: upload/readback/end bounds, invalid roles rejected; GPU submissions=0");return 0;
}
