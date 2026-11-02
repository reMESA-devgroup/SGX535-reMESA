/* SPDX-License-Identifier: MIT. Actual frozen owners/provider; native receipts
 * and CPU backing are modeled. No kernel, GPU or pixel-correctness claim. */
#define REMESA_SCENE_PROVIDER_TEST_MAIN original_scene_provider_main
#include "test_scene_provider.c"
static unsigned read_fault,read_maps,read_unmaps;
static int (*original_map)(void *,uint32_t,bool,void **);
static int (*original_unmap)(void *,uint32_t,bool);
static void *bad_pointer;
static int read_map(void *p,uint32_t id,bool write,void **out)
{
 CHECK(!write);read_maps++;
 if(read_fault==1)return ENOMEM;
 if(read_fault==2)return EOWNERDEAD;
 int e=original_map(p,id,false,out);CHECK(!e&&*out);
 if(read_fault==3)return EIO;
 if(read_fault==4)*out=NULL;
 if(read_fault==8)*out=bad_pointer;
 return 0;
}
static int read_unmap(void *p,uint32_t id,bool write)
{
 CHECK(!write);read_unmaps++;
 if(read_fault==5)return EOWNERDEAD;
 if(read_fault==6)return EIO;
 return original_unmap(p,id,false);
}
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned which=(unsigned)strtoul(argv[1],NULL,10);CHECK(which<12);
 CHECK(!setenv("REMESA_CAPTURE_EXPORT_DIR","/UNWRITTEN-MODEL",1));
 CHECK(!setenv("REMESA_CAPTURE_FINITE_VERTICES","1",1));
 CHECK(!setenv("REMESA_CAPTURE_INITIALIZED_PREFIX","1",1));
 CHECK(!setenv("REMESA_CAPTURE_FINALIZED_DEPTH","1",1));
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};struct graph g={0};graph_setup(&f,&d,&g);
 original_map=target_ops.cpu_map;original_unmap=target_ops.cpu_unmap;
 target_ops.cpu_map=read_map;target_ops.cpu_unmap=read_unmap;
 const void *cpu=NULL;
 CHECK(remesa_persistent_scene_color_map(&d,g.capture,&cpu)==EAGAIN&&!cpu&&!read_maps);
 CHECK(remesa_persistent_scene_color_unmap(&d,g.capture)==ENOENT);
 struct remesa_scene_capture shared={0};
 if(which==9)CHECK(!remesa_persistent_scene_capture(&d,g.parts[0],g.event,g.parts[2],g.parts[3],&shared));
 void *fence=NULL;CHECK(!remesa_persistent_scene_capture_submit_keep(&d,g.capture,&fence));
 d.ops->destroy_fence(d.priv,fence);fence=NULL;
 unsigned creates=f.creates,writes=f.writes,unpins=f.unpins;
 if(which==11){
  struct remesa_scene_capture bad=g.capture;bad.serial++;
  CHECK(remesa_persistent_scene_color_map(&d,bad,&cpu)==ESTALE&&!cpu&&!read_maps);
  bad=g.capture;bad.owner++;
  CHECK(remesa_persistent_scene_color_map(&d,bad,&cpu)==EINVAL&&!read_maps);
  CHECK(remesa_persistent_scene_color_map(&d,g.capture,(const void **)&d)==EINVAL&&!read_maps);
  cpu=(const void*)1;CHECK(remesa_persistent_scene_color_map(&d,g.capture,&cpu)==EBUSY&&cpu==(const void*)1&&!read_maps);cpu=NULL;
 }
 read_fault=which<=8?which:0;bad_pointer=&d;
 const void **output=&cpu;
 if(which==7){
  struct remesa_persistent_scene_footprint fp;CHECK(!remesa_persistent_scene_capture_snapshot(&d,g.capture,&fp));
  unsigned handle=0;for(unsigned i=0;i<fp.allocation_count;i++)if(fp.allocations[i].roles==REMESA_SCENE_REF_COLOR)handle=fp.allocations[i].handle;
  CHECK(handle&&handle<=f.creates);output=(const void**)f.backing[handle-1].bytes;*output=NULL;
 }
 int e=remesa_persistent_scene_color_map(&d,g.capture,output);
 if(which==1){
  CHECK(e==ENOMEM&&!cpu&&read_maps==1&&!read_unmaps);
  read_fault=0;CHECK(!remesa_persistent_scene_color_map(&d,g.capture,&cpu));
 }else if(which==2||which==3||which==4||which==7||which==8){
  CHECK(e==EOWNERDEAD&&!*output&&read_maps==1&&!read_unmaps);
  CHECK(remesa_persistent_scene_color_unmap(&d,g.capture)==EOWNERDEAD&&!read_unmaps);
  CHECK(remesa_persistent_scene_capture_submit_keep(&d,g.capture,&fence)==EOWNERDEAD&&!fence&&calls==1);
  CHECK(remesa_persistent_scene_capture_release(&d,g.capture)==EOWNERDEAD);
  owners_drop(&d,&g);CHECK(f.unpins==unpins&&!f.destroys&&remesa_persistent_close(&d)==EBUSY);
  puts("PASS uncertain/invalid COLOR mapping retains entire endpoint and possible READ lease; GPU0");return 0;
 }else CHECK(!e&&cpu);
 CHECK(cpu&&read_maps==(which==1?2u:1u)&&!read_unmaps);
 CHECK(remesa_persistent_scene_capture_submit_keep(&d,g.capture,&fence)==EBUSY&&!fence&&calls==1);
 CHECK(remesa_persistent_scene_capture_release(&d,g.capture)==EBUSY);
 const void *another=NULL;CHECK(remesa_persistent_scene_color_map(&d,g.capture,&another)==EBUSY&&!another);
 /* The ordinary unmap entry cannot turn this explicit READ lease into WRITE. */
 d.ops->unmap(d.priv,g.children[8],true);CHECK(!read_unmaps);
 if(which==9){
  CHECK(remesa_persistent_scene_color_unmap(&d,shared)==ENOENT);
  CHECK(remesa_persistent_scene_capture_release(&d,shared)==EBUSY);
 }
 if(which==10)owners_drop(&d,&g);
 e=remesa_persistent_scene_color_unmap(&d,g.capture);
 if(which==5||which==6){
  CHECK(e==EOWNERDEAD&&read_unmaps==1);
  CHECK(remesa_persistent_scene_color_unmap(&d,g.capture)==EOWNERDEAD&&read_unmaps==1);
  owners_drop(&d,&g);CHECK(f.unpins==unpins&&!f.destroys&&remesa_persistent_close(&d)==EBUSY);
  puts("PASS uncertain COLOR unmap retains lease/backing, no retry; GPU0");return 0;
 }
 CHECK(!e&&read_unmaps==1&&remesa_persistent_scene_color_unmap(&d,g.capture)==ENOENT);
 CHECK(f.creates==creates&&f.writes==writes&&f.unpins==unpins);
 CHECK(!remesa_persistent_scene_capture_submit_keep(&d,g.capture,&fence)&&calls==2);
 CHECK(!remesa_persistent_scene_capture_release(&d,g.capture));
 if(which==9)CHECK(!remesa_persistent_scene_capture_release(&d,shared));
 d.ops->destroy_fence(d.priv,fence);if(which!=10)owners_drop(&d,&g);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));
 puts("PASS retired COLOR READ while capture held, exclusion until exact unmap, repeated submit and complete cleanup; GPU0");
}
