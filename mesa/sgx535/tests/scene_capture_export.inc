/* SPDX-License-Identifier: MIT. Export actual constructors, GPU0. */
#include "../../../tools/remesa/scene_fixture.h"
#include <sys/stat.h>
static void fixture_write(const char *dir,const char *name,const void *data,size_t bytes)
{
 char path[4096];CHECK(snprintf(path,sizeof path,"%s/%s",dir,name)>0);
 FILE *fp=fopen(path,"wbx");CHECK(fp);CHECK(fwrite(data,1,bytes,fp)==bytes);CHECK(!fclose(fp));
}
static void scene_capture_export(struct fixture *f,const struct remesa_persistent_scene_footprint *footprint,
 const struct remesa_scene_packet *packet)
{
 const char *root=getenv("REMESA_CAPTURE_EXPORT_DIR");if(!root)return;
 char dir[4096],name[64];CHECK(snprintf(dir,sizeof dir,"%s/%ux%u",root,footprint->width,footprint->height)>0);
 CHECK(!mkdir(dir,0700));struct remesa_scene_fixture fixture={.version=1,.count=footprint->allocation_count};
 for(unsigned i=0;i<fixture.count;i++){
  fixture.bindings[i].address=footprint->allocations[i].gpu_va;fixture.bindings[i].bytes=footprint->allocations[i].allocation_bytes;
  fixture.bindings[i].id=footprint->allocations[i].handle;fixture.bindings[i].access=footprint->allocations[i].access;
  if(fixture.bindings[i].access==REMESA_ACCESS_READ){
   struct backing *b=get(f,fixture.bindings[i].id);CHECK(b&&b->length==fixture.bindings[i].bytes);
   CHECK(snprintf(name,sizeof name,"bo-%u.bin",i)>0);fixture_write(dir,name,b->bytes,b->length);
  }
 }
 fixture_write(dir,"packet.bin",packet,sizeof(*packet));fixture_write(dir,"placement.bin",&fixture,sizeof fixture);
}
