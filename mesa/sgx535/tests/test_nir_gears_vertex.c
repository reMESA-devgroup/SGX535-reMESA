/* SPDX-License-Identifier: MIT. Authored complete ff-vs; CPU recognition only. */
#include "remesa_nir.h"
#include "remesa_usse_arithmetic.h"
#include "remesa_shader_ir.h"
#include "remesa_shader_program.h"
#include "nir_builder.h"
#include "mesa/program/prog_statevars.h"
#include "util/ralloc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
#include "authored_gears_vertex_nir_fixture.h"
int main(void){
 unsigned char oracles[648];FILE*carrier=fopen("body-oracles.bin","rb");assert(carrier&&fread(oracles,1,sizeof(oracles),carrier)==sizeof(oracles)&&fgetc(carrier)==EOF&&!fclose(carrier));
 unsigned char tail[8];carrier=fopen("tail-oracle.bin","rb");assert(carrier&&fread(tail,1,8,carrier)==8&&fgetc(carrier)==EOF&&!fclose(carrier));
 glsl_type_singleton_init_or_ref();unsigned checks=0,mutations=0;
 for(unsigned attr=0;attr<2;attr++)for(unsigned mode=0;mode<29;mode++){
  nir_shader*s=make(attr,mode);struct remesa_nir_gears_vertex_profile result,before;memset(&result,0xa5,sizeof(result));before=result;
  int error=remesa_nir_gears_vertex(s,&result);
  if(mode){assert(error==ENOTSUP&&!memcmp(&result,&before,sizeof(result)));}
  else {
   assert(!error&&result.normal_from_attribute==attr&&result.uniform_vectors==(attr?9u:10u)&&result.source_dwords==(attr?37u:41u)&&result.input_count==(attr?2u:1u)&&result.output_dwords==8&&result.temporary_dwords==11&&result.point_size_bits==0x3f800000);
   unsigned char encoded[312];size_t written=0;
   const struct remesa_usse_cpu_gears_vertex_body request={result.normal_from_attribute,result.uniform_vectors,result.source_dwords,REMESA_USSE_CPU_GEARS_TRIANGLES,0};
   assert(!remesa_usse_cpu_gears_vertex_body_encode(&request,encoded,sizeof(encoded),&written)&&written==312&&!memcmp(encoded,oracles+324*attr+12,312));
   assert(oracles[324*attr]==attr&&oracles[324*attr+4]==result.uniform_vectors&&oracles[324*attr+8]==0x38&&oracles[324*attr+9]==1);
   struct remesa_shader_ir ir;assert(!remesa_shader_ir_lower(s,&ir)&&!remesa_shader_ir_gears_vertex(&ir)&&!remesa_shader_ir_validate(&ir));
   struct remesa_shader_vertex_layout layout;assert(!remesa_shader_ir_vertex_layout(&ir,&layout)&&layout.input_count==(attr?2u:1u)&&layout.stride_dwords==(attr?8u:4u)&&layout.color_components==4&&layout.output_dwords==8);
   struct remesa_shader_program program;assert(!remesa_shader_program_prepare(&ir,&program)&&program.status==REMESA_PROGRAM_COMPILE_OFFLINE&&!remesa_shader_program_validate(&program));
   struct remesa_shader_program_report report;assert(!remesa_shader_program_report(&program,&report)&&!report.binary_error&&report.binary_bytes==320&&report.launch_error==ENOTSUP);
   unsigned char full[336],saved[336];memset(full,0xa5,sizeof(full));memcpy(saved,full,sizeof(full));written=777;
   assert(remesa_shader_program_encode(&program,full,319,&written)==ENOSPC&&written==777&&!memcmp(full,saved,sizeof(full)));
   assert(!remesa_shader_program_encode(&program,full,sizeof(full),&written)&&written==320&&!memcmp(full,oracles+324*attr+12,312)&&!memcmp(full+312,tail,8)&&!memcmp(full+320,saved+320,16));
   struct remesa_shader_selected_resources resources,old_resources;memset(&resources,0xa5,sizeof(resources));old_resources=resources;
   assert(remesa_shader_program_selected_resources(&program,&resources)==ENOTSUP&&!memcmp(&resources,&old_resources,sizeof(resources)));
   struct remesa_pds_request launch={0};struct remesa_pds_plan plan,old_plan;memset(&plan,0xa5,sizeof(plan));old_plan=plan;
   assert(remesa_shader_program_prepare_launch(&program,&launch,&plan)==ENOTSUP&&!memcmp(&plan,&old_plan,sizeof(plan)));
   for(unsigned word=0;word<sizeof(ir)/4;word++) {
    struct remesa_shader_ir bad=ir;uint32_t value;memcpy(&value,(unsigned char*)&bad+4*word,4);value^=1;memcpy((unsigned char*)&bad+4*word,&value,4);
    assert(remesa_shader_ir_validate(&bad)!=0);mutations++;
   }
   char dump[4096];size_t needed;assert(!remesa_shader_ir_dump(&ir,dump,sizeof(dump),&needed)&&strstr(dump,"gears-vertex"));
   FILE*semantic=fopen(attr?"semantic-attr.bin":"semantic-state.bin","wb");assert(semantic&&fwrite(&ir,1,sizeof(ir),semantic)==sizeof(ir)&&!fclose(semantic));
   remesa_shader_program_finish(&program);
   FILE*f=fopen(attr?"authored-attr.nir.txt":"authored-state.nir.txt","w");assert(f);nir_print_shader(s,f);assert(!fclose(f));
  }
  ralloc_free(s);checks++;
 }
 struct remesa_nir_gears_vertex_profile out;assert(remesa_nir_gears_vertex(NULL,&out)==EINVAL);nir_shader*s=make(true,0);assert(remesa_nir_gears_vertex(s,NULL)==EINVAL);ralloc_free(s);
 glsl_type_singleton_decref();printf("IR mutation checks: %u\n",mutations);printf("PASS full ff-vs POSITION/COLOR0/PSIZ recognition, uniform state slots/swizzles/normal variants, %u cases, both complete NIR->typed312byte bodies match independent original CPU writers; full canonical IR/program320 offline; launch/resource guards unchanged; CPU ONLY\n",checks);return 0;
}
