// SPDX-License-Identifier: GPL-2.0-only
/* Native semantic draw -> original qualified SGX owner/executor -> GPU readback.
 * No demo subprocess or opaque fixed entry call. This is bounded research
 * integration, not the permanent DRM ABI or a qualified reusable context. */
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include "psb_drv.h"
#include "psb_reg.h"
#include "power.h"
#include "gma500_bo_owner.h"
#include "gma500_fixed_backend.h"
#include "gma500_capsule_observer.h"
#include "remesa_native_bridge.h"
static DEFINE_MUTEX(native_lock);
static struct remesa_qualified_asset native_asset;
static struct remesa_native_draw native_expected;
static struct sgx535_gma500_owner native_owner;
static struct sgx535_gma500_fixed_backend native_backend;
static bool asset_installed, authorization_used;
int remesa_native_asset_install(const struct remesa_qualified_asset *asset)
{
    int ret=0;
    if (!asset || !asset->expected || !asset->construct || !asset->publish || !asset->core_id || !asset->core_revision ||
        !asset->timeout_ticks || asset->timeout_ticks>60*HZ ||
        !asset->max_samples || asset->max_samples>300000) return -EINVAL;
    mutex_lock(&native_lock);
    if (asset_installed || authorization_used) ret=-EBUSY;
    else {
        native_asset=*asset; native_expected=*asset->expected;
        native_asset.expected=&native_expected; asset_installed=true;
    }
    mutex_unlock(&native_lock); return ret;
}
static int valid_request(const struct remesa_native_draw *request)
{
    if (request->version!=REMESA_NATIVE_ABI_VERSION || request->flags ||
        request->reserved || request->reserved2 || request->width!=32 || request->height!=32 ||
        request->vertex_count<3 || request->vertex_count>REMESA_NATIVE_MAX_VERTICES ||
        !request->index_count || request->index_count>REMESA_NATIVE_MAX_INDICES ||
        request->index_count%3) return -EINVAL;
    /* Match all semantic and unused bytes, not just a workload name. Asset
     * qualification binds this immutable description to its actual BO image. */
    if (memcmp(request,&native_expected,offsetof(struct remesa_native_draw,operation)))
        return -EOPNOTSUPP;
    return 0;
}
int remesa_native_draw_ioctl(struct drm_device *dev,void *data,struct drm_file *file)
{
#ifdef REMESA_LAB_DEPTH1280_PROFILE
 /* Legacy semantic ioctl1 describes32x32 and must not enter this candidate.
  * The separately qualified retained v2 route is the sole future caller. */
 (void)dev;(void)data;(void)file;return -EOPNOTSUPP;
#else

    struct remesa_native_draw *request=data;
    struct drm_psb_private *dev_priv;
    struct sgx535_frozen_color_summary summary;
    int ret, cleanup;
    bool admitted=false, guard_power=false;
    if (!dev || !data || !file || !capable(CAP_SYS_ADMIN)) return -EPERM;
    mutex_lock(&native_lock);
    if (!asset_installed) { ret=-ENODEV; goto unlock; }
    if (authorization_used) { ret=-EBUSY; goto unlock; }
    ret=valid_request(request);
    if (ret) goto unlock;
    dev_priv=dev->dev_private;
    if (!dev_priv || !dev_priv->sgx_reg) { ret=-ENODEV; goto unlock; }
    if (!gma_power_begin(dev,false)) { ret=-ENODEV; goto unlock; }
    guard_power=true;
    if (PSB_RSGX32(PSB_CR_CORE_ID)!=native_asset.core_id ||
        PSB_RSGX32(PSB_CR_CORE_REVISION)!=native_asset.core_revision) { ret=-ENODEV; goto unlock; }
    memset((u8 *)request+offsetof(struct remesa_native_draw,operation),0,
           sizeof(*request)-offsetof(struct remesa_native_draw,operation));
    /* The policy allowance cannot be rearmed by close, timeout or error. */
    authorization_used=true;
    request->operation=1;
    ret=sgx535_provenance_native_call(dev,THIS_MODULE,&native_owner,request,
        offsetof(struct remesa_native_draw,operation));
    if (ret) goto result;
    admitted=true;
    ret=native_asset.construct(dev,&native_owner);
    if (ret) goto result;
    ret=native_asset.publish(&native_owner,request);
    if (ret) goto result;
    ret=sgx535_gma500_fixed_backend_begin(&native_backend,&native_owner);
    if (ret) goto result;
    ret=sgx535_fixed_service_run_once(&native_owner.fixed_service,
        &sgx535_gma500_fixed_backend_ops,&sgx535_gma500_fixed_backend_status_source,
        &native_backend,native_asset.core_revision,(u32)request->operation,
        native_asset.timeout_ticks,native_asset.max_samples);
    if (ret) ret=-EIO;
    request->phase=native_owner.session.phase;
    request->events=native_owner.session.observed_events;
    request->issued=native_owner.fixed_service.unsafe_possible?1:0;
    if (!ret && native_owner.session.phase==SGX535_PHASE_RETIRED) {
        ret=sgx535_gma500_owner_capture_color(&native_owner,&summary,request->color);
        if (!ret) { request->published=1; request->status=REMESA_NATIVE_STATUS_COMPLETED; }
    }
result:
    if (native_owner.initialized) {
        request->phase=native_owner.session.phase;
        request->events=native_owner.session.observed_events;
        request->issued=native_owner.fixed_service.unsafe_possible?1:0;
        if (!sgx535_fixed_service_may_release(&native_owner.fixed_service)) {
            request->status=REMESA_NATIVE_STATUS_HELD;
            if (!ret) ret=-EIO;
        } else {
            cleanup=native_backend.active?sgx535_gma500_fixed_backend_end(&native_backend):0;
            if (!cleanup) cleanup=sgx535_gma500_owner_release(&native_owner);
            if (cleanup) {
                request->status=REMESA_NATIVE_STATUS_HELD;
                if (!ret) ret=cleanup;
            }
        }
    }
    request->operation_errno=ret;
    if (admitted) {
        cleanup=sgx535_provenance_native_close(dev,&native_owner,0,request,sizeof(*request));
        if (cleanup) {
            request->published=0;
            request->status=REMESA_NATIVE_STATUS_HELD;
            request->operation_errno=-EIO;
        }
    }
    /* Hardware error is reported inside the attributable response, allowing
     * DRM to copy output even when execution failed or must remain held. */
    ret=0;
unlock:
    if (guard_power) gma_power_end(dev);
    mutex_unlock(&native_lock); return ret;

#endif
}
