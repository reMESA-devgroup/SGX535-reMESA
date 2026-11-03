/* SPDX-License-Identifier: GPL-2.0-only */
/* Included after the qualified scene-controller CPU dependency fixture.
 * All source/GEM/execute/patch callbacks are modeled; this is never GPU proof. */
static void typed_submit(struct drm_device *dev,struct drm_file *file,unsigned version,
 unsigned frame,struct remesa_general_session_request *r)
{
 *r=(struct remesa_general_session_request){.version=version,.action=2,.context=77,
  .session=1,.sequence=frame,.budget_ms=1000};
 expected_execution_reads=version==1?expected_reads:0;
 hashes=0;CHECK(!remesa_general_session_ioctl(dev,r,file));
}
int main(int argc,char **argv)
{
 CHECK(argc==3);unsigned which=(unsigned)strtoul(argv[1],NULL,10),size=(unsigned)strtoul(argv[2],NULL,10);
 CHECK(which<20&&size<4);fault=OK;expected_count=18;
 struct drm_psb_private priv={(void *)1};struct drm_device dev={&priv};struct drm_file file={1},otherfile={2};
 struct remesa_gem_memory memory={.dev=&dev,.file=&file,.context=77,.root={0,0xdfffffffULL}};
 endpoint=&memory;memory_enabled=scene_probe_enabled=true;
 const struct remesa_gears_scene_probe_profile *profile=size<2?&gears_homogeneous_probe_profiles[size]:&gears_clamped_api_probe_profiles[size-2];
 expected_bindings=profile->bindings;
 unsigned version=which==15?1:2;
 struct remesa_general_session_request r={.version=version,.action=1,.context=77,.profile=3+size,.max_jobs=3};
 for(unsigned i=0;i<18;i++){
  r.ids[i]=32+i;memory.bo[i]=(struct backing){.live=true,.generation=1,.va={expected_bindings[i].address,0},
   .gem={expected_bindings[i].bytes},.pinned=true,.reserved=true,.initialized=true,.zero_pristine=true};
  if(expected_bindings[i].access==1){expected_reads++;images[i]=malloc(expected_bindings[i].bytes);CHECK(images[i]);}
 }
 CHECK(!remesa_general_session_ioctl(&dev,&r,&file)&&!r.error&&r.state==1&&hashes==13);
 struct remesa_general_parameter_request u={.version=1,.draw=1,.offset=4,.words=16,
  .context=77,.session=1,.retired_operation=1,.sequence=1};
 for(unsigned i=0;i<16;i++)u.data[i]=0x3f800000;
 CHECK(!remesa_general_parameter_ioctl(&dev,&u,&file)&&u.error==(version==1?-EOPNOTSUPP:-ENODATA)&&!patch_calls);
 typed_submit(&dev,&file,version,1,&r);CHECK(!r.error&&r.state==1&&executions==1&&bind_calls==1);
 CHECK(hashes==(version==1?13u:0u)&&general_session->parameter_policy_count==1);
 u=(struct remesa_general_parameter_request){.version=1,.draw=1,.offset=4,.words=16,
  .context=77,.session=1,.retired_operation=r.operation,.sequence=1};
 for(unsigned i=0;i<16;i++)u.data[i]=0x3f800000;
 memset(model_parameter_bytes,0x5a,sizeof model_parameter_bytes);
 switch(which){
  case 1:u.context++;break;case 2:u.retired_operation++;break;case 3:u.sequence++;break;
  case 4:u.draw=2;break;case 5:u.flags=1;break;case 6:u.data[16]=1;break;
  case 7:u.data[0]=0x7f800000;break;case 8:u.offset=0;break;
  case 9:patch_error=-EIO;break;case 10:patch_error=-ENOMEM;break;
  case 12:scene_probe_enabled=false;break;case 13:administrator=false;break;
  case 17:general_session->updates=~0ULL;break;case 18:u.reserved[1]=1;break;
 }
 struct remesa_general_parameter_request before=u;
 int rc=remesa_general_parameter_ioctl(&dev,&u,which==11?&otherfile:&file);
 CHECK(!memcmp(&u,&before,offsetof(struct remesa_general_parameter_request,accepted_sequence)));
 if(which==1||which==5||which==6||which==11||which==12||which==13||which==18){
  CHECK(rc<0&&!patch_calls&&!general_session->updates&&executions==1);
 }else if(which==9){
  CHECK(!rc&&u.error==-EIO&&u.state==2&&general_session->poisoned&&!general_session->updates);
  u=before;CHECK(!remesa_general_parameter_ioctl(&dev,&u,&file)&&u.error==-EIO&&patch_calls==1);return 0;
 }else if(which==2||which==3||which==4||which==7||which==8||which==10||which==15||which==17){
  CHECK(!rc&&u.error&&u.state==1&&!u.accepted_sequence&&executions==1);
  CHECK(general_session->updates==(which==17?~0ULL:0));
  for(unsigned i=0;i<sizeof model_parameter_bytes;i++)CHECK(model_parameter_bytes[i]==0x5a);
 }else{
  CHECK(!rc&&!u.error&&u.state==1&&u.accepted_sequence==1&&general_session->updates==1&&executions==1);
  CHECK(!memcmp(model_parameter_bytes+4,u.data,64)&&patch_calls==1);
  for(unsigned i=0;i<sizeof model_parameter_bytes;i++)if(i<4||i>=68)CHECK(model_parameter_bytes[i]==0x5a);
  if(which==16){
   u=before;u.sequence=2;typed_submit(&dev,&file,2,2,&r);
   CHECK(!r.error&&!hashes&&executions==2&&bind_calls==1);
   CHECK(!remesa_general_parameter_ioctl(&dev,&u,&file)&&u.error==-ESTALE&&!u.accepted_sequence&&patch_calls==1);
   u=before;u.sequence=2;u.retired_operation=r.operation;
   CHECK(!remesa_general_parameter_ioctl(&dev,&u,&file)&&!u.error&&u.accepted_sequence==2&&patch_calls==2&&executions==2);
  }else if(which==19){
   fault=EXEC;typed_submit(&dev,&file,2,2,&r);
   CHECK(r.error&&r.state==2&&general_session->poisoned);
   u=before;u.sequence=2;CHECK(!remesa_general_parameter_ioctl(&dev,&u,&file)&&u.state==2&&u.error==-EIO&&patch_calls==1);return 0;
  }
 }
 administrator=true;scene_probe_enabled=memory_enabled=false;
 r=(struct remesa_general_session_request){.version=version==1?2:1,.action=3,.context=77,.session=1,
  .sequence=general_session->completed};
 CHECK(remesa_general_session_ioctl(&dev,&r,&file)==-EPROTO&&general_session);
 r.version=version;CHECK(!remesa_general_session_ioctl(&dev,&r,&file)&&!r.error&&r.state==3&&!general_session);
 CHECK(!endpoint_lock.held&&!scene_probe_lock.held);
 for(unsigned i=0;i<18;i++)free(images[i]);
 printf("PASS%u typed-session/controller/atomic-data/epoch/v1-preservation checks, callbacks modeled; GPU0\n",checks);return 0;
}
