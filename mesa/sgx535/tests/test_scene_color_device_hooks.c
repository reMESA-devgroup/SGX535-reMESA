/* SPDX-License-Identifier: MIT. Actual provider device-hook bridge; modeled
 * native receipts and CPU backing only, no GPU or rendered pixels. */
#define REMESA_SCENE_PROVIDER_TEST_MAIN original_scene_provider_main
#include "test_scene_provider.c"
static unsigned failure;
static int (*saved_read_map)(void *,uint32_t,bool,void **);
static int (*saved_read_unmap)(void *,uint32_t,bool);
static int bridge_map(void *p,uint32_t id,bool write,void **out)
{CHECK(!write);if(failure==1)return EOWNERDEAD;return saved_read_map(p,id,false,out);}
static int bridge_unmap(void *p,uint32_t id,bool write)
{CHECK(!write);if(failure==2)return EOWNERDEAD;return saved_read_unmap(p,id,false);}
int main(int argc,char **argv)
{
 CHECK(argc==2);failure=(unsigned)strtoul(argv[1],NULL,10);CHECK(failure<3);
 CHECK(!setenv("REMESA_CAPTURE_EXPORT_DIR","/UNWRITTEN-MODEL",1));
 CHECK(!setenv("REMESA_CAPTURE_FINITE_VERTICES","1",1));
 CHECK(!setenv("REMESA_CAPTURE_INITIALIZED_PREFIX","1",1));
 CHECK(!setenv("REMESA_CAPTURE_FINALIZED_DEPTH","1",1));
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};struct graph g={0};graph_setup(&f,&d,&g);
 CHECK(d.ops->map_retired_color&&d.ops->unmap_retired_color);void *cpu=NULL;
 CHECK(d.ops->map_retired_color(&d,g.children[8],&cpu)==EAGAIN&&!cpu);
 CHECK(d.ops->map_retired_color(&d,g.children[0],&cpu)==EACCES&&!cpu);
 CHECK(d.ops->map_retired_color(&d,g.children[9],&cpu)==EACCES&&!cpu);
 CHECK(d.ops->map_retired_color(&d,g.table,&cpu)==EACCES&&!cpu);
 CHECK(d.ops->map_retired_color(&d,(void *)1,&cpu)==EINVAL&&!cpu);
 void *fence=NULL;CHECK(!remesa_persistent_scene_capture_submit_keep(&d,g.capture,&fence));d.ops->destroy_fence(d.priv,fence);fence=NULL;
 saved_read_map=target_ops.cpu_map;saved_read_unmap=target_ops.cpu_unmap;
 target_ops.cpu_map=bridge_map;target_ops.cpu_unmap=bridge_unmap;
 unsigned unpins=f.unpins,writes=f.writes;
 int e=d.ops->map_retired_color(&d,g.children[8],&cpu);
 if(failure==1)CHECK(e==EOWNERDEAD&&!cpu);
 else{
  CHECK(!e&&cpu);
  CHECK(d.ops->unmap_retired_color(&d,g.children[9])==ENOENT);
  e=d.ops->unmap_retired_color(&d,g.children[8]);
  CHECK(e==(failure==2?EOWNERDEAD:0));
 }
 CHECK(f.writes==writes&&f.unpins==unpins);
 if(failure){
  CHECK(remesa_persistent_scene_capture_submit_keep(&d,g.capture,&fence)==EOWNERDEAD&&!fence&&calls==1);
  owners_drop(&d,&g);CHECK(!f.destroys&&f.unpins==unpins&&remesa_persistent_close(&d)==EBUSY);
  puts("PASS device COLOR hooks preserve uncertain lease/whole endpoint, no GPU");return 0;
 }
 CHECK(d.ops->unmap_retired_color(&d,g.children[8])==ENOENT);
 CHECK(!remesa_persistent_scene_capture_submit_keep(&d,g.capture,&fence));d.ops->destroy_fence(d.priv,fence);
 CHECK(!remesa_persistent_scene_capture_release(&d,g.capture));owners_drop(&d,&g);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));
 puts("PASS device COLOR hooks select exact published role, refuse shader/DEPTH/DPM, release READ and reuse; GPU0");
}
