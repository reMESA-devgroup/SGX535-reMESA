/* SPDX-License-Identifier: MIT
 * Real immutable-image serializer and persistent provider path. Syscalls are
 * interposed; no device, GPU VA installation, cache publication or dispatch.
 */
int inherited_persistent_main(void);
#define main inherited_persistent_main
#include "test_persistent_shader.c"
#undef main
#include "remesa_linux.h"
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <unistd.h>
static unsigned opens,closes,ioctls;
int open(const char *path,int flags,...)
{(void)path;(void)flags;opens++;return 123;}
int close(int fd){CHECK(fd==123);closes++;return 0;}
int ioctl(int fd,unsigned long op,...)
{
 CHECK(fd==123&&op==DRM_IOCTL_VERSION);ioctls++;
 va_list args;va_start(args,op);struct drm_version *v=va_arg(args,void *);va_end(args);
 memcpy(v->name,"gma500",6);v->name_len=6;return 0;
}
static struct remesa_shader_host_pipeline *pipeline(void)
{
 struct remesa_shader_draw_programs pair=programs(0xff00ff00);
 struct remesa_pds_data_cell cells[2][12];
 struct remesa_shader_host_pipeline_request r={.banks={1,2},.data_byte_offsets={4,0}};
 for(unsigned c=0;c<4;c++) {
  r.vertex_mapping.input[c]=(struct remesa_usse_cpu_register){2,c};
  r.vertex_mapping.output[c]=(struct remesa_usse_cpu_register){1,c};
 }
 for(unsigned s=0;s<2;s++) {
  r.use[s]=(struct remesa_pds_binding){.allocation_cookie=1,.owner_cookie=7,
   .mapping_generation=3,.address=0x20000000,.offset=s?32:0,
   .bytes=s?8:16,.allocation_bytes=4096};
  r.pds[s]=(struct remesa_pds_block_request){.operation=9,.generation=4,
   .allocation={.allocation_cookie=2+s,.owner_cookie=7,.mapping_generation=5,
    .address=0x21000000+s*0x01000000,.bytes=256,.allocation_bytes=4096},
   .data_bytes=48,.code_bytes=s?8:4,.code_alignment=16};
  for(unsigned c=0;c<12;c++)cells[s][c]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_LITERAL32,c};
  cells[s][r.data_byte_offsets[s]/4]=(struct remesa_pds_data_cell){REMESA_PDS_DATA_USE_ENTRY_LOWER19,0};
  r.data[s]=(struct remesa_pds_host_data_recipe){cells[s],12};
 }
 struct remesa_pds_structure_request source={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(!remesa_shader_host_pipeline_create_pds_structure(&pair,&r,&source,&p));return p;
}
static void supplied(bool linux_endpoint,enum failure fail)
{
 struct fixture f={.fail=fail},foreign_f={0};struct remesa_device d={0},foreign={0};
 struct remesa_shader_host_pipeline *p=pipeline();struct remesa_shader_host_pipeline_view view;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&view));
 struct remesa_shader_request expected;CHECK(!remesa_shader_image_build_request(view.image_owner,101,1,&expected));
 CHECK(expected.resource_mask==11&&expected.resources[3].length==56&&expected.resources[0].length==16);
 struct remesa_shader_image *clone=NULL;
 CHECK(!remesa_shader_image_clone(view.image_owner,&clone));
 struct remesa_shader_image *old=clone;
 CHECK(remesa_shader_image_clone(view.image_owner,&clone)==EBUSY&&clone==old);
 if(linux_endpoint) {
  CHECK(!remesa_linux_open(&d,"INTERPOSED-NO-DEVICE"));
  void *absent=NULL;CHECK(remesa_linux_prepare_image(&d,clone,&absent)==ENODATA&&!absent);
  struct remesa_persistent_config cfg={.ops=&ops,.priv=&f,.context=101,
   .va_start=0x100000,.va_end=0x200000,.byte_budget=65536,.resource_only_backend=true};
  CHECK(!remesa_linux_enable_shader_staging(&d,&cfg));
 } else open_device(&f,&d,101);
 open_device(&foreign_f,&foreign,102);
 void *occupied=(void *)1;
 unsigned before_creates=f.creates;
 CHECK((linux_endpoint?remesa_linux_prepare_image(&d,clone,&occupied):
  remesa_persistent_prepare_image(&d,clone,&occupied))==EBUSY&&occupied==(void *)1&&f.creates==before_creates);
 struct remesa_shader_image_view clone_view;
 CHECK(!remesa_shader_image_inspect(clone,&clone_view));
 CHECK(remesa_shader_image_clone(clone,(void *)clone_view.fragment_pds.bytes)==EINVAL);
 void *token=NULL;
 int error=linux_endpoint?remesa_linux_prepare_image(&d,clone,&token):remesa_persistent_prepare_image(&d,clone,&token);
 remesa_shader_image_destroy(clone);remesa_shader_host_pipeline_destroy(p);
 if(fail!=FAIL_NONE) {
  CHECK(error!=0&&error!=ENOTSUP&&!token);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
 } else {
  CHECK(error==ENOTSUP&&token);
  struct remesa_shader_request request;struct remesa_shader_bindings bindings;
  CHECK(!(linux_endpoint?remesa_linux_shader_bindings(&d,token,&request,&bindings):
   remesa_persistent_shader_bindings(&d,token,&request,&bindings)));
  CHECK(!memcmp(&request,&expected,sizeof(request)));
  CHECK(bindings.owner==101&&bindings.generation==1&&bindings.resource_mask==11);
  CHECK(!memcmp(f.backing[0].bytes,request.data,request.byte_count));
  for(size_t i=request.byte_count;i<4096;i++)CHECK(!f.backing[0].bytes[i]);
  CHECK(remesa_shader_bindings_validate(&request,&bindings,102,1)==REMESA_SHADER_REQUEST_INVALID);
  CHECK(remesa_shader_bindings_validate(&request,&bindings,101,2)==REMESA_SHADER_REQUEST_INVALID);
  unsigned creates=f.creates,maps=f.cpu_maps;void *occupied=token;
  /* Stale source is gone: token's independent storage still verifies. */
  CHECK(!(linux_endpoint?remesa_linux_shader_verify(&d,token):remesa_persistent_shader_verify(&d,token)));
  CHECK(f.creates==creates&&f.cpu_maps==maps+1);
  CHECK(remesa_persistent_shader_bindings(&foreign,token,&request,&bindings)==EINVAL);
  foreign.ops->destroy_shader_programs(foreign.priv,token);
  CHECK(f.backing[0].live&&occupied==token);
  unsigned syscall_count=ioctls;
  void *fence=(void *)1;
  CHECK(d.ops->submit_shader_programs(d.priv,(void *)1,(void *)1,(void *)1,token,&fence)==ENOTSUP&&!fence);
  CHECK(ioctls==syscall_count&&!f.forbidden);
  struct remesa_shader_host_pipeline *next=pipeline();
  struct remesa_shader_host_pipeline_view next_view;
  CHECK(!remesa_shader_host_pipeline_inspect(next,&next_view));
  void *second=NULL;
  CHECK((linux_endpoint?remesa_linux_prepare_image(&d,next_view.image_owner,&second):
   remesa_persistent_prepare_image(&d,next_view.image_owner,&second))==ENOTSUP&&second&&second!=token);
  remesa_shader_host_pipeline_destroy(next);
  CHECK(!(linux_endpoint?remesa_linux_shader_bindings(&d,second,&request,&bindings):
   remesa_persistent_shader_bindings(&d,second,&request,&bindings)));
  CHECK(request.owner==101&&request.generation==2&&bindings.generation==2);
  d.ops->destroy_shader_programs(d.priv,second);
  d.ops->destroy_shader_programs(d.priv,token);
  CHECK((linux_endpoint?remesa_linux_shader_verify(&d,token):remesa_persistent_shader_verify(&d,token))==EINVAL);
  CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!f.gpu_maps);
 }
 CHECK(!(linux_endpoint?remesa_linux_close_checked(&d):remesa_persistent_close(&d)));
 CHECK(!remesa_persistent_close(&foreign));
}
int main(void)
{
 CHECK(!inherited_persistent_main());
 const enum failure failures[]={FAIL_NONE,FAIL_CREATE,FAIL_CREATE_WITH_HANDLE,FAIL_PIN,
  FAIL_MAP,FAIL_READ_MAP,FAIL_READ_BODY,FAIL_READ_TAIL};
 for(unsigned mode=0;mode<2;mode++)for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);i++)
  supplied(mode,failures[i]);
 CHECK(opens==8&&closes==8&&ioctls==8);
 printf("PASS: %u supplied immutable VS16 FS8 PDS56 staging checks; native adapter, provider rollback, owner/generation; zero GPU calls\n",checks);
 return 0;
}
