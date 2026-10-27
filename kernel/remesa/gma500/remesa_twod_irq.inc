/* SPDX-License-Identifier: GPL-2.0-only */
/* Include in gma500_fixed_backend.c after fixed_irq_lock/fixed_irq_owner.
 * The existing capture hook must invoke this before psb_sgx_interrupt ACK.
 * Reservation set/clear and 3D owner admission use the SAME IRQ lock. */
#include "remesa_twod_irq.h"
static struct remesa_twod_reservation *twod_irq_owner;
static void twod_irq_assert_locked(void)
{
#ifdef __KERNEL__
 lockdep_assert_held(&fixed_irq_lock);
#endif
}
bool remesa_twod_irq_busy_locked(void)
{twod_irq_assert_locked();return twod_irq_owner!=NULL;}
bool remesa_twod_irq_available_locked(void)
{twod_irq_assert_locked();return !fixed_irq_owner&&!twod_irq_owner;}
int remesa_twod_irq_claim_locked(struct remesa_twod_reservation *r)
{
 twod_irq_assert_locked();
 if(!r||!r->held||!r->device||r->device->remesa_twod_owner!=r||
    r->irq_cookie||r->irq_issued||r->irq_retired||r->pending1||r->pending2)return -EINVAL;
 if(!remesa_twod_irq_available_locked())return -EBUSY;
 twod_irq_owner=r;return 0;
}
static bool twod_irq_event_complete(struct remesa_twod_reservation *r)
{return r->pending1==_PSB_CE_TWOD_COMPLETE&&!r->pending2;}
int remesa_twod_irq_drop_locked(struct remesa_twod_reservation *r)
{
 twod_irq_assert_locked();if(!r||twod_irq_owner!=r)return -ENOENT;
 if(r->irq_issued&&(!r->irq_retired||!twod_irq_event_complete(r)))return -EBUSY;
 twod_irq_owner=NULL;return 0;
}
int remesa_twod_irq_issue_locked(struct remesa_twod_reservation *r,u64 cookie)
{
 twod_irq_assert_locked();if(!r||twod_irq_owner!=r)return -ENOENT;
 if(!cookie)return -EINVAL;
 if(r->irq_issued||r->irq_cookie||r->irq_retired)return -EBUSY;
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
