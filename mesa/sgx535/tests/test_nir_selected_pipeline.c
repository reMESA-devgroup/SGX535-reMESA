/* SPDX-License-Identifier: MIT: authored NIR -> owned selected USE/PDS bytes. */
#include "remesa_shader_program.h"
#include "remesa_nir.h"
#include "remesa/shader_request.h"
#include "nir_builder.h"
#include "util/ralloc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef NDEBUG
#error Assertions required
#endif
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
/* Allocation failures exercise partial immutable-store ownership; no global
 * NIR allocator is replaced and tracking is enabled only around prepare. */
static unsigned fail_calloc,fail_malloc,live;
static bool track;
static void *tracked[64];
void *__real_calloc(size_t,size_t);void *__real_malloc(size_t);void __real_free(void *);
void *__wrap_calloc(size_t,size_t);void *__wrap_malloc(size_t);void __wrap_free(void *);
static void record(void *p){if(track&&p){for(unsigned i=0;i<64;i++)if(!tracked[i]){tracked[i]=p;live++;return;}assert(!"tracking capacity");}}
void *__wrap_calloc(size_t n,size_t bytes){if(fail_calloc&&!--fail_calloc)return NULL;void *p=__real_calloc(n,bytes);record(p);return p;}
void *__wrap_malloc(size_t bytes){if(fail_malloc&&!--fail_malloc)return NULL;void *p=__real_malloc(bytes);record(p);return p;}
void __wrap_free(void *p){if(track&&p)for(unsigned i=0;i<64;i++)if(tracked[i]==p){tracked[i]=NULL;live--;break;}__real_free(p);}
static nir_shader *make(unsigned stage,unsigned constant)
{
 static const nir_shader_compiler_options options={0};
 nir_builder b=nir_builder_init_simple_shader(stage?MESA_SHADER_FRAGMENT:MESA_SHADER_VERTEX,&options,"selected original NIR pair");
 if(!stage) {
  b.shader->info.io_lowered=true;nir_def *zero=nir_imm_int(&b,0),*v[3];
  for(unsigned i=0;i<3;i++)v[i]=nir_load_input(&b,4,32,zero,.base=i,.dest_type=nir_type_float32,.io_semantics={.location=VERT_ATTRIB_GENERIC0+i,.num_slots=1});
  nir_def *f=nir_fsat(&b,nir_fdot3(&b,v[1],v[2])),*color=nir_vec4(&b,f,f,f,f);
  nir_store_output(&b,v[0],zero,.base=0,.write_mask=15,.src_type=nir_type_float32,.io_semantics={.location=VARYING_SLOT_POS,.num_slots=1});
  nir_store_output(&b,color,zero,.base=1,.write_mask=15,.src_type=nir_type_float32,.io_semantics={.location=VARYING_SLOT_COL0,.num_slots=1});
 } else {
  nir_variable *out=nir_variable_create(b.shader,nir_var_shader_out,glsl_vec4_type(),"color");out->data.location=FRAG_RESULT_DATA0;
  nir_def *color;
  if(constant)color=nir_imm_vec4(&b,0,1,0,1);
  else {nir_variable *in=nir_variable_create(b.shader,nir_var_shader_in,glsl_vec4_type(),"COL0");in->data.location=VARYING_SLOT_COL0;color=nir_load_deref(&b,nir_build_deref_var(&b,in));}
  nir_store_deref(&b,nir_build_deref_var(&b,out),color,15);
 }
 nir_validate_shader(b.shader,"selected pair source");return b.shader;
}
static void programs(unsigned constant,struct remesa_shader_program p[2])
{
 for(unsigned s=0;s<2;s++){nir_shader *nir=make(s,constant);struct remesa_shader_ir ir;CHECK(!remesa_shader_ir_lower(nir,&ir));CHECK(!remesa_shader_program_prepare(&ir,&p[s]));ralloc_free(nir);}
}
static void requests(const struct remesa_shader_program p[2],struct remesa_pds_selected_request r[2])
{
 memset(r,0,2*sizeof(*r));
 for(unsigned s=0;s<2;s++) {
  struct remesa_shader_selected_resources res;CHECK(!remesa_shader_program_selected_resources(&p[s],&res));
  r[s].profile=res.profile;r[s].semantic_identity=res.semantic_identity;r[s].use_bank=s?3:4;
  struct remesa_pds_block_request b={.operation=11,.generation=7,
   .allocation={.allocation_cookie=3,.owner_cookie=9,.mapping_generation=2,.address=0x20000000,
    .offset=s?0x1100:0x1000,.bytes=s?64:192,.allocation_bytes=0x20000},
   .data_bytes=48,.code_bytes=s?(res.profile==REMESA_PDS_SELECTED_FS_INPUT_COL0?12:8):16,.code_alignment=16};
  CHECK(!remesa_pds_request_from_block(&b,&r[s].storage));
  r[s].use=(struct remesa_pds_binding){.allocation_cookie=1,.owner_cookie=9,.mapping_generation=2,
   .address=0x20080000,.offset=s?0:0x600,.bytes=res.reserved_use_bytes,.allocation_bytes=0x8000};
 }
 r[0].source_records=3;r[0].vertex_source=(struct remesa_pds_binding){.allocation_cookie=2,.owner_cookie=9,
  .mapping_generation=2,.address=0x20028000,.bytes=144,.allocation_bytes=4096};
}
static void dump(const char *dir,const char *name,const void *data,size_t bytes)
{
 char path[1024];CHECK(snprintf(path,sizeof(path),"%s/%s",dir,name)>0);FILE *f=fopen(path,"wb");CHECK(f&&fwrite(data,1,bytes,f)==bytes&&!fclose(f));
}
int main(int argc,char **argv)
{
 CHECK(argc==1||argc==2);glsl_type_singleton_init_or_ref();
 for(unsigned constant=0;constant<2;constant++) {
  struct remesa_shader_program p[2];programs(constant,p);struct remesa_pds_selected_request r[2];requests(p,r);
  struct remesa_shader_pipeline oldpair;CHECK(remesa_shader_pipeline_prepare(&p[0],&p[1],&oldpair)==ENOTSUP);
  if(constant) {
   struct remesa_shader_selected_pipeline *unknown=NULL;
   CHECK(remesa_shader_selected_pipeline_prepare(p,r,&unknown)==ENOTSUP&&!unknown);
   struct remesa_pds_selected_plan plan;CHECK(!remesa_shader_program_prepare_selected_pds(&p[1],&r[1],&plan));
   unsigned char use[8],data[64],mask[64];size_t ul=0,pl=0;
   memset(data,0xa5,sizeof(data));memset(mask,0xa5,sizeof(mask));
   CHECK(!remesa_shader_program_encode(&p[1],use,sizeof(use),&ul)&&ul==8);
   CHECK(!remesa_pds_selected_encode(&plan,data,mask,sizeof(data),&pl)&&pl==56);
   for(unsigned n=56;n<64;n++)CHECK(data[n]==0xa5&&mask[n]==0xa5);
   CHECK(!plan.relocations[0].background&&plan.use_committed_bytes==8&&plan.use_reserved_bytes==8);
   struct remesa_shader_storage *stores[3]={0};
   const void *bytes[3]={use,data,mask};const size_t lengths[3]={8,56,56};
   for(unsigned k=0;k<3;k++) {
    struct remesa_shader_storage_request sr={REMESA_IR_FRAGMENT,k?REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE:REMESA_SHADER_STORAGE_HOST_CPU_RECORDS,bytes[k],lengths[k],k==2?4:32};
    CHECK(!remesa_shader_storage_create(&sr,&stores[k]));
    struct remesa_shader_storage_view sv;CHECK(!remesa_shader_storage_inspect(stores[k],&sv)&&sv.byte_length==lengths[k]&&!memcmp(sv.bytes,bytes[k],lengths[k]));
    CHECK(remesa_shader_storage_validate_execution(stores[k])==ENOTSUP);
   }
   if(argc==2){dump(argv[1],"constant-use.bin",use,ul);dump(argv[1],"constant-pds.bin",data,pl);dump(argv[1],"constant-mask.bin",mask,pl);}
   for(unsigned k=0;k<3;k++)CHECK(!remesa_shader_storage_reference(&stores[k],NULL));
   continue;
  }
  struct remesa_shader_selected_pipeline *owner=NULL;CHECK(!remesa_shader_selected_pipeline_prepare(p,r,&owner));CHECK(owner);
  struct remesa_shader_selected_pipeline_view v;CHECK(!remesa_shader_selected_pipeline_inspect(owner,&v));
  CHECK(v.resources[0].vertex.input_count==3&&v.resources[0].vertex.stride_dwords==12&&v.resources[0].vertex.output_dwords==8);
  CHECK(v.use[0].byte_length==64&&v.use[1].byte_length==8&&v.pds[0].byte_length==64&&v.pds[1].byte_length==(constant?56:60));
  CHECK(v.plans[0].use_reserved_bytes==144&&v.plans[1].use_reserved_bytes==(constant?8:16));
  CHECK(v.blockers==(REMESA_SELECTED_VERTEX_TRANSPORT|REMESA_SELECTED_DRAW_PACKET)&&v.vertex_transport_error==ENOTSUP&&v.draw_packet_error==ENOTSUP&&v.execution_error==ENOTSUP);
  CHECK(!p[0].usse.schema&&(!constant?!p[1].usse.schema:p[1].usse.schema==REMESA_USSE_SCHEMA));
  CHECK(!remesa_shader_selected_pipeline_revalidate(owner,p,r));
  CHECK(remesa_shader_selected_pipeline_validate_execution(owner)==ENOTSUP);
  if(argc==2&&!constant)for(unsigned s=0;s<2;s++){
   dump(argv[1],s?"fs-use.bin":"vs-use.bin",v.use[s].bytes,v.use[s].byte_length);
   dump(argv[1],s?"fs-pds.bin":"vs-pds.bin",v.pds[s].bytes,v.pds[s].byte_length);
   dump(argv[1],s?"fs-mask.bin":"vs-mask.bin",v.known_mask[s].bytes,v.known_mask[s].byte_length);
  }
  struct remesa_shader_request stage;CHECK(!remesa_shader_selected_pipeline_build_request(owner,9,7,&stage));
  CHECK(stage.resource_mask==15&&!remesa_shader_selected_pipeline_validate_request(owner,&stage));
  CHECK(stage.resources[0].length==64&&stage.resources[1].length==8&&stage.resources[2].length==64&&stage.resources[3].length==60);
  size_t covered=0;
  for(unsigned n=0;n<4;n++){for(size_t k=covered;k<stage.resources[n].offset;k++)CHECK(!stage.data[k]);covered=stage.resources[n].offset+stage.resources[n].length;}
  CHECK(covered==stage.byte_count);
  for(size_t k=covered;k<sizeof(stage.data);k++)CHECK(!stage.data[k]);
  stage.data[stage.resources[2].offset+48]^=1;CHECK(remesa_shader_selected_pipeline_validate_request(owner,&stage)==EPROTO);
  CHECK(!remesa_shader_selected_pipeline_build_request(owner,9,7,&stage));
  CHECK(remesa_shader_selected_pipeline_prepare(p,r,&owner)==EBUSY);
  CHECK(remesa_shader_selected_pipeline_inspect(owner,(void *)v.use[0].bytes)==EINVAL);
  CHECK(remesa_shader_selected_pipeline_clone(owner,(void *)v.pds[1].bytes)==EINVAL);
  CHECK(remesa_shader_selected_pipeline_build_request(owner,9,7,(void *)v.use[0].bytes)==EINVAL);
  CHECK(remesa_shader_selected_pipeline_validate_request(owner,(void *)v.pds[0].bytes)==EINVAL);
  struct remesa_shader_selected_pipeline *copy=NULL;CHECK(!remesa_shader_selected_pipeline_clone(owner,&copy));
  struct remesa_shader_program wrong=p[1];CHECK(!remesa_shader_program_prepare_color(0xffff00ff,&wrong));
  struct remesa_shader_selected_resources unsupported;memset(&unsupported,0xa5,sizeof(unsupported));struct remesa_shader_selected_resources ubefore=unsupported;
  CHECK(remesa_shader_program_selected_resources(&wrong,&unsupported)==ENOTSUP&&!memcmp(&unsupported,&ubefore,sizeof(unsupported)));
  for(unsigned mode=0;mode<13;mode++) {
   struct remesa_pds_selected_request bad[2];memcpy(bad,r,sizeof(bad));
   if(mode==0)bad[0].semantic_identity^=1;
   if(mode==1)bad[1].profile=REMESA_PDS_SELECTED_VS_WHITE_LAMBERT3;
   if(mode==2)bad[0].vertex_source.bytes=143;
   if(mode==3)bad[0].source_records=4;
   if(mode==4)bad[0].use.bytes=64;
   if(mode==5)bad[1].use.offset=0x600;
   if(mode==6)bad[1].storage.generation++;
   if(mode==7)bad[0].vertex_source.owner_cookie++;
   if(mode==8)bad[0].storage.code.offset+=16;
   if(mode==9)bad[0].storage.data.offset+=1;
   if(mode==10)bad[1].vertex_source=bad[0].vertex_source;
   if(mode==11)bad[0].source_records=0;
   if(mode==12)bad[0].reserved=1;
   struct remesa_shader_selected_pipeline *no=NULL;CHECK(remesa_shader_selected_pipeline_prepare(p,bad,&no)!=0&&!no);
   CHECK(remesa_shader_selected_pipeline_revalidate(owner,p,bad)!=0);
  }
  for(unsigned kind=0;kind<2;kind++)for(unsigned index=1;index<=(kind?6u:7u);index++) {
   struct remesa_shader_selected_pipeline *failed=NULL;CHECK(!live);
   fail_calloc=kind?0:index;fail_malloc=kind?index:0;track=true;
   int err=remesa_shader_selected_pipeline_prepare(p,r,&failed);
   track=false;fail_calloc=fail_malloc=0;
   CHECK(err==ENOMEM&&!failed&&!live);CHECK(!remesa_shader_selected_pipeline_validate(owner));
  }
  struct remesa_pds_selected_request moved[2];memcpy(moved,r,sizeof(moved));
  for(unsigned i=0;i<2;i++)moved[i].use.address=0x20020000;
  struct remesa_shader_selected_pipeline *rebased=NULL;CHECK(!remesa_shader_selected_pipeline_prepare(p,moved,&rebased));
  struct remesa_shader_selected_pipeline_view rv;CHECK(!remesa_shader_selected_pipeline_inspect(rebased,&rv));
  for(unsigned st=0;st<2;st++) {
   CHECK(!memcmp(rv.use[st].bytes,v.use[st].bytes,v.use[st].byte_length));
   size_t cell=st?0:16;
   for(size_t n=0;n<v.pds[st].byte_length;n++)if(n<cell||n>=cell+4)
    CHECK(((const unsigned char *)rv.pds[st].bytes)[n]==((const unsigned char *)v.pds[st].bytes)[n]);
   CHECK(rv.plans[st].use_address_bits==(v.plans[st].use_address_bits|0x40));
  }
  CHECK(remesa_shader_selected_pipeline_revalidate(owner,p,moved)==ESTALE);
  remesa_shader_selected_pipeline_destroy(rebased);
  struct remesa_shader_program badp[2];memcpy(badp,p,sizeof(badp));badp[0].usse.schema=1;
  struct remesa_shader_selected_pipeline *no=NULL;CHECK(remesa_shader_selected_pipeline_prepare(badp,r,&no)==EPROTO&&!no);
  CHECK(remesa_shader_selected_pipeline_revalidate(owner,badp,r)==EPROTO);
  memset(p,0,sizeof(p));memset(r,0,sizeof(r));CHECK(!remesa_shader_selected_pipeline_validate(owner));
  remesa_shader_selected_pipeline_destroy(owner);CHECK(!remesa_shader_selected_pipeline_validate(copy));
  CHECK(!remesa_shader_selected_pipeline_build_request(copy,9,7,&stage)&&!remesa_shader_selected_pipeline_validate_request(copy,&stage));
  remesa_shader_selected_pipeline_destroy(copy);
 }
 glsl_type_singleton_decref();printf("PASS %u checks: real NIR selected USE/PDS immutable pair/ownership; transport+packet unsupported\n",checks);return 0;
}
