/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in gma500_fixed_backend.c after fixed_irq_lock/fixed_irq_owner.
 * The existing capture hook must invoke this before psb_sgx_interrupt ACK.
 * Reservation set/clear and 3D owner admission use the SAME IRQ lock. */
#include "remesa_twod_irq.h"
static struct remesa_twod_reservation *twod_irq_owner;
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
static struct remesa_twod_reservation *twod_irq_fixed_lender;
static const void *twod_irq_fixed_token;
#endif
static void twod_irq_assert_locked(void)
{
#ifdef __KERNEL__
 lockdep_assert_held(&fixed_irq_lock);
#endif
}
bool remesa_twod_irq_busy_locked(void)
{
 twod_irq_assert_locked();
#ifdef REMESA_PRIVATE_IRQ_HANDOFF
 return twod_irq_owner!=NULL||twod_irq_fixed_lender!=NULL;
#else
 return twod_irq_owner!=NULL;
#endif
}
bool remesa_twod_irq_available_locked(void)
{twod_irq_assert_locked();return !fixed_irq_owner&&!remesa_twod_irq_busy_locked();}
int remesa_twod_irq_claim_locked(struct remesa_twod_reservation *r)
{
 twod_irq_assert_locked();
 if(!r||!r->held||!r->device||r->device->remesa_twod_owner!=r||
    r->irq_cookie||r->irq_issued||r->irq_retired||r->pending1||r->pending2||r->mmu_owner)return -EINVAL;
 if(!remesa_twod_irq_available_locked())return -EBUSY;
 twod_irq_owner=r;return 0;
}
static bool twod_irq_event_complete(struct remesa_twod_reservation *r)
{return r->pending1==_PSB_CE_TWOD_COMPLETE&&!r->pending2;}
int remesa_twod_irq_drop_locked(struct remesa_twod_reservation *r)
{
 twod_irq_assert_locked();if(!r||twod_irq_owner!=r)return -ENOENT;
 if(r->mmu_owner)return -EBUSY;
 if(r->irq_issued&&(!r->irq_retired||!twod_irq_event_complete(r)))return -EBUSY;
 twod_irq_owner=NULL;return 0;
}
int remesa_twod_irq_issue_locked(struct remesa_twod_reservation *r,u64 cookie)
{
 twod_irq_assert_locked();if(!r||twod_irq_owner!=r)return -ENOENT;
 if(!cookie)return -EINVAL;
 if(r->irq_issued||r->irq_cookie||r->irq_retired||r->mmu_owner)return -EBUSY;
 r->irq_cookie=cookie;r->irq_issued=true;return 0;
}
void remesa_twod_irq_capture_locked(struct drm_device *dev,u32 status1,u32 status2)
{
 struct remesa_twod_reservation *r=twod_irq_owner;
 twod_irq_assert_locked();
 if(!r||!r->irq_issued||!r->held||r->device->dev!=dev)return;
 /* Keep 2D bit27 AND all unexpected/fault bits. Only master is aggregate. */
 r->pending1|=status1&~(1u<<31);r->pending2|=status2;
}
int remesa_twod_irq_snapshot_locked(struct remesa_twod_reservation *r,u64 cookie,
 struct remesa_twod_irq_snapshot *out)
{
 struct remesa_twod_irq_snapshot result;
 twod_irq_assert_locked();if(!out)return -EINVAL;
 if(!r||twod_irq_owner!=r)return -ENOENT;
 if(!r->irq_issued||!cookie||cookie!=r->irq_cookie)return -EACCES;
 result.cookie=r->irq_cookie;result.status1=r->pending1;result.status2=r->pending2;
 *out=result;return 0;
}
int remesa_twod_irq_mark_retired_locked(struct remesa_twod_reservation *r,u64 cookie)
{
 twod_irq_assert_locked();if(!r||twod_irq_owner!=r)return -ENOENT;
 if(!r->irq_issued||!cookie||cookie!=r->irq_cookie)return -EACCES;
 if(!twod_irq_event_complete(r))return -EIO;
 if(r->irq_retired)return -EBUSY;
 r->irq_retired=true;return 0;
}

int remesa_twod_irq_guard_locked(struct remesa_twod_reservation *r)
{
 twod_irq_assert_locked();
 if(!r||twod_irq_owner!=r)return -ENOENT;
 if(r->irq_cookie||r->irq_issued||r->irq_retired||r->pending1||r->pending2)
  return -EBUSY;
 return 0;
}

#ifdef REMESA_PRIVATE_IRQ_HANDOFF
/* Private to the staged backend translation unit. Both publication points of
 * begin recheck this under the SAME IRQ lock; the console reservation never
 * drops. These checks do not replace source-history admission or retirement. */
static int remesa_twod_irq_handoff_ready_locked(struct remesa_twod_reservation *r,
 const void *token,struct drm_device *dev)
{
 twod_irq_assert_locked();
 if(!r||!token||!dev)return -EINVAL;
 if(fixed_irq_owner||twod_irq_fixed_lender)return -EBUSY;
 if(!r->held||!r->device||r->device->dev!=dev||r->device->remesa_twod_owner!=r||
    r->mmu_owner!=token)return -EACCES;
 return remesa_twod_irq_guard_locked(r);
}
static int remesa_twod_irq_handoff_fixed_locked(struct remesa_twod_reservation *r,
 const void *token,struct sgx535_gma500_fixed_backend *backend,struct drm_device *dev)
{
 int error;
 if(!backend)return -EINVAL;
 error=remesa_twod_irq_handoff_ready_locked(r,token,dev);if(error)return error;
 twod_irq_fixed_lender=r;twod_irq_fixed_token=token;
 twod_irq_owner=NULL;fixed_irq_owner=backend;return 0;
}
/* Called only after the original fixed-service may_release gate succeeds.
 * Extra pending events remain a rejection, not an event-mask workaround. */
static int remesa_twod_irq_return_fixed_locked(struct sgx535_gma500_fixed_backend *backend)
{
 struct remesa_twod_reservation *r=twod_irq_fixed_lender;
 twod_irq_assert_locked();
 if(fixed_irq_owner!=backend)return -ENOENT;
 if(!r){fixed_irq_owner=NULL;return 0;}
 if(twod_irq_owner||!r->held||!r->device||r->device->remesa_twod_owner!=r||
    r->mmu_owner!=twod_irq_fixed_token)return -EIO;
 if(r->irq_cookie||r->irq_issued||r->irq_retired||r->pending1||r->pending2||
    backend->pending_status1||backend->pending_status2)return -EBUSY;
 twod_irq_owner=r;fixed_irq_owner=NULL;
 twod_irq_fixed_lender=NULL;twod_irq_fixed_token=NULL;return 0;
}
static u32 remesa_twod_irq_guarded_extra_locked(
 struct sgx535_gma500_fixed_backend *backend,u32 status1)
{
 twod_irq_assert_locked();
 /* Existing normal3D masks are untouched; guarded3D must retain an unexpected
  * 2D completion because its producer reservation prevents any owned2D job. */
 return twod_irq_fixed_lender&&fixed_irq_owner==backend?status1&(1u<<27):0;
}
#endif
