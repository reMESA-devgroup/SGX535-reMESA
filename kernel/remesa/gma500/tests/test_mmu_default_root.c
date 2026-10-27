/* SPDX-License-Identifier: GPL-2.0-only */
#define main confinement_test_main
#include "test_mmu_confinement.c"
#undef main
#define down_read down_write
#define up_read up_write
static int atomic_read(atomic_t *p){return *p;}
#include "../remesa_mmu_default_root.inc"
static void root_case(int expected)
{
 u32 out=0xa5a5a5a5;struct fixture before=f;
 int error=psb_mmu_default_root((struct drm_device*)1,&f.def,&out);
 CHECK(error==expected);
 CHECK(out==(expected?0xa5a5a5a5:(u32)f.def.p->pfn<<12));
 CHECK(!memcmp(&before,&f,sizeof f)&&!active&&!maps);
}
static void ready(void)
{reset();f.driver.dev=(struct drm_device*)1;f.driver.has_clflush=1;f.driver.needs_tlbflush=0;}
int main(void)
{
 ready();root_case(0);
 ready();f.def.p->pfn=0xfffff;root_case(0);
 ready();f.def.p->pfn=0x100000;root_case(-ERANGE);
 ready();f.def.p->pfn=0;root_case(-ERANGE);
 ready();f.driver.dev=(struct drm_device*)2;root_case(-EACCES);
 ready();f.driver.default_pd=&f.pd;root_case(-EACCES);
 ready();f.def.hw_context=-1;root_case(-EACCES);
 ready();f.def.hw_context=1;root_case(-EACCES);
 ready();f.def.p=NULL;root_case(-EACCES);
 ready();f.def.tables=NULL;root_case(-EACCES);
 ready();f.def.pd_mask=9;root_case(-EACCES);
 ready();f.driver.has_clflush=0;root_case(-EACCES);
 ready();f.driver.needs_tlbflush=1;root_case(-EACCES);
 ready();u32 out=17;
 CHECK(psb_mmu_default_root(NULL,&f.def,&out)==-EINVAL&&out==17);
 CHECK(psb_mmu_default_root((struct drm_device*)1,NULL,&out)==-EINVAL&&out==17);
 CHECK(psb_mmu_default_root((struct drm_device*)1,&f.def,NULL)==-EINVAL);
 printf("PASS %u authenticated default root checks; CPU model only\n",checks);return 0;
}
