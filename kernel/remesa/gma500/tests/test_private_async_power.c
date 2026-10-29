/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual IRQ/backlight bodies; reduced PM, DRM, IRQ and I2C/PWM dependencies. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#define CONFIG_BACKLIGHT_CLASS_DEVICE 1
#define dev_dbg(...) ((void)0)
#define dev_err(...) ((void)0)
#define DRM_DEBUG_DRIVER(...) ((void)0)
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define _PSB_PIPE_EVENT_FLAG 1U
#define _PSB_IRQ_ASLE 2U
#define _PSB_IRQ_SGX_FLAG 4U
#define _PSB_IRQ_DISP_HOTSYNC 8U
#define PSB_INT_IDENTITY_R 1
#define PSB_CR_EVENT_STATUS 2
#define PSB_CR_EVENT_STATUS2 3
#define PORT_HOTPLUG_STAT 4
#define BLC_I2C_TYPE 1
#define ASLE_BCLP_VALID (1U<<31)
#define ASLE_BCLP_MSK 255U
#define ASLE_BACKLIGHT_FAILED 1U
#define ASLE_CBLV_VALID (1U<<31)
typedef uint32_t u32;
typedef int irqreturn_t;
struct drm_device;
struct backlight_device {struct {int brightness,max_brightness;} props;struct drm_device *dev;};
struct opregion_asle {u32 cblv;};
struct bl_info {int type;};
struct ops {int (*hotplug)(struct drm_device *);};
struct drm_psb_private {
 pthread_mutex_t irqmask_lock;u32 vdc_irq_mask;struct ops *ops;
 struct {struct opregion_asle *asle;} opregion;
 struct backlight_device *backlight_device;struct bl_info *lvds_bl;
 bool backlight_enabled;int backlight_level;
};
struct drm_device {struct drm_psb_private *dev_private;};
static struct drm_psb_private private;
static struct drm_device device={&private};
static struct backlight_device backlight={.dev=&device};
static struct backlight_device *psb_backlight_device=&backlight;
static struct opregion_asle asle;
static struct bl_info bl;
static struct ops ops;
static pthread_mutex_t admission=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ops_lock=PTHREAD_MUTEX_INITIALIZER;
static bool registered,unregister_started,unregister_done;
static pthread_mutex_t control=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t change=PTHREAD_COND_INITIALIZER;
static _Thread_local bool pm_owner,interrupt_context;
static bool active,transition,unplugged,pause_irq,pause_brightness,pause_callback,paused,released;
static unsigned refs,entries,reads,writes,bad_mmio,captures,acks,brightness_calls;
static unsigned source_order,callback_depth;
static int worker_result;
static bool gma_power_begin(struct drm_device *d,bool force)
{
 bool ok;(void)force;assert(d==&device);pthread_mutex_lock(&admission);
 ok=active&&(!transition||(pm_owner&&!interrupt_context));if(ok)refs++;
 pthread_mutex_unlock(&admission);return ok;
}
static void gma_power_end(struct drm_device *d)
{assert(d==&device);pthread_mutex_lock(&admission);assert(refs);refs--;pthread_mutex_unlock(&admission);}
static bool gma_power_is_on(struct drm_device *d)
{bool ok;assert(d==&device);pthread_mutex_lock(&admission);ok=active&&(!transition||(pm_owner&&!interrupt_context));pthread_mutex_unlock(&admission);return ok;}
static bool drm_dev_enter(struct drm_device *d,int *index)
{assert(d==&device);pthread_mutex_lock(&admission);bool ok=!unplugged;if(ok)entries++;*index=1;pthread_mutex_unlock(&admission);return ok;}
static void drm_dev_exit(int index)
{assert(index==1);pthread_mutex_lock(&admission);assert(entries);entries--;pthread_mutex_unlock(&admission);}
static int mock_suspend(void)
{pthread_mutex_lock(&admission);if(refs){pthread_mutex_unlock(&admission);return -EBUSY;}transition=true;active=false;pthread_mutex_unlock(&admission);return 0;}
static void seam(bool enabled)
{if(!enabled)return;pthread_mutex_lock(&control);paused=true;pthread_cond_broadcast(&change);while(!released)pthread_cond_wait(&change,&control);pthread_mutex_unlock(&control);}
static void mmio(void){if(!active)bad_mmio++;}
static u32 reg_read(unsigned reg)
{mmio();reads++;if(reg==PSB_INT_IDENTITY_R){seam(pause_irq);return _PSB_IRQ_SGX_FLAG|_PSB_PIPE_EVENT_FLAG;}return reg==PSB_CR_EVENT_STATUS?0x40:0;}
#define PSB_RVDC32(r) reg_read(r)
#define PSB_RSGX32(r) reg_read(r)
#define PSB_WVDC32(v,r) do{(void)(v);(void)(r);mmio();writes++;}while(0)
#define REG_READ(r) reg_read(r)
#define REG_WRITE(v,r) PSB_WVDC32(v,r)
#define spin_lock(m) ((void)pthread_mutex_lock(m))
#define spin_unlock(m) ((void)pthread_mutex_unlock(m))
#define rmb() ((void)0)
static unsigned long sgx535_gma500_fixed_irq_lock(void){assert(!source_order);source_order=1;return 1;}
static void sgx535_gma500_fixed_irq_capture_locked(struct drm_device *d,u32 a,u32 b)
{assert(d==&device&&a==0x40&&!b&&source_order==1);captures++;source_order=2;}
static void psb_sgx_interrupt(struct drm_device *d,u32 a,u32 b)
{assert(d==&device&&a==0x40&&!b&&source_order==2);acks++;source_order=3;mmio();}
static void sgx535_gma500_fixed_irq_unlock(unsigned long f){assert(f==1&&source_order==3);source_order=0;}
static void psb_vdc_interrupt(struct drm_device *d,u32 status)
{assert(d==&device&&status);assert(gma_power_is_on(d));mmio();}
static struct drm_device *bl_get_data(struct backlight_device *bd){assert(bd==&backlight);return bd->dev;}
static int psb_set_brightness(struct backlight_device *bd);
static int backlight_update_status(struct backlight_device *bd)
{callback_depth++;seam(pause_callback);int ret=psb_set_brightness(bd);callback_depth--;return ret;}
static int brightness_backend(struct drm_device *d,int level)
{assert(d==&device&&level>=1);brightness_calls++;seam(pause_brightness);mmio();return 0;}
#define psb_lvds_i2c_set_brightness brightness_backend
#define psb_lvds_pwm_set_brightness brightness_backend
static int psb_brightness;
#include "async_actual.inc"
static void fixture(void)
{
 assert(!refs&&!entries&&!source_order);active=true;transition=unplugged=false;
 pause_irq=pause_brightness=pause_callback=paused=released=false;
 reads=writes=bad_mmio=captures=acks=brightness_calls=0;worker_result=0;
 registered=true;unregister_started=unregister_done=false;
 private.vdc_irq_mask=~0U;private.ops=&ops;private.opregion.asle=&asle;
 private.backlight_device=&backlight;private.lvds_bl=&bl;private.backlight_enabled=true;
 backlight.props.max_brightness=100;backlight.props.brightness=50;asle.cblv=123;bl.type=BLC_I2C_TYPE;
}
static void wait_pause(void)
{pthread_mutex_lock(&control);while(!paused)pthread_cond_wait(&change,&control);pthread_mutex_unlock(&control);}
static void release(void)
{pthread_mutex_lock(&control);released=true;pthread_cond_broadcast(&change);pthread_mutex_unlock(&control);}
static void *irq_worker(void *arg)
{(void)arg;interrupt_context=true;worker_result=psb_irq_handler(1,&device);return NULL;}
static void *brightness_worker(void *arg)
{(void)arg;worker_result=(int)asle_set_backlight(&device,ASLE_BCLP_VALID|128);return NULL;}
/* Target sysfs holds ops_lock across update_status; unregister takes the
 * same lock before clearing ops. These two dependency bodies model that join. */
static int sysfs_write(void)
{pthread_mutex_lock(&ops_lock);int ret=registered?backlight_update_status(&backlight):-ENODEV;pthread_mutex_unlock(&ops_lock);return ret;}
static void *sysfs_worker(void *unused)
{(void)unused;worker_result=sysfs_write();return NULL;}
static void *unregister_worker(void *unused)
{
 (void)unused;pthread_mutex_lock(&control);unregister_started=true;
 pthread_cond_broadcast(&change);pthread_mutex_unlock(&control);
 pthread_mutex_lock(&ops_lock);registered=false;pthread_mutex_unlock(&ops_lock);
 pthread_mutex_lock(&control);unregister_done=true;pthread_cond_broadcast(&change);
 pthread_mutex_unlock(&control);return NULL;
}
int main(void)
{
 (void)gma_power_begin;(void)gma_power_end;(void)drm_dev_enter;(void)drm_dev_exit;
 assert(!pthread_mutex_init(&private.irqmask_lock,NULL));
 /* Already-running actual handler must prevent the modeled PM close. */
 fixture();pause_irq=true;pthread_t thread;assert(!pthread_create(&thread,NULL,irq_worker,NULL));wait_pause();
 assert(mock_suspend()==(EXPECT_GUARDS?-EBUSY:0));release();assert(!pthread_join(thread,NULL));
 assert(worker_result==IRQ_HANDLED&&captures==1&&acks==1&&!source_order);
 assert(EXPECT_GUARDS?!bad_mmio:bad_mmio>0);
 /* Offline shared IRQ must not touch/ACK status or invent completion. */
 fixture();active=false;assert(psb_irq_handler(1,&device)==(EXPECT_GUARDS?IRQ_NONE:IRQ_HANDLED));
 assert(EXPECT_GUARDS?(!reads&&!writes&&!captures&&!acks):(bad_mmio>0&&captures==1&&acks==1));
 /* Actual ASLE -> generic backlight -> platform callback -> LVDS dispatch. */
 fixture();pause_brightness=true;assert(!pthread_create(&thread,NULL,brightness_worker,NULL));wait_pause();
 assert(mock_suspend()==(EXPECT_GUARDS?-EBUSY:0));release();assert(!pthread_join(thread,NULL));
 assert(!worker_result&&brightness_calls==1);assert(EXPECT_GUARDS?!bad_mmio:bad_mmio>0);
 fixture();active=false;assert(asle_set_backlight(&device,ASLE_BCLP_VALID|128)==(EXPECT_GUARDS?ASLE_BACKLIGHT_FAILED:0));
 assert(EXPECT_GUARDS?(asle.cblv==123&&!brightness_calls):(asle.cblv!=123&&bad_mmio>0));
 fixture();active=false;assert(psb_set_brightness(&backlight)==(EXPECT_GUARDS?-EAGAIN:0));
 assert(EXPECT_GUARDS?!brightness_calls:bad_mmio>0);
 /* Unplug is distinct from power availability; deny before owner/MMIO use. */
 fixture();unplugged=true;assert(!psb_set_brightness(&backlight));
 assert(asle_set_backlight(&device,ASLE_BCLP_VALID|128)==(EXPECT_GUARDS?ASLE_BACKLIGHT_FAILED:0));
 assert(brightness_calls==(EXPECT_GUARDS?1U:2U));
 /* Pre-unplug ASLE entry may complete its nested callback without a hidden
  * nested DRM denial. Detach must join this entry before destroying owners. */
 fixture();pause_callback=true;assert(!pthread_create(&thread,NULL,brightness_worker,NULL));wait_pause();
 assert(entries==(EXPECT_GUARDS?1U:0U)&&refs==(EXPECT_GUARDS?1U:0U));
 pthread_mutex_lock(&admission);unplugged=true;pthread_mutex_unlock(&admission);
 release();assert(!pthread_join(thread,NULL));assert(!worker_result&&brightness_calls==1);
 assert(!entries&&!refs);
 /* Planned detach retains mappings while unregister joins admitted sysfs.
  * Future sysfs writes are denied after the registration owner is removed. */
 fixture();pause_brightness=true;assert(!pthread_create(&thread,NULL,sysfs_worker,NULL));wait_pause();
 pthread_t unregister_thread;assert(!pthread_create(&unregister_thread,NULL,unregister_worker,NULL));
 pthread_mutex_lock(&control);while(!unregister_started)pthread_cond_wait(&change,&control);
 assert(!unregister_done);pthread_mutex_unlock(&control);
 release();assert(!pthread_join(thread,NULL));assert(!pthread_join(unregister_thread,NULL));
 assert(!worker_result&&unregister_done&&brightness_calls==1);
 assert(sysfs_write()==-ENODEV&&brightness_calls==1);
 /* Native PM owner restore callbacks still nest; borrowed current IRQ cannot. */
 fixture();transition=true;pm_owner=true;
 assert(!psb_set_brightness(&backlight));assert(!asle_set_backlight(&device,ASLE_BCLP_VALID|128));
 assert(brightness_calls==2&&!bad_mmio);interrupt_context=true;
 assert(psb_irq_handler(1,&device)==(EXPECT_GUARDS?IRQ_NONE:IRQ_HANDLED));
 pm_owner=interrupt_context=false;assert(!refs&&!entries);
 puts(EXPECT_GUARDS?"ASYNC_POWER_GUARDS_MODEL_PASS actual handler/brightness chain, dependencies modeled; GPU0":"ASYNC_POWER_COUNTEREXAMPLES_CONFIRMED actual raw handler/brightness chain, dependencies modeled; NOT safety pass, GPU0");
 return 0;
}
