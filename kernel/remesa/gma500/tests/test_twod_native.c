/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual native adapter, CPU MMIO/clock model; no CPU pixel rendering. */
#define main reservation_test_main
#include "test_twod_reservation.c"
#undef main
#include <unistd.h>
#include <sys/wait.h>
#include <stdarg.h>
#include "native_source_defs.h"
#include "../remesa_twod_native.h"
struct psb_mmu_pd {struct drm_device *dev;u32 root;int error;};
int psb_mmu_default_root(struct drm_device *dev,struct psb_mmu_pd *pd,u32 *out)
{if(pd->error)return pd->error;CHECK(pd->dev==dev);*out=pd->root;return 0;}
static u32 registers[0x5000/4];
static unsigned mode,base_writes,acks,sleeps,barriers,route_calls,native_reads;
static u64 clock_ns;
static struct remesa_twod_native *active_native;
static char failure_log[512];
static void native_failure_log(const char *format,...)
{
 CHECK(!irq_lock_held&&!active_native->reservation.device->lock_2d.held);
 va_list args;va_start(args,format);vsnprintf(failure_log,sizeof failure_log,format,args);va_end(args);
}
#define pr_err(...) native_failure_log(__VA_ARGS__)
static u64 ktime_get_ns(void){clock_ns+=100000;return clock_ns;}
static void usleep_range(unsigned a,unsigned b)
{CHECK(a==50&&b==100&&!irq_lock_held);clock_ns+=100000;sleeps++;}
static void dma_rmb(void){barriers++;}
static u32 get_unaligned_le32(const void *p)
{const unsigned char *b=p;return (u32)b[0]|(u32)b[1]<<8|(u32)b[2]<<16|(u32)b[3]<<24;}
static void remesa_route_source_pending(struct drm_device *dev,u32 s1,u32 s2)
{CHECK(dev&&irq_lock_held);(void)s1;(void)s2;route_calls++;}
#include "fixed_capture_actual.inc"
static u32 native_read(struct drm_psb_private *p,u32 reg)
{
 CHECK(irq_lock_held&&p->dev->powered);native_reads++;
 if(reg==PSB_CR_2D_SOCIF||reg==PSB_CR_2D_BLIT_STATUS||reg==PSB_CR_BIF_TWOD_REQ_BASE||
    reg>=PSB_SGX_2D_SLAVE_PORT)return model_read(p,reg);
 CHECK(reg<sizeof registers);return registers[reg/4];
}
static void native_write(struct drm_psb_private *p,u32 value,u32 reg)
{
 CHECK(irq_lock_held);
 if(reg==PSB_CR_BIF_TWOD_REQ_BASE){
  CHECK(p->lock_2d.held);base_writes++;
  if((mode==8&&base_writes==1)||(mode==9&&base_writes==2))return;
  p->base=value;
  if(mode==21&&base_writes==1){registers[PSB_CR_BIF_INT_STAT/4]=4;registers[PSB_CR_BIF_FAULT/4]=value;}
  return;
 }
 if(reg==PSB_CR_EVENT_HOST_CLEAR){
  CHECK(value==_PSB_CE_TWOD_COMPLETE);acks++;
  if(mode!=7)registers[PSB_CR_EVENT_STATUS/4]&=~value;
  return;
 }
 CHECK(p->lock_2d.held&&active_native->reservation.irq_issued&&active_native->cookie&&producer_depth==1);
 model_write(p,value,reg);
 if(p->words%10==0){
  if(mode!=10)registers[PSB_CR_EVENT_STATUS/4]|=_PSB_CE_TWOD_COMPLETE;
  if(mode==11)registers[PSB_CR_EVENT_STATUS2/4]|=1u<<4;
  if(mode==12)p->blit=_PSB_C2B_STATUS_BUSY;
  if(mode==13)registers[PSB_CR_BIF_INT_STAT/4]=1u<<4;
 }
}
#undef PSB_RSGX32
#undef PSB_WSGX32
#define PSB_RSGX32(reg) native_read(dev_priv,(reg))
#define PSB_WSGX32(value,reg) native_write(dev_priv,(value),(reg))
#include "../remesa_twod_native.inc"
#include "remesa_twod.c"
static void native_case(unsigned width,unsigned height,unsigned fault)
{
 struct drm_device dev={.powered=1};
 struct drm_psb_private p={.dev=&dev,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x28000000};
 struct psb_mmu_pd pd={&dev,0x12345000,0};
 struct remesa_twod_native native={0};struct remesa_twod_completion receipt;
 struct remesa_twod_launch launch={.dev=&dev,.pd=&pd,.ticket={77,1,0,1},.count=2};
 struct remesa_twod_copy copy={.source={1,0x40001000,0x00fff000,width,height,width*4,1},
  .destination={2,0x41000000,0x01000000,width,height,width*4,1},.request_base=0x40000000,.width=width,.height=height};
 CHECK(!remesa_twod_copy_prepare(&copy,&launch.plan));
 dev.dev_private=&p;active_native=&native;mode=fault;
 memset(registers,0,sizeof registers);failure_log[0]=0;base_writes=acks=sleeps=barriers=0;clock_ns=0;
 registers[PSB_CR_CORE_ID/4]=0x01130000;registers[PSB_CR_CORE_REVISION/4]=0x00010201;
 registers[PSB_CR_BIF_DIR_LIST_BASE0/4]=pd.root;
 registers[PSB_CR_EVENT_STATUS/4]=_PSB_CE_TWOD_COMPLETE; /* stale event */
 if(fault==1)registers[PSB_CR_CORE_ID/4]^=1;
 if(fault==2)registers[PSB_CR_CORE_REVISION/4]^=1;
 if(fault==3)registers[PSB_CR_BIF_DIR_LIST_BASE0/4]^=4096;
 if(fault==4)registers[PSB_CR_BIF_BANK0/4]=1;
 if(fault==5)registers[PSB_CR_BIF_CTRL/4]=_PSB_MMU_ER_MASK;
 if(fault==6)registers[PSB_CR_EVENT_STATUS2/4]=1u<<4;
 if(fault==14)registers[SGX535_SOURCE_EVENT_TIMER_REG/4]=SGX535_SOURCE_EVENT_TIMER_ENABLE;
 if(fault==15)registers[PSB_CR_SOFT_RESET/4]=1;
 for(unsigned iteration=0;iteration<(fault?1u:3u);iteration++){
  u64 cookie=0;unsigned words=p.words,initial_base_writes=base_writes;
  int error=remesa_twod_native_executor.start(&native,&launch,&cookie);
  if(fault==21){
   CHECK(error==-EIO&&native.failed&&native.reservation.held&&!p.words&&!cookie&&!producer_depth);
#ifdef REMESA_TWOD_LAB_IOCTL
   CHECK(strstr(failure_log,"phase=5 words=0")&&strstr(failure_log,"bif=00000004 addr=40000000"));
#endif
   return;
  }
  if((fault>=1&&fault<=8)||fault==14||fault==15){
   CHECK(error&&native.failed&&native.reservation.held&&!p.words&&!cookie);
#ifdef REMESA_TWOD_LAB_IOCTL
   if(fault==1||fault==2)CHECK(strstr(failure_log,"valid=0"));
#endif
   CHECK(!p.resets);return;
  }
  if(fault==13){CHECK(error==-EIO&&cookie&&p.words==10&&native.reservation.held);return;}
  CHECK(!error&&cookie==iteration+1&&native.reservation.held&&p.words==words+10);
  CHECK(base_writes==initial_base_writes+1&&p.base==copy.request_base);
  for(unsigned i=0;i<10;i++)CHECK(p.packet[words+i]==get_unaligned_le32(launch.plan.command+4*i));
  CHECK(remesa_twod_native_executor.retire(&native,&launch,cookie)==-EBUSY);
  memset(&receipt,0xa5,sizeof receipt);struct remesa_twod_completion old=receipt;
  CHECK(remesa_twod_native_executor.poll(&native,&launch,cookie+1,1,&receipt)==-EACCES);
  CHECK(!memcmp(&receipt,&old,sizeof old));
  unsigned reads=native_reads;
  if(fault==18)dev.powered=0;
  error=remesa_twod_native_executor.poll(&native,&launch,cookie,1,&receipt);
  if(fault==18){CHECK(error==-EIO&&native.failed&&native.reservation.held&&native_reads==reads);return;}
  if(fault==10||fault==11||fault==12){
   CHECK(error==(fault==11?-EIO:-ETIMEDOUT)&&native.failed&&native.reservation.held);
   CHECK(!memcmp(&receipt,&old,sizeof old)&&!p.resets);return;
  }
  CHECK(!error&&native.complete&&receipt.event==_PSB_CE_TWOD_COMPLETE&&receipt.cookie==cookie);
  if(fault==16)registers[PSB_CR_BIF_INT_STAT/4]=1;
  if(fault==19)dev.powered=0;
  reads=native_reads;
  error=remesa_twod_native_executor.visible(&native,&launch,cookie);
  if(fault==19){CHECK(error==-EIO&&native.failed&&native.reservation.held&&native_reads==reads);return;}
  if(fault==16){CHECK(error==-EIO&&native.failed&&native.reservation.held);return;}
  CHECK(!error&&native.visible&&barriers==iteration+1);
  if(fault==17)registers[PSB_CR_EVENT_STATUS2/4]=1;
  if(fault==20)dev.powered=0;
  reads=native_reads;
  error=remesa_twod_native_executor.retire(&native,&launch,cookie);
  if(fault==20){CHECK(error==-EIO&&native.failed&&native.reservation.held&&native_reads==reads);return;}
  if(fault==9||fault==17){CHECK(error==-EIO&&native.failed&&native.reservation.held);return;}
  CHECK(!error&&!native.live&&!native.reservation.held&&!twod_irq_owner&&!p.remesa_twod_owner);
  CHECK(p.base==0x28000000&&base_writes==initial_base_writes+2&&!p.resets);
  launch.ticket.operation++;
 }
 CHECK(!irq_lock_held&&!p.lock_2d.held&&route_calls>0);
}
int main(void)
{
 CHECK(!reservation_test_main());
 native_case(1280,800,0);native_case(640,640,0);native_case(641,479,0);
 for(unsigned fault=1;fault<=21;fault++){
  pid_t child=fork();CHECK(child>=0);
  if(!child){native_case(32,32,fault);CHECK(!producer_depth);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 printf("PASS %u parent native adapter checks plus 21 joined MMIO fault models; no GPU execution\n",checks);
 return 0;
}
