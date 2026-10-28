/* SPDX-License-Identifier: MIT. Provider release ownership, CPU-only fixture. */
int persistent_fixture(void);
#define main persistent_fixture
#include "test_persistent_shader.c"
#undef main
struct lifetime {struct fixture *fixture;unsigned releases;int error;};
static int provider_release(void *p)
{struct lifetime *l=p;CHECK(l->fixture->creates==l->fixture->destroys&&l->fixture->pins==l->fixture->unpins);l->releases++;return l->error;}
int main(void)
{
 for(unsigned fault=0;fault<3;fault++){
  struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};open_device(&f,&d,710+fault);
  struct lifetime l={.fixture=&f,.error=fault==1?EIO:fault==2?-1:0};
  CHECK(remesa_persistent_take_provider(NULL,provider_release,&l)==EINVAL);
  CHECK(remesa_persistent_take_provider(&d,NULL,&l)==EINVAL);
  CHECK(!remesa_persistent_take_provider(&d,provider_release,&l));
  CHECK(remesa_persistent_take_provider(&d,provider_release,&l)==EBUSY);
  void *bo=NULL;CHECK(!d.ops->allocate(d.priv,4096,&bo)&&bo);
  CHECK(remesa_persistent_close(&d)==EBUSY&&!l.releases);
  d.ops->destroy_bo(d.priv,bo);
  int e=remesa_persistent_close(&d);CHECK(l.releases==1);
  if(fault){CHECK(e==(fault==1?EIO:EOWNERDEAD)&&d.priv);
   CHECK(remesa_persistent_close(&d)==e&&l.releases==1);
   CHECK(d.ops->allocate(d.priv,4096,&bo)==EIO); /* Failed owner stays closed. */
  }else CHECK(!e&&!d.priv&&!d.ops);
 }
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};open_device(&f,&d,720);void *bo=NULL;
 CHECK(!d.ops->allocate(d.priv,4096,&bo));struct lifetime l={.fixture=&f};
 CHECK(remesa_persistent_take_provider(&d,provider_release,&l)==EBUSY&&!l.releases);
 d.ops->destroy_bo(d.priv,bo);CHECK(!remesa_persistent_close(&d)&&!l.releases);
 puts("PASS provider lifetime: opt-in before resources, deferred close, exactly-once release, cached failure/malformed retention and unchanged borrowed-provider teardown; GPU0");return 0;
}
