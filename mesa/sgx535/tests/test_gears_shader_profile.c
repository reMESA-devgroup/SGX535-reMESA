/* SPDX-License-Identifier: MIT. Actual-NIR host records and independent CPU
 * writer bytes arrive as files. No device, submission or float simulation. */
#include "remesa_shader_program.h"
#include "remesa_usse_arithmetic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void read_exact(const char*n,void*p,size_t bytes){FILE*f=fopen(n,"rb");assert(f&&fread(p,1,bytes,f)==bytes&&fgetc(f)==EOF&&!fclose(f));}
int main(void){
 unsigned char oracle[648],tail[8];read_exact("body-oracles.bin",oracle,sizeof(oracle));read_exact("tail-oracle.bin",tail,8);unsigned checks=0;
 for(unsigned attr=0;attr<2;attr++){
  struct remesa_shader_ir ir;read_exact(attr?"semantic-attr.bin":"semantic-state.bin",&ir,sizeof(ir));
  assert(!remesa_shader_ir_validate(&ir)&&!remesa_shader_ir_gears_vertex(&ir)&&ir.instructions[0].input_slot==attr);
  struct remesa_shader_program p;assert(!remesa_shader_program_prepare(&ir,&p)&&!remesa_shader_program_validate(&p)&&p.status==REMESA_PROGRAM_COMPILE_OFFLINE);
  struct remesa_shader_program_report report;assert(!remesa_shader_program_report(&p,&report)&&report.binary_bytes==320&&!report.binary_error&&report.launch_error==ENOTSUP);
  unsigned char out[336],before[336];memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));size_t n=777;
  assert(remesa_shader_program_encode(&p,out,319,&n)==ENOSPC&&n==777&&!memcmp(out,before,sizeof(out)));
  assert(!remesa_shader_program_encode(&p,out,sizeof(out),&n)&&n==320&&!memcmp(out,oracle+attr*324+12,312)&&!memcmp(out+312,tail,8)&&!memcmp(out+320,before+320,16));
  struct remesa_shader_selected_resources r,saved;memset(&r,0xa5,sizeof(r));saved=r;
  assert(remesa_shader_program_selected_resources(&p,&r)==ENOTSUP&&!memcmp(&r,&saved,sizeof(r)));
  for(unsigned word=0;word<sizeof(ir)/4;word++){
   struct remesa_shader_ir bad=ir;uint32_t v;memcpy(&v,(unsigned char*)&bad+4*word,4);v^=1;memcpy((unsigned char*)&bad+4*word,&v,4);
   struct remesa_shader_program dest,old;memset(&dest,0xa5,sizeof(dest));old=dest;
   assert(remesa_shader_ir_validate(&bad)!=0&&remesa_shader_program_prepare(&bad,&dest)!=0&&!memcmp(&dest,&old,sizeof(dest)));checks++;
  }
  p.usse.count=1;assert(remesa_shader_program_validate(&p)==EPROTO);remesa_shader_program_finish(&p);
 }
 printf("PASS both actual-NIR canonical host records -> original writer320byte bodies; %u mutations, rollback/old launch/resource guards; CPU ONLY\n",checks);return 0;
}
