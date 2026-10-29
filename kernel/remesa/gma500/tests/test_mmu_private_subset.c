/* SPDX-License-Identifier: GPL-2.0-only */
/* Real private owner/auditor with a GPU subset and retained CPU-only sidecar. */
#define REMESA_PRIVATE_TEST_MAIN prior_owner_test_main
#include "test_mmu_private.c"
int main(void)
{
 struct remesa_mmu_private *owner=NULL;
 struct remesa_mmu_allowed_binding gpu[2];
 u32 scratch[REMESA_MMU_AUDIT_OVERHEAD_WORDS+4];
 setup_owner();gpu[0]=f.bindings[0];gpu[1]=f.bindings[1];
 CHECK(!remesa_mmu_private_prepare(&f.driver,gpu,2,&owner));
 CHECK(owner&&owner->count==2&&owner->held==3&&owner->pd->hw_context==-1);
 CHECK(!owner->pd->tables[1023]&&!owner->pd->p->words[1023]);
 CHECK(references[11]==2&&references[12]==2&&references[13]==2&&references[14]==1);
 CHECK(!psb_mmu_audit_unbound_pd(owner->pd,gpu,2,scratch,
                                REMESA_MMU_AUDIT_OVERHEAD_WORDS+3));
 CHECK(!remesa_mmu_private_release(&owner));released();
 printf("PASS %u actual private-owner subset checks; CPU-only page absent; no GPU\n",checks);
 return 0;
}
