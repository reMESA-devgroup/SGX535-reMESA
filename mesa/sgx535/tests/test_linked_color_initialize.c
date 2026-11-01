/* SPDX-License-Identifier: MIT. Callback/ownership model, no GPU or CPU pixels. */
#include <stdbool.h>
struct remesa_gpu_fill;
static int initialize_fill(void *,const struct remesa_gpu_fill *);
static bool fill_enabled=true;
#define REMESA_LINKED_TEST_NATIVE_FILL (fill_enabled?initialize_fill:NULL)
#define REMESA_LINKED_BACKGROUND_FIXTURE
#include "test_linked_background.c"
#include <unistd.h>
#include <sys/wait.h>
static unsigned fill_calls;
static int fill_error;
static struct remesa_gpu_fill last_fill;
static int initialize_fill(void *priv,const struct remesa_gpu_fill *fill)
{
 CHECK(priv&&fill);fill_calls++;last_fill=*fill;return fill_error;
}
int main(void)
{
 CHECK(!ta_persistent_fixture());CHECK(!ta_linkage_fixture());
 const unsigned sizes[][2]={{1280,800},{640,640},{641,479},{1600,768}};
 for(unsigned k=0;k<4;k++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0};
  bg_setup(&f,&d,6100+k,sizes[k][0],sizes[k][1],c);
  struct remesa_persistent_linked_targets_backing b,after;
  CHECK(!remesa_persistent_linked_targets_snapshot(&d,c[10],&b));
  unsigned maps=f.cpu_maps,creates=f.creates,gpu_maps=f.gpu_maps,reads=f.reads,writes=f.writes,calls=fill_calls;
  void *lease=NULL;CHECK(!d.ops->map(d.priv,c[0],false,&lease));
  CHECK(remesa_persistent_linked_targets_color_initialize(&d,c[10],0xff123456)==EBUSY&&fill_calls==calls);
  d.ops->unmap(d.priv,c[0],false);maps=f.cpu_maps;reads=f.reads;writes=f.writes;
  CHECK(remesa_persistent_linked_targets_color_initialize(&d,c[7],0xff123456)==EINVAL&&fill_calls==calls);
  fill_error=EOPNOTSUPP;
  CHECK(remesa_persistent_linked_targets_color_initialize(&d,c[10],0xff123456)==EOPNOTSUPP&&fill_calls==calls+1);
  fill_error=0;
  for(unsigned repeat=0;repeat<2;repeat++){
   CHECK(!remesa_persistent_linked_targets_color_initialize(&d,c[10],0xff123456));
   CHECK(fill_calls==calls+2+repeat&&last_fill.destination.id==b.color.handle&&
    last_fill.destination.width==((sizes[k][0]+31)&~31u)&&last_fill.destination.height==sizes[k][1]&&
    last_fill.destination.stride==b.color.state.stride_bytes&&last_fill.width==last_fill.destination.width&&
    last_fill.height==last_fill.destination.height&&!last_fill.destination_x&&!last_fill.destination_y&&last_fill.color==0xff123456);
   CHECK((uint64_t)last_fill.width*last_fill.height*4==b.color.state.image_bytes);
  }
  CHECK(f.cpu_maps==maps&&f.creates==creates&&f.gpu_maps==gpu_maps&&f.reads==reads&&f.writes==writes);
  CHECK(get(&f,b.color.handle)->bytes[0]==0xa5&&get(&f,b.color.handle)->bytes[b.color.state.image_bytes-1]==0xa5);
  CHECK(!remesa_persistent_linked_targets_snapshot(&d,c[10],&after)&&!memcmp(&b,&after,sizeof b));
  void *color=c[8];for(unsigned i=0;i<10;i++)d.ops->destroy_bo(d.priv,c[i]);
  CHECK(!remesa_persistent_linked_targets_color_initialize(&d,c[10],0x01020304)&&fill_calls==calls+4);
  const struct remesa_fill_region ordinary={.destination={color,last_fill.width,last_fill.height,last_fill.destination.stride},
   .width=last_fill.width,.height=last_fill.height};
  CHECK(remesa_persistent_fill_region(d.priv,&ordinary)==EINVAL&&fill_calls==calls+4);
  d.ops->destroy_bo(d.priv,c[10]);CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&d));
 }
 fill_enabled=false;
 {
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0};
  bg_setup(&f,&d,6150,640,640,c);unsigned calls=fill_calls;
  CHECK(remesa_persistent_linked_targets_color_initialize(&d,c[10],0)==EOPNOTSUPP&&fill_calls==calls);
  bg_drop_children(&d,c);CHECK(f.creates==f.destroys&&!remesa_persistent_close(&d));
 }
 fill_enabled=true;
 for(unsigned mode=0;mode<2;mode++){
  pid_t child=fork();CHECK(child>=0);
  if(!child){
   struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};void *c[11]={0};
   bg_setup(&f,&d,6160+mode,640,640,c);struct remesa_persistent_linked_targets_backing b;
   CHECK(!remesa_persistent_linked_targets_snapshot(&d,c[10],&b));fill_error=mode?-1:EOWNERDEAD;
   CHECK(remesa_persistent_linked_targets_color_initialize(&d,c[10],0)==EOWNERDEAD);
   bg_drop_children(&d,c);CHECK(get(&f,b.color.handle)->live&&get(&f,b.color.handle)->pinned&&remesa_persistent_close(&d)==EBUSY);
   _exit(0); /* Deliberate uncertain destination retention; no forced disposal. */
  }
  int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 printf("PASS typed COLOR initialization: four dimensions, full pitch plane/row padding, native callback only/no CPU pixels, explicit repeated fill, missing backend/leases/ordinary errors, retained children without widening public writes and uncertain destination retention; GPU0 (%uchecks)\n",checks);return 0;
}
