/* SPDX-License-Identifier: MIT */
/* Address placement through one real owner implementation, injected CPU ops. */
#define REMESA_VA_PLACEMENT_FIXTURE
#include "test_large_depth_resources.c"
static uint64_t observed_va[64];
static int observe_map(void *data,uint32_t id,uint64_t va,size_t n)
{
 CHECK(id&&id<=64&&va&&!(va&4095)&&n&&!(n&4095));
 observed_va[id-1]=va;return gpu_map(data,id,va,n);
}
static void policy_checks(void)
{
 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device d={0};
 struct remesa_persistent_ops observed=ops;observed.gpu_map=observe_map;
 struct remesa_persistent_config c={.ops=&observed,.priv=&f,.context=1800,
 .va_start=0x20000000,.va_end=0x42000000,.byte_budget=65536,.resource_only_backend=true,
 .reserved_count=1,.reserved={{0x20000000,0x20001000}}};
 c.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){0x20000000,0x20018000,32768};
 c.placement[REMESA_VA_RASTER]=(struct remesa_persistent_va_range){0x30001000,0x30200000,1048576};
 c.placement[REMESA_VA_SURFACE]=(struct remesa_persistent_va_range){0x40000000,0x42000000,0};
 struct remesa_persistent_config bad=c;
 bad.placement[REMESA_VA_PROGRAM].alignment=12288;
 CHECK(remesa_persistent_open(&d,&bad)==EINVAL&&!d.priv&&!f.creates);
 bad=c;bad.placement[REMESA_VA_PROGRAM].end=0x43000000;
 CHECK(remesa_persistent_open(&d,&bad)==EINVAL&&!d.priv);
 bad=c;bad.placement[REMESA_VA_PROGRAM].start++;
 CHECK(remesa_persistent_open(&d,&bad)==EINVAL&&!d.priv);
 bad=c;bad.placement[REMESA_VA_PROGRAM].alignment=UINT64_MAX;
 CHECK(remesa_persistent_open(&d,&bad)==EINVAL&&!d.priv);
 CHECK(!remesa_persistent_open(&d,&c));
 /* Config is copied, and the shared ledger covers overlap across classes. */
 c.placement[REMESA_VA_PROGRAM].start=0x21000000;
 void *a=NULL,*b=NULL,*r=NULL,*z=NULL,*g=NULL;
 CHECK(!remesa_persistent_allocate_placed(&d,4096,REMESA_VA_PROGRAM,&a));
 CHECK(observed_va[0]==0x20008000);
 CHECK(!d.ops->allocate(d.priv,32768,&g));CHECK(observed_va[1]==0x20009000); /* page-first-fit, overlaps the next32K program slot */
 unsigned creates=f.creates;
 CHECK(remesa_persistent_allocate_placed(&d,4096,REMESA_VA_PROGRAM,&b)==ENOSPC&&!b&&f.creates==creates);
 d.ops->destroy_bo(d.priv,g);g=NULL;
 CHECK(!remesa_persistent_allocate_placed(&d,4096,REMESA_VA_PROGRAM,&b));
 CHECK(observed_va[f.creates-1]==0x20010000);
 CHECK(!remesa_persistent_allocate_placed(&d,4096,REMESA_VA_RASTER,&r));
 CHECK(observed_va[f.creates-1]==0x30100000);
 CHECK(!remesa_persistent_depth_allocate(&d,32,32,4,&z));
 struct remesa_persistent_depth_backing depth;
 CHECK(!remesa_persistent_depth_snapshot(&d,z,&depth)&&depth.gpu_va==0x40000000);
 creates=f.creates;
 CHECK(remesa_persistent_allocate_placed(&d,1,(enum remesa_persistent_va_class)-1,&g)==EINVAL&&!g&&creates==f.creates);
 CHECK(remesa_persistent_allocate_placed(&d,SIZE_MAX,REMESA_VA_PROGRAM,&g)==EINVAL&&!g);
 void *occupied=a;
 CHECK(remesa_persistent_allocate_placed(&d,1,REMESA_VA_PROGRAM,&occupied)==EBUSY&&occupied==a);
 void *cpu=NULL;CHECK(!d.ops->map(d.priv,a,true,&cpu));
 CHECK(remesa_persistent_allocate_placed(&d,1,REMESA_VA_PROGRAM,&g)==EBUSY&&!g);
 d.ops->unmap(d.priv,a,true);
 d.ops->destroy_bo(d.priv,a);a=NULL;
 f.fail=FAIL_CREATE;
 CHECK(remesa_persistent_allocate_placed(&d,4096,REMESA_VA_PROGRAM,&a)==ENOMEM&&!a);
 f.fail=FAIL_NONE;
 CHECK(!remesa_persistent_allocate_placed(&d,4096,REMESA_VA_PROGRAM,&a));
 CHECK(observed_va[f.creates-1]==0x20008000); /* failed reservation returned */
 d.ops->destroy_bo(d.priv,a);d.ops->destroy_bo(d.priv,b);d.ops->destroy_bo(d.priv,r);d.ops->destroy_bo(d.priv,z);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden);
 CHECK(!remesa_persistent_close(&d));
 /* Alignment at the top of the 32-bit aperture must refuse, never wrap. */
 c=(struct remesa_persistent_config){.ops=&observed,.priv=&f,.context=1801,
 .va_start=0xfffff000,.va_end=UINT64_C(0x100000000),.byte_budget=4096,.resource_only_backend=true};
 c.placement[REMESA_VA_PROGRAM]=(struct remesa_persistent_va_range){c.va_start,c.va_end,8192};
 CHECK(!remesa_persistent_open(&d,&c));a=NULL;creates=f.creates;
 CHECK(remesa_persistent_allocate_placed(&d,4096,REMESA_VA_PROGRAM,&a)==ENOSPC&&!a&&f.creates==creates);
 CHECK(!remesa_persistent_close(&d));
}
int main(void)
{
 CHECK(!large_depth_resources(false));
 CHECK(!large_depth_resources(true));
 policy_checks();
 printf("PASS shared VA placement: full1280 color/Z/region/scene, program window, raster alignment, cross-class collisions, reservations, rollback, top-bound refusal; NO GPU (%u checks)\n",checks);
 return 0;
}
