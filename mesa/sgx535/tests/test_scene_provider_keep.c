/* SPDX-License-Identifier: MIT. Actual provider/ownership; native receipts
 * modeled, no kernel, GPU or pixels. Original one-shot fixture stays intact. */
#define REMESA_SCENE_PROVIDER_TEST_MAIN original_scene_provider_main
#include "test_scene_provider.c"
int main(int argc,char **argv)
{
 CHECK(argc==2);unsigned which=(unsigned)strtoul(argv[1],NULL,10);CHECK(which<11);
 CHECK(!setenv("REMESA_CAPTURE_EXPORT_DIR","/UNWRITTEN-MODEL",1));
 CHECK(!setenv("REMESA_CAPTURE_FINITE_VERTICES","1",1));
 CHECK(!setenv("REMESA_CAPTURE_INITIALIZED_PREFIX","1",1));
 CHECK(!setenv("REMESA_CAPTURE_FINALIZED_DEPTH","1",1));
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};struct graph g={0};
 graph_setup(&f,&d,&g);struct remesa_scene_capture token=g.capture;
 struct remesa_scene_packet before={0},after={0};CHECK(!remesa_persistent_scene_packet_snapshot(&d,token,&before));
 unsigned creates=f.creates,reads=f.reads,writes=f.writes,unpins=f.unpins;
 void *fences[8]={0};
 if(which==0){
  owners_drop(&d,&g);CHECK(!f.destroys);
  for(unsigned i=0;i<32;i++){
   CHECK(!remesa_persistent_scene_capture_submit_keep(&d,token,&fences[0])&&fences[0]);
   CHECK(!d.ops->wait(d.priv,fences[0],0,true));d.ops->destroy_fence(d.priv,fences[0]);fences[0]=NULL;
   CHECK(!remesa_persistent_scene_packet_snapshot(&d,token,&after)&&!memcmp(&before,&after,sizeof before));
   CHECK(f.creates==creates&&f.reads==reads&&f.writes==writes&&f.unpins==unpins&&!f.destroys);
  }
  CHECK(calls==32&&remesa_persistent_close(&d)==EBUSY);
 }else if(which==1){
  for(unsigned i=0;i<8;i++)CHECK(!remesa_persistent_scene_capture_submit_keep(&d,token,&fences[i]));
  void *ninth=NULL;CHECK(remesa_persistent_scene_capture_submit_keep(&d,token,&ninth)==EBUSY&&!ninth&&calls==8);
  d.ops->destroy_fence(d.priv,fences[0]);fences[0]=NULL;
  CHECK(!remesa_persistent_scene_capture_submit_keep(&d,token,&fences[0])&&calls==9);
  for(unsigned i=0;i<8;i++)d.ops->destroy_fence(d.priv,fences[i]);
 }else{
  CHECK(!remesa_persistent_scene_capture_submit_keep(&d,token,&fences[0]));
  CHECK(!d.ops->wait(d.priv,fences[0],0,true));
  if(which==2){
   mode=1;CHECK(remesa_persistent_scene_capture_submit_keep(&d,token,&fences[1])==EOPNOTSUPP&&!fences[1]);
   CHECK(!remesa_persistent_scene_packet_snapshot(&d,token,&after)&&!memcmp(&before,&after,sizeof before));
   mode=0;CHECK(!remesa_persistent_scene_capture_submit_keep(&d,token,&fences[1])&&calls==3);
   d.ops->destroy_fence(d.priv,fences[0]);d.ops->destroy_fence(d.priv,fences[1]);
  }else{
   static const unsigned faults[]={2,3,4,5,6,7,8,13};mode=faults[which-3];
   CHECK(remesa_persistent_scene_capture_submit_keep(&d,token,&fences[1])==EOWNERDEAD&&fences[1]&&calls==2);
   CHECK(remesa_persistent_scene_capture_release(&d,token)==EOWNERDEAD);
   void *extra=NULL;CHECK(remesa_persistent_scene_capture_submit_keep(&d,token,&extra)==EOWNERDEAD&&!extra&&calls==2);
   owners_drop(&d,&g);d.ops->destroy_fence(d.priv,fences[0]);d.ops->destroy_fence(d.priv,fences[1]);
   CHECK(f.creates==creates&&f.reads==reads&&f.writes==writes&&f.unpins==unpins&&!f.destroys);
   CHECK(remesa_persistent_close(&d)==EBUSY);
   puts("PASS later ambiguous/malformed receipt retains reusable capture and complete graph; GPU0");return 0;
  }
 }
 CHECK(!remesa_persistent_scene_packet_snapshot(&d,token,&after)&&!memcmp(&before,&after,sizeof before));
 CHECK(f.creates==creates&&f.reads==reads&&f.writes==writes&&f.unpins==unpins);
 CHECK(!remesa_persistent_scene_capture_release(&d,token));
 CHECK(remesa_persistent_scene_packet_snapshot(&d,token,&after)==ESTALE);
 if(which!=0)owners_drop(&d,&g);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!remesa_persistent_close(&d));
 puts("PASS reusable capture: same owners/packet, monotonic operations, bounded fences, no per-submit memory IO, exact cleanup; GPU0");
}
