/* Actual provider branch and fault policy with synthetic PTE model. GPU0. */
#define REMESA_MEMORY_TEST
#include "../module/remesa_gem_memory.c"
#include <stdio.h>
static struct remesa_gem_memory memory_state;
static struct psb_mmu_pd test_pd;
static struct page page_objects[3],*wanted[3];
static struct remesa_memory_binding binding={32,0x20000000,3*4096};
static unsigned char commands[4]={0};
static int guard(u64 o,const void *p,unsigned n){(void)o;(void)p;(void)n;return 0;}
static void fixture(bool fast){
 memset(&memory_state,0,sizeof(memory_state));memset(test_pt,0,sizeof(test_pt));mutex_init(&memory_state.lock);memory_state.context=7;memory_state.pd=&test_pd;memory_state.pds_write_guard=fast?guard:NULL;
 struct backing *b=&memory_state.bo[0];b->live=b->pinned=b->reserved=true;b->generation=1;b->gem.size=3*4096;b->va.start=binding.va;b->inserted=3;b->pages=wanted;
 for(unsigned i=0;i<3;i++){page_objects[i].pfn=0x1000+i;wanted[i]=&page_objects[i];test_pt[i].live=true;test_pt[i].va=binding.va+(u64)i*4096;test_pt[i].pfn=page_objects[i].pfn;}batch_verify_calls=0;
}
static int prepare(struct remesa_memory_ticket *t){return remesa_gem_job_prepare(&memory_state,&binding,1,commands,sizeof(commands),t);}
int main(void){
 unsigned checks=0;
 for(unsigned fast=0;fast<=1;fast++){
  struct remesa_memory_ticket ticket;struct remesa_memory_binding actual[16];fixture(fast);
  assert(!prepare(&ticket)&&batch_verify_calls==fast);struct retained_job *j=ticket_job(&memory_state,ticket);assert(j);
  for(unsigned i=0;i<4;i++){assert(!verify_job_bindings(&memory_state,j,actual));assert(actual[0].id==binding.id&&actual[0].va==binding.va&&actual[0].bytes==binding.bytes);assert(batch_verify_calls==fast*(i+2));++checks;}
  memory_state.pds_write_guard=NULL;unsigned before=batch_verify_calls;assert(!verify_job_bindings(&memory_state,j,actual)&&batch_verify_calls==before);assert(!remesa_gem_job_cancel(&memory_state,ticket));++checks;
  for(unsigned bad=0;bad<3;bad++){
   fixture(fast);test_pt[bad].pfn^=1;assert(prepare(&ticket)==-EIO&&memory_state.bo[0].quarantine&&!memory_state.jobs[0].live);++checks;
   fixture(fast);assert(!prepare(&ticket));j=ticket_job(&memory_state,ticket);test_pt[bad].live=false;assert(verify_job_bindings(&memory_state,j,actual)==-EIO);test_pt[bad].live=true;assert(!remesa_gem_job_cancel(&memory_state,ticket));++checks;
  }
  fixture(fast);memory_state.bo[0].lease=true;assert(prepare(&ticket)==-EBUSY&&!batch_verify_calls);++checks;
 }
 printf("PASS actual provider PTE routing: %u cases, OPEN8 seal selects complete batch, legacy and cleared seal use old loop, every tested PFN/missing mapping rejects, prepare quarantine/busy preserved; CPU/GPU0\n",checks);return 0;
}
