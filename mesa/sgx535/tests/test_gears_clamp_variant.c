/* SPDX-License-Identifier: MIT. Existing compiler oracles and real owners;
 * modeled GEM only, no CPU vertex execution or physical approval. */
#define REMESA_GEARS_ATTRIBUTE_OWNER_FIXTURE
#include "test_gears_attribute_owners.c"
#include "remesa_usse_arithmetic.h"

int main(int argc,char **argv)
{
 CHECK(argc==3);unsigned char oracle[24],physical[320];
 FILE *file=fopen(argv[1],"rb");CHECK(file&&fread(oracle,1,24,file)==24&&fgetc(file)==EOF&&!fclose(file));
 file=fopen(argv[2],"rb");CHECK(file&&fread(physical,1,320,file)==320&&fgetc(file)==EOF&&!fclose(file));
 unsigned mutations=0;
 for(unsigned attr=0;attr<2;attr++){
  struct remesa_shader_program original=vertex(attr),saved=original,variant;
  CHECK(!remesa_shader_program_clamp_vertex_color(&original,&variant));
  CHECK(!memcmp(&saved,&original,sizeof saved));
  CHECK(!remesa_shader_program_validate(&variant)&&!remesa_shader_ir_gears_vertex_clamped(&variant.semantic));
  CHECK(remesa_shader_ir_gears_vertex(&variant.semantic)==ENOTSUP);
  unsigned char base[320],bytes[352],before[352];size_t n=777;
  CHECK(!remesa_shader_program_encode(&original,base,sizeof base,&n)&&n==320);
  if(attr)CHECK(!memcmp(base,physical,320));
  memset(bytes,0xa5,sizeof bytes);memcpy(before,bytes,sizeof bytes);n=777;
  CHECK(remesa_shader_program_encode(&variant,bytes,343,&n)==ENOSPC&&n==777&&!memcmp(bytes,before,sizeof bytes));
  CHECK(!remesa_shader_program_encode(&variant,bytes,sizeof bytes,&n)&&n==344);
  CHECK(!memcmp(bytes,base,312)&&!memcmp(bytes+312,oracle,24)&&!memcmp(bytes+336,base+312,8));
  CHECK(!memcmp(bytes+344,before+344,8)); /* Capacity is not extra CODE. */
  struct remesa_shader_program_report report;CHECK(!remesa_shader_program_report(&variant,&report));
  CHECK(report.binary_bytes==344&&!report.binary_error&&report.launch_error==ENOTSUP);
  struct remesa_shader_vertex_layout layout,old;
  CHECK(!remesa_shader_ir_vertex_layout(&original.semantic,&old));
  CHECK(!remesa_shader_ir_vertex_layout(&variant.semantic,&layout)&&!memcmp(&old,&layout,sizeof old));
  struct remesa_shader_program fs=fragment(1);struct remesa_shader_linkage link,unclamped;
  CHECK(!remesa_shader_linkage_prepare(&variant,&fs,&link)&&!remesa_shader_linkage_validate(&link));
  CHECK(!remesa_shader_linkage_prepare(&original,&fs,&unclamped));
  CHECK(remesa_shader_linkage_revalidate(&unclamped,&variant,&fs)==ESTALE);
  struct remesa_shader_selected_resources resource,prior;
  memset(&resource,0xa5,sizeof resource);prior=resource;
  CHECK(remesa_shader_program_selected_resources(&variant,&resource)==ENOTSUP&&!memcmp(&resource,&prior,sizeof prior));
  int error=remesa_shader_program_gears_attribute_resources(&variant,&resource);
  if(attr){
   CHECK(!error&&resource.profile==REMESA_PDS_SELECTED_VS_GEARS_ATTRIBUTE_CLAMPED_PADDED3);
   CHECK(resource.logical_use_bytes==344&&resource.reserved_use_bytes==352&&resource.host_alignment==32);
   CHECK(!remesa_shader_program_gears_attribute_resources(&original,&prior));
   CHECK(resource.semantic_identity!=prior.semantic_identity);
  }else CHECK(error==ENOTSUP&&!memcmp(&resource,&prior,sizeof prior));
  for(unsigned word=0;word<sizeof(variant.semantic)/4;word++){
   struct remesa_shader_ir bad=variant.semantic;uint32_t value;
   memcpy(&value,(unsigned char *)&bad+4*word,4);value^=1;
   memcpy((unsigned char *)&bad+4*word,&value,4);
   struct remesa_shader_program output,untouched;memset(&output,0xa5,sizeof output);untouched=output;
   CHECK(remesa_shader_ir_validate(&bad)!=0);
   CHECK(remesa_shader_program_prepare(&bad,&output)!=0&&!memcmp(&output,&untouched,sizeof output));mutations++;
  }
  struct remesa_shader_program output,untouched;memset(&output,0xa5,sizeof output);untouched=output;
  CHECK(remesa_shader_program_clamp_vertex_color(&variant,&output)==ENOTSUP&&!memcmp(&output,&untouched,sizeof output));
  CHECK(remesa_shader_program_clamp_vertex_color(&fs,&output)==ENOTSUP&&!memcmp(&output,&untouched,sizeof output));
  CHECK(remesa_shader_program_clamp_vertex_color(&original,&original)==EINVAL&&!memcmp(&saved,&original,sizeof saved));
  char dump[4096];CHECK(!remesa_shader_ir_dump(&variant.semantic,dump,sizeof dump,&n)&&strstr(dump,"gears-vertex-clamped"));
 }
 unsigned char bytes[32],before[32];memset(bytes,0xa5,sizeof bytes);memcpy(before,bytes,sizeof bytes);size_t n=777;
 CHECK(remesa_usse_cpu_color_output_clamp_encode(bytes,23,&n)==ENOSPC&&n==777&&!memcmp(bytes,before,sizeof bytes));
 CHECK(remesa_usse_cpu_color_output_clamp_encode(bytes,sizeof bytes,(void *)bytes)==EINVAL&&!memcmp(bytes,before,sizeof bytes));
 CHECK(remesa_usse_cpu_color_output_clamp_encode(NULL,24,&n)==EINVAL);
 CHECK(remesa_usse_cpu_color_output_clamp_encode(bytes,SIZE_MAX,&n)==EINVAL);
 CHECK(!remesa_usse_cpu_color_output_clamp_encode(bytes,sizeof bytes,&n)&&n==24&&!memcmp(bytes,oracle,24)&&!memcmp(bytes+24,before+24,8));

 struct fixture f={.allow_gpu_map=true,.allow_sync=true};struct remesa_device device={0};
 void *program=NULL,*geometry=NULL,*parameters=NULL,*pds=NULL,*state=NULL;
 setup_gears(&f,&device,9900,&program,&geometry,&parameters,4,36);
 device.ops->destroy_bo(device.priv,program);program=NULL;
 struct remesa_shader_linkage link=gears_pair();struct remesa_shader_program variant;
 CHECK(!remesa_shader_program_clamp_vertex_color(&link.vertex,&variant));link.vertex=variant;
 CHECK(!remesa_shader_linkage_validate(&link));const uint32_t banks[]={4,3};
 CHECK(!remesa_persistent_gears_attribute_program_prepare(&device,&link,banks,&program));
 CHECK(!remesa_persistent_gears_attribute_pds_prepare(&device,program,geometry,parameters,&pds));
 struct remesa_persistent_gears_attribute_pds_backing backing;
 CHECK(!remesa_persistent_gears_attribute_pds_snapshot(&device,pds,&backing));
 CHECK(backing.pds.program.program_bytes[0]==344&&backing.pds.program.reserved_bytes[0]==352&&backing.pds.program.program_offset[1]==352);
 CHECK(backing.pds.plans[0].use_committed_bytes==344&&backing.pds.plans[0].use_reserved_bytes==352);
 CHECK(backing.input_dwords==8&&backing.temporary_dwords==11&&backing.output_dwords==8);
 unsigned char *image=get(&f,backing.pds.program.handle)->bytes;
 CHECK(!memcmp(image,physical,312)&&!memcmp(image+312,oracle,24)&&!memcmp(image+336,physical+312,8));
 for(unsigned i=344;i<352;i++)CHECK(!image[i]);
 struct remesa_persistent_linked_state_request request={
  .isp={.depth_bits_present=true,.depth_test=true,.depth_write=true,.compare=REMESA_DEPTH_LESS,.rgba_mask=15},
  .viewport={.framebuffer_width=1280,.framebuffer_height=800,.width=1280,.height=800,
   .depth_bits=24,.far_z=1,.path=REMESA_TA_VIEWPORT_MATRIX_WINDOW}};
 unsigned creates=f.creates;
 CHECK(remesa_persistent_gears_attribute_state_prepare(&device,pds,&request,4,&state)==ENOTSUP&&!state&&f.creates==creates);
 CHECK(!remesa_persistent_gears_homogeneous_state_prepare(&device,pds,&request,4,&state));
 void *parents[]={program,geometry,parameters,pds};for(unsigned i=0;i<4;i++)device.ops->destroy_bo(device.priv,parents[i]);
 struct remesa_persistent_linked_state_backing snapshot;
 CHECK(!remesa_persistent_gears_homogeneous_state_snapshot(&device,state,&snapshot));
 device.ops->destroy_bo(device.priv,state);
 CHECK(f.creates==f.destroys&&f.pins==f.unpins&&!f.forbidden&&!remesa_persistent_close(&device));
 printf("PASS clamp variant CODE344/reserve352/original suffix, %u canonical mutations, distinct linkage/PDS, legacy state refusal and dependency teardown; GPU0\n",mutations);
 return 0;
}
