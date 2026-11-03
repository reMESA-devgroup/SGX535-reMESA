/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual native adapter, CPU MMIO/clock model; no CPU pixel rendering. */
#define main reservation_test_main
#include "test_twod_reservation.c"
#undef main
#include <unistd.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <stdlib.h>
#include "native_source_defs.h"
#include "../remesa_twod_native.h"
struct psb_mmu_pd {struct drm_device *dev;u32 root;int error;};
int psb_mmu_default_root(struct drm_device *dev,struct psb_mmu_pd *pd,u32 *out)
{if(pd->error)return pd->error;CHECK(pd->dev==dev);*out=pd->root;return 0;}
static u32 registers[0x5000/4];
static unsigned mode,base_writes,acks,sleeps,barriers,route_calls,native_reads,power_loss_reads;
static u64 clock_ns;
static unsigned packet_words=10,alpha_writes,initial_alpha,base_sleeps;
static bool record_retirement;
#ifdef REMESA_TWOD_VENDOR_STREAM
static int vendor_stream;
#endif
static struct remesa_twod_native *active_native;
static char failure_log[512];
#ifdef REMESA_TWOD_LAB_IOCTL
static void native_failure_log(const char *format,...)
{
 CHECK(!irq_lock_held&&!active_native->reservation.device->lock_2d.held);
 va_list args;va_start(args,format);vsnprintf(failure_log,sizeof failure_log,format,args);va_end(args);
}
#define pr_err(...) native_failure_log(__VA_ARGS__)
#endif
static u64 ktime_get_ns(void){clock_ns+=100000;return clock_ns;}
static void base_observe_sleep(void);
static void usleep_range(unsigned a,unsigned b)
{
 if(a==1000&&b==1500){base_observe_sleep();return;}
 CHECK(a==50&&b==100&&!irq_lock_held);clock_ns+=100000;sleeps++;
}
static void dma_rmb(void){barriers++;}
static u32 get_unaligned_le32(const void *p)
{const unsigned char *b=p;return (u32)b[0]|(u32)b[1]<<8|(u32)b[2]<<16|(u32)b[3]<<24;}
#ifdef REMESA_OWNED_FILL_NATIVE_TEST
static void owned_pending_forward(struct drm_device *,u32,u32);
static bool owned_publication_live(void);
#define NATIVE_PRODUCER_LIVE() (producer_depth==1||owned_publication_live())
#else
#define NATIVE_PRODUCER_LIVE() (producer_depth==1)
#endif
static void remesa_route_source_pending(struct drm_device *dev,u32 s1,u32 s2)
{
 CHECK(dev&&irq_lock_held);(void)s1;(void)s2;route_calls++;
#ifdef REMESA_OWNED_FILL_NATIVE_TEST
 owned_pending_forward(dev,s1,s2);
#endif
}
#include "fixed_capture_actual.inc"
static void base_observe_sleep(void)
{
 struct drm_psb_private *p=active_native->reservation.device;
 struct remesa_twod_reservation other={0};base_sleeps++;
 CHECK(p&&!irq_lock_held&&!p->lock_2d.held&&producer_depth==1);
 CHECK(active_native->reservation.held&&active_native->reservation.irq_issued&&active_native->cookie);
 CHECK(p->remesa_twod_owner==&active_native->reservation&&twod_irq_owner==p->remesa_twod_owner);
 CHECK(remesa_twod_reserve(p,&other)==-EBUSY&&!other.held);
 clock_ns+=1000000;
 if(mode==34){registers[PSB_CR_BIF_INT_STAT/4]=0x4004;registers[PSB_CR_BIF_FAULT/4]=p->base;}
 if(mode==35){power_loss_reads=native_reads;p->dev->powered=0;}
 if(mode==36||mode==37){
  u32 event1=mode==36?_PSB_CE_TWOD_COMPLETE:0,event2=mode==37?1u<<4:0;
  unsigned long flags=sgx535_gma500_fixed_irq_lock();
  sgx535_gma500_fixed_irq_capture_locked(p->dev,event1,event2);
  /* IRQ consumed raw status; owned ledger must still reject before FIFO. */
  registers[PSB_CR_EVENT_STATUS/4]=registers[PSB_CR_EVENT_STATUS2/4]=0;
  sgx535_gma500_fixed_irq_unlock(flags);
 }
 if(mode==41)registers[0x0e1c/4]^=1;
 if(mode==38)registers[PSB_CR_BIF_DIR_LIST_BASE0/4]^=4096;
}
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
 if(reg==0x0e1c){
  CHECK(p->lock_2d.held&&NATIVE_PRODUCER_LIVE()&&(packet_words==9||packet_words==7));
  alpha_writes++;
  if(alpha_writes%2==1)CHECK(p->words%packet_words==0&&!active_native->cookie&&value==0xff00);
  else CHECK(active_native->complete&&active_native->visible&&value==initial_alpha);
  if((mode==40&&alpha_writes==1)||(mode==42&&alpha_writes==2))return;
  registers[reg/4]=value;
  if(mode==43||(mode==49&&alpha_writes==2)){registers[PSB_CR_BIF_INT_STAT/4]=4;registers[PSB_CR_BIF_FAULT/4]=p->base;}
  if(mode==44||(mode==48&&alpha_writes==2)){power_loss_reads=native_reads;p->dev->powered=0;}
  if(mode==45||(mode==50&&alpha_writes==2))registers[PSB_CR_EVENT_STATUS2/4]=1u<<4;
  return;
 }
 if(reg==PSB_CR_BIF_TWOD_REQ_BASE){
  CHECK(p->lock_2d.held);base_writes++;
  if((mode==8&&base_writes==1)||(mode==9&&base_writes==2))return;
  p->base=value;
#ifdef REMESA_OWNED_FILL_NATIVE_TEST
  if(base_writes==1&&mode==51)registers[PSB_CR_EVENT_STATUS/4]|=_PSB_CE_TWOD_COMPLETE;
  if(base_writes==1&&mode==52)registers[PSB_CR_EVENT_STATUS/4]|=1;
#endif
  if(mode==21&&base_writes==1){registers[PSB_CR_BIF_INT_STAT/4]=4;registers[PSB_CR_BIF_FAULT/4]=value;}
  return;
 }
 if(reg==PSB_CR_EVENT_HOST_CLEAR){
  CHECK(value==_PSB_CE_TWOD_COMPLETE);acks++;
  if(mode!=7)registers[PSB_CR_EVENT_STATUS/4]&=~value;
  return;
 }
 CHECK(p->lock_2d.held&&active_native->reservation.irq_issued&&active_native->cookie&&NATIVE_PRODUCER_LIVE());
 model_write(p,value,reg);
 if(mode>=22&&mode<=31&&p->words==mode-21){
  registers[PSB_CR_BIF_INT_STAT/4]=0x4004;
  registers[PSB_CR_BIF_FAULT/4]=p->base;
 }
 if(mode==32&&p->words==3)registers[PSB_CR_EVENT_STATUS2/4]=1u<<4;
 if(mode==33&&p->words==3){power_loss_reads=native_reads;p->dev->powered=0;}
 if(p->words%packet_words==0){
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
#ifdef REMESA_TWOD_FILL_STREAM
static bool fill_stream;
#endif
static void native_case(unsigned width,unsigned height,unsigned fault)
{
 struct drm_device dev={.powered=1};
 struct drm_psb_private p={.dev=&dev,.sgx_reg=(void*)1,.fifo=_PSB_C2_SOCIF_EMPTY,.base=0x28000000};
 struct psb_mmu_pd pd={&dev,0x12345000,0};
 struct remesa_twod_native native={0};struct remesa_twod_completion receipt;
 struct remesa_twod_retirement proof={0};
 struct remesa_twod_launch launch={.dev=&dev,.pd=&pd,.ticket={77,1,0,1},.count=2};
 if(record_retirement)launch.retirement=&proof;
 struct remesa_twod_copy copy={.source={1,0x40001000,0x00fff000,width,height,width*4,1},
  .destination={2,0x41000000,0x01000000,width,height,width*4,1},.request_base=0x40000000,.width=width,.height=height};
#ifdef REMESA_TWOD_FILL_STREAM
 if(fill_stream){
  struct remesa_twod_fill fill={.destination=copy.destination,.request_base=copy.request_base,
   .width=width,.height=height,.color=0x7f123456};
  CHECK(!remesa_twod_fill_prepare(&fill,&launch.plan));launch.count=1;
 }else
#endif
#ifdef REMESA_TWOD_VENDOR_STREAM
 if(vendor_stream)CHECK(!remesa_twod_copy_prepare_vendor(&copy,&launch.plan));
 else
#endif
 CHECK(!remesa_twod_copy_prepare(&copy,&launch.plan));
 dev.dev_private=&p;active_native=&native;mode=fault;
 memset(registers,0,sizeof registers);failure_log[0]=0;base_writes=acks=sleeps=barriers=alpha_writes=base_sleeps=0;clock_ns=0;
 registers[0x0e1c/4]=fault==39?0x00010000u:initial_alpha;
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
  if((fault>=34&&fault<=38)||fault==41){
   CHECK(error==-EIO&&native.failed&&native.reservation.held&&cookie&&!p.words);
   CHECK(!producer_depth&&!irq_lock_held&&!p.lock_2d.held&&!p.resets);
   CHECK(!native.complete&&!native.visible&&twod_irq_owner&&p.remesa_twod_owner);
   if(fault==35)CHECK(native_reads==power_loss_reads);
#ifdef REMESA_TWOD_LAB_IOCTL
   CHECK(strstr(failure_log,fault==35?"phase=10 words=0 valid=0":"phase=10 words=0 valid=1"));
#endif
   if(fault==36)CHECK(native.reservation.pending1==_PSB_CE_TWOD_COMPLETE&&!native.reservation.pending2);
   if(fault==37)CHECK(!native.reservation.pending1&&native.reservation.pending2==(1u<<4));
   return;
  }
  if(fault>=22&&fault<=33){
   unsigned stop=fault<=31?fault-21:3;
   CHECK(error==-EIO&&native.failed&&native.reservation.held&&cookie&&p.words==stop);
   CHECK(!producer_depth&&!irq_lock_held&&!p.lock_2d.held&&!p.resets);
   CHECK(!native.complete&&!native.visible&&twod_irq_owner&&p.remesa_twod_owner);
   if(fault==33)CHECK(native_reads==power_loss_reads);
#ifdef REMESA_TWOD_LAB_IOCTL
   char boundary[64];snprintf(boundary,sizeof boundary,"phase=9 words=%u valid=%u",stop,fault==33?0:1);
   CHECK(strstr(failure_log,boundary));
   if(fault<=31)CHECK(strstr(failure_log,"bif=00004004 addr=40000000"));
#endif
   return;
  }
  if(fault==40||(fault>=43&&fault<=45)){
   CHECK(error==-EIO&&native.failed&&native.reservation.held&&!p.words&&!cookie);
   CHECK(alpha_writes==1&&!base_writes&&!producer_depth&&!native.complete&&!native.visible);
   if(fault==44)CHECK(native_reads==power_loss_reads);
   return;
  }
  if(fault==21){
   CHECK(error==-EIO&&native.failed&&native.reservation.held&&!p.words&&!cookie&&!producer_depth);
#ifdef REMESA_TWOD_LAB_IOCTL
   CHECK(strstr(failure_log,"phase=5 words=0")&&strstr(failure_log,"bif=00000004 addr=40000000"));
#endif
   return;
  }
  if((fault>=1&&fault<=8)||fault==14||fault==15||fault==39){
   CHECK(error&&native.failed&&native.reservation.held&&!p.words&&!cookie);
#ifdef REMESA_TWOD_LAB_IOCTL
   if(fault==1||fault==2)CHECK(strstr(failure_log,"valid=0"));
#endif
   CHECK(!p.resets);return;
  }
  if(fault==13){CHECK(error==-EIO&&cookie&&p.words==packet_words&&native.reservation.held);return;}
#ifdef REMESA_TWOD_LAB_IOCTL
  if(fault==11){CHECK(error==-EIO&&cookie&&p.words==packet_words&&native.reservation.held);return;}
#endif
  CHECK(!error&&cookie==iteration+1&&native.reservation.held&&p.words==words+packet_words);
  CHECK(base_writes==initial_base_writes+1&&p.base==copy.request_base);
  for(unsigned i=0;i<packet_words;i++)CHECK(p.packet[words+i]==get_unaligned_le32(launch.plan.command+4*i));
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
  if(fault==46)registers[0x0e1c/4]^=1;
  reads=native_reads;
  error=remesa_twod_native_executor.visible(&native,&launch,cookie);
  if(fault==19){CHECK(error==-EIO&&native.failed&&native.reservation.held&&native_reads==reads);return;}
  if(fault==16||fault==46){CHECK(error==-EIO&&native.failed&&native.reservation.held);return;}
  CHECK(!error&&native.visible&&barriers==iteration+1);
  if(fault==17)registers[PSB_CR_EVENT_STATUS2/4]=1;
  if(fault==20)dev.powered=0;
  if(fault==47)registers[0x0e1c/4]^=1;
  reads=native_reads;
  error=remesa_twod_native_executor.retire(&native,&launch,cookie);
  if(error&&record_retirement)CHECK(!proof.native_retired&&!proof.gem_retired);
  if(fault==20){CHECK(error==-EIO&&native.failed&&native.reservation.held&&native_reads==reads);return;}
  if(fault==42||fault>=47){
   CHECK(error==-EIO&&native.failed&&native.reservation.held&&!producer_depth);
   CHECK(alpha_writes==(fault==47?1u:2u));
   if(fault==48)CHECK(native_reads==power_loss_reads);
   CHECK(!p.resets);return;
  }
  if(fault==9||fault==17){CHECK(error==-EIO&&native.failed&&native.reservation.held);return;}
  CHECK(!error&&!native.live&&!native.reservation.held&&!twod_irq_owner&&!p.remesa_twod_owner);
  if(record_retirement){
   CHECK(proof.native_retired&&!proof.gem_retired&&proof.dev==&dev&&proof.pd==&pd);
   CHECK(proof.ticket.context==launch.ticket.context&&proof.ticket.operation==launch.ticket.operation&&
    proof.ticket.slot==launch.ticket.slot&&proof.ticket.generation==launch.ticket.generation&&proof.cookie==cookie);
   CHECK(proof.destination_id==launch.plan.destination_id&&
    proof.destination_first==launch.plan.destination_first&&proof.destination_end==launch.plan.destination_end);
   CHECK(proof.default_root==pd.root&&proof.request_base==launch.plan.request_base&&proof.restored_request_base==0x28000000);
   CHECK(proof.packet_dwords==packet_words&&proof.core==0x01130000&&proof.revision==0x10201);
   CHECK(proof.status1==_PSB_CE_TWOD_COMPLETE&&!proof.status2);
   memset(&proof,0,sizeof proof);
  }
  CHECK(p.base==0x28000000&&base_writes==initial_base_writes+2&&!p.resets);
  CHECK(registers[0x0e1c/4]==initial_alpha);
  CHECK(alpha_writes==(packet_words!=10?2u*(iteration+1):0));
#ifdef REMESA_TWOD_LAB_IOCTL
  CHECK(base_sleeps==iteration+1);
#else
  CHECK(!base_sleeps);
#endif
  launch.ticket.operation++;
 }
 CHECK(!irq_lock_held&&!p.lock_2d.held&&route_calls>0);
}
#ifndef REMESA_NATIVE_FIXTURE_ONLY
int main(void)
{
 record_retirement=getenv("REMESA_TEST_TWOD_RETIREMENT")!=NULL;
 CHECK(!reservation_test_main());
 native_case(1280,800,0);native_case(640,640,0);native_case(641,479,0);
 unsigned faults=21;
#ifdef REMESA_TWOD_LAB_IOCTL
 faults=38;
#endif
 for(unsigned fault=1;fault<=faults;fault++){
  pid_t child=fork();CHECK(child>=0);
  if(!child){native_case(32,32,fault);CHECK(!producer_depth);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 }
#ifdef REMESA_TWOD_VENDOR_STREAM
 vendor_stream=1;packet_words=9;
 native_case(1280,800,0);native_case(640,640,0);native_case(641,479,0);
 initial_alpha=0xff00;native_case(32,32,0);
 initial_alpha=0x1234;native_case(32,32,0);initial_alpha=0;
 unsigned vendor_faults=0;
 for(unsigned fault=1;fault<=50;fault++){
#ifndef REMESA_TWOD_LAB_IOCTL
  if((fault>=22&&fault<=38)||fault==41)continue;
#endif
  if(fault==31)continue; /* word ten does not exist in this profile */
  vendor_faults++;
  pid_t child=fork();CHECK(child>=0);
  if(!child){native_case(32,32,fault);CHECK(!producer_depth);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 printf("PASS vendor nine-word profile: three dimensions, two saved-state fixtures and %u joined fault models including ALPHA publication, interference and restore failures; CPU only\n",vendor_faults);
#endif
#ifdef REMESA_TWOD_FILL_STREAM
 fill_stream=true;packet_words=7;
 native_case(1280,800,0);native_case(640,640,0);native_case(641,479,0);
 initial_alpha=0xff00;native_case(32,32,0);initial_alpha=0x1234;native_case(32,32,0);initial_alpha=0;
 unsigned fill_faults=0;
 for(unsigned fault=1;fault<=50;fault++){
  if((fault>=22&&fault<=38)||fault==41)continue;
  fill_faults++;pid_t child=fork();CHECK(child>=0);
  if(!child){native_case(32,32,fault);CHECK(!producer_depth);_exit(0);}
  int status;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 printf("PASS seven-word native fill: three dimensions, saved ALPHA and %u joined fault models; CPU only\n",fill_faults);
#endif
 printf("PASS %u parent native adapter checks plus %u joined MMIO fault models; no GPU execution\n",checks,faults);
 return 0;
}
#endif
