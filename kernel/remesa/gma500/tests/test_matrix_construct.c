/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual memory admission block and backing lease. Metadata/locks modeled;
 * ordinary act() validation still follows this gate in the real service. */
#define main inherited_backing_tests
#include "test_private_session.c"
#undef main
int main(void)
{
 CHECK(!inherited_backing_tests());fixture();struct remesa_gem_memory *m=NULL;
 CHECK(!acquire(&m)&&m==&s);s.private_matrix_color=32;s.private_matrix_constructing=true;
 const unsigned actions[]={REMESA_MEM_CREATE,REMESA_MEM_PIN,REMESA_MEM_MAP,REMESA_MEM_UNMAP,
  REMESA_MEM_UNPIN,REMESA_MEM_DESTROY,REMESA_MEM_LEASE,REMESA_MEM_READ,REMESA_MEM_WRITE,REMESA_MEM_UNLEASE,REMESA_MEM_SYNC};
 for(unsigned i=0;i<sizeof actions/sizeof actions[0];i++){
  struct remesa_memory_request r={.action=actions[i],.id=33};
  CHECK(matrix_cpu_construct(&s,&r)&&!session_gate(&s,&r));
  r.id=32;
  bool read=actions[i]==REMESA_MEM_LEASE||actions[i]==REMESA_MEM_READ||actions[i]==REMESA_MEM_UNLEASE||actions[i]==REMESA_MEM_SYNC;
  CHECK(matrix_color_protected_write(&s,&r)==!read);
  CHECK(session_gate(&s,&r)==(read?0:-EPERM));
 }
 struct remesa_memory_request r={.action=REMESA_MEM_LEASE,.id=32,.flags=1};
 CHECK(session_gate(&s,&r)==-EPERM);r.action=REMESA_MEM_WRITE;CHECK(session_gate(&s,&r)==-EPERM);
 r=(struct remesa_memory_request){.action=REMESA_MEM_CREATE};
 s.private_matrix_constructing=false;CHECK(session_gate(&s,&r)==-EBUSY);s.private_matrix_constructing=true;
 for(unsigned i=0;i<8;i++){s.jobs[i].live=true;CHECK(!matrix_cpu_construct(&s,&r)&&session_gate(&s,&r)==-EBUSY);s.jobs[i].live=false;}
 s.stream_bound=true;CHECK(!matrix_cpu_construct(&s,&r)&&session_gate(&s,&r)==-EBUSY);s.stream_bound=false;
 s.executor=(void*)1;CHECK(!matrix_cpu_construct(&s,&r));s.executor=NULL;
 s.ta_cache_domains=true;CHECK(!matrix_cpu_construct(&s,&r));s.ta_cache_domains=false;
 s.private_device_lost=true;CHECK(!matrix_cpu_construct(&s,&r));s.private_device_lost=false;
 s.private_source_poisoned=true;CHECK(!matrix_cpu_construct(&s,&r));s.private_source_poisoned=false;
 s.file=NULL;CHECK(!matrix_cpu_construct(&s,&r));s.file=&file;
 s.private_matrix_color=0;CHECK(!matrix_cpu_construct(&s,&r));s.private_matrix_color=32;
 s.private_session=NULL;CHECK(!matrix_cpu_construct(&s,&r));s.private_session=&owner;
 s.private_matrix_depth=34;
 for(unsigned i=0;i<sizeof actions/sizeof actions[0];i++){
  r=(struct remesa_memory_request){.action=actions[i],.id=34};
  bool read=actions[i]==REMESA_MEM_LEASE||actions[i]==REMESA_MEM_READ||actions[i]==REMESA_MEM_UNLEASE||actions[i]==REMESA_MEM_SYNC;
  CHECK(matrix_cpu_construct(&s,&r)==read&&matrix_color_protected_write(&s,&r)==!read);
 }
 r=(struct remesa_memory_request){.action=REMESA_MEM_LEASE,.id=34,.flags=1};
 CHECK(!matrix_cpu_construct(&s,&r)&&matrix_color_protected_write(&s,&r));
 r=(struct remesa_memory_request){.action=REMESA_MEM_CREATE};CHECK(matrix_cpu_construct(&s,&r));
 r.action=999;CHECK(!matrix_cpu_construct(&s,&r)&&session_gate(&s,&r)==-EBUSY);
 CHECK(remesa_gem_memory_close(&s)==-EBUSY&&!cleanup);
 CHECK(!remesa_private_session_release(&s,&owner));
 CHECK(!s.private_matrix_constructing&&!s.private_matrix_color&&!s.private_matrix_depth&&!s.private_session);
 puts("PASS normal CPU construction admission, protected COLOR, active/frozen/foreign-state refusals; no source/GPU authority");
 return 0;
}
