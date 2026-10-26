/* SPDX-License-Identifier: MIT
 * Original NIR -> owned VS/FS -> DATA relocation integration, CPU only.
 * No historical shader/program bytes, device calls or rasterization fixtures.
 */
#include "remesa_shader_program.h"
#include "remesa_screen.h"
#include "remesa_fragment_body.h"
#include "nir_fixture.h"
#include "vertex_nir_fixture.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,allocation,fail_at,live;
static bool tracking;
static const char *export_directory;
static void *owned[64];
#define CHECK(x) do {assert(x);checks++;} while(0)
void *__real_calloc(size_t,size_t);
void *__real_malloc(size_t);
void __real_free(void *);
void *__wrap_calloc(size_t,size_t);
void *__wrap_malloc(size_t);
void __wrap_free(void *);
static void *track(void *p)
{
 if(p)for(unsigned i=0;i<64;i++)if(!owned[i]){owned[i]=p;live++;return p;}
 assert(!p);return p;
}
void *__wrap_calloc(size_t n,size_t bytes)
{if(!tracking)return __real_calloc(n,bytes);if(++allocation==fail_at)return NULL;return track(__real_calloc(n,bytes));}
void *__wrap_malloc(size_t bytes)
{if(!tracking)return __real_malloc(bytes);if(++allocation==fail_at)return NULL;return track(__real_malloc(bytes));}
void __wrap_free(void *p)
{
 if(!p)return;
 for(unsigned i=0;i<64;i++)if(owned[i]==p){owned[i]=NULL;live--;__real_free(p);return;}
 __real_free(p);
}
static uint32_t get32(const void *data)
{const unsigned char *p=data;return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void export_bytes(const char *name,const void *bytes,size_t length)
{
 char path[1024];int n=snprintf(path,sizeof(path),"%s/%s",export_directory,name);
 CHECK(n>0&&(size_t)n<sizeof(path));
 FILE *file=fopen(path,"wx");CHECK(file);
 CHECK(fwrite(bytes,1,length,file)==length);CHECK(!fclose(file));
}
static struct remesa_shader_draw_programs programs(uint32_t color)
{
 struct remesa_shader_program stages[2];struct remesa_shader_ir ir;
 nir_shader *vs=vertex_fixture(),*fs=fixture(color);
 CHECK(!remesa_shader_ir_lower(vs,&ir)&&!remesa_shader_program_prepare(&ir,&stages[0]));
 CHECK(!remesa_shader_ir_lower(fs,&ir)&&!remesa_shader_program_prepare(&ir,&stages[1]));
 ralloc_free(vs);ralloc_free(fs);
 struct remesa_shader_draw_programs result;
 CHECK(!remesa_shader_draw_programs_prepare(&stages[0],&stages[1],&result));return result;
}
static struct remesa_shader_host_pipeline_request request(struct remesa_pds_data_cell cells[2][12])
{
 struct remesa_shader_host_pipeline_request r={.banks={1,2},.data_byte_offsets={4,8}};
 for(unsigned c=0;c<4;c++) {
  r.vertex_mapping.input[c]=(struct remesa_usse_cpu_register){0,8+c};
  r.vertex_mapping.output[c]=(struct remesa_usse_cpu_register){2,24+c};
 }
 for(unsigned i=0;i<2;i++) {
  r.use[i]=(struct remesa_pds_binding){.allocation_cookie=1,.owner_cookie=7,
   .mapping_generation=3,.address=UINT64_C(0x20000000),.offset=i?32:0,
   .bytes=i?8:32,.allocation_bytes=4096};
  r.pds[i]=(struct remesa_pds_block_request){.operation=9,.generation=4,
   .allocation={.allocation_cookie=2+i,.owner_cookie=7,.mapping_generation=5,
    .address=UINT64_C(0x21000000)+i*UINT64_C(0x01000000),.bytes=256,.allocation_bytes=4096},
   .data_bytes=48,.code_bytes=4,.code_alignment=16};
  for(unsigned c=0;c<12;c++)cells[i][c]=(struct remesa_pds_data_cell){
   .kind=REMESA_PDS_DATA_LITERAL32,.value=UINT32_C(0x12340000)+i*256+c};
  cells[i][r.data_byte_offsets[i]/4]=(struct remesa_pds_data_cell){
   .kind=REMESA_PDS_DATA_USE_ENTRY_LOWER19,.value=UINT32_C(0xa0000000)};
  r.data[i]=(struct remesa_pds_host_data_recipe){.cells=cells[i],.cell_count=12};
 }
 return r;
}
static void positive(uint32_t color)
{
 struct remesa_shader_draw_programs pair=programs(color),saved=pair;
 struct remesa_pds_data_cell cells[2][12];struct remesa_shader_host_pipeline_request r=request(cells),saved_r=r;
 saved_r=r;
 struct remesa_shader_host_pipeline *p=NULL;
 CHECK(!remesa_shader_host_pipeline_create(&pair,&r,&p)&&p);
 CHECK(!memcmp(&pair,&saved,sizeof(pair))&&!memcmp(&r,&saved_r,sizeof(r)));
 CHECK(!remesa_shader_host_pipeline_validate(p));
 CHECK(!remesa_shader_host_pipeline_revalidate(p,&pair,&r));
 struct remesa_shader_host_pipeline_view view;
 CHECK(!remesa_shader_host_pipeline_inspect(p,&view));
 CHECK(view.execution_error==ENOTSUP&&view.pds_code_error==ENOTSUP&&view.image.execution_error==ENOTSUP);
 CHECK(view.image.vertex.byte_length==32&&view.image.fragment.byte_length==r.use[1].bytes);
 CHECK(view.links->launch.semantic.binary_error==ENOTSUP);
 for(unsigned i=0;i<2;i++) {
  const struct remesa_shader_storage_view *data=&view.data[i].storage;
  CHECK(data->format==REMESA_SHADER_STORAGE_HOST_PDS_DATA&&data->byte_length==48);
  CHECK(view.links->launch.stage[i].request.code.offset==48);
  for(unsigned c=0;c<12;c++) {
   uint32_t expected=cells[i][c].value;
   if(c==r.data_byte_offsets[i]/4)expected|=view.links->use[i].address_bits;
   CHECK(get32((const unsigned char *)data->bytes+4*c)==expected);
  }
 }
 if(export_directory&&color==UINT32_C(0xff00ff00)) {
  export_bytes("vertex.cpu-candidate.bin",view.image.vertex.bytes,view.image.vertex.byte_length);
  export_bytes("fragment.cpu-candidate.bin",view.image.fragment.bytes,view.image.fragment.byte_length);
  export_bytes("vertex.pds-host-data.bin",view.data[0].storage.bytes,view.data[0].storage.byte_length);
  export_bytes("fragment.pds-host-data.bin",view.data[1].storage.bytes,view.data[1].storage.byte_length);
  char metadata[768];
  int n=snprintf(metadata,sizeof(metadata),
   "{\n  \"scope\": \"Original CPU fixtures; not executable GPU images\",\n"
   "  \"pds_code_generated\": false,\n  \"execution_qualified\": false,\n"
   "  \"numeric_VS_mapping_is_hardware_default\": false,\n"
   "  \"stage_bytes\": [32, 8],\n  \"DATA_bytes\": [48, 48],\n"
   "  \"USE_address_bits\": [%u, %u],\n  \"DATA_fixup_offsets\": [4, 8]\n}\n",
   view.links->use[0].address_bits,view.links->use[1].address_bits);
  CHECK(n>0&&(size_t)n<sizeof(metadata));export_bytes("scope.json",metadata,(size_t)n);
 }
 struct remesa_shader_host_pipeline *old=p;
 CHECK(remesa_shader_host_pipeline_create(&pair,&r,&p)==EBUSY&&p==old);
 struct remesa_shader_host_pipeline_request changed=r;
 changed.banks[1]=3;CHECK(remesa_shader_host_pipeline_revalidate(p,&pair,&changed)==ESTALE);
 changed=r;changed.vertex_mapping.output[0].number=64;
 CHECK(remesa_shader_host_pipeline_revalidate(p,&pair,&changed)==ESTALE);
 cells[0][0].value^=1;CHECK(remesa_shader_host_pipeline_revalidate(p,&pair,&r)==ESTALE);
 cells[0][0].value^=1;
 CHECK(!remesa_shader_host_pipeline_revalidate(p,&pair,&r));
 struct remesa_shader_host_pipeline_view sentinel,before;memset(&sentinel,0xa5,sizeof(sentinel));before=sentinel;
 CHECK(remesa_shader_host_pipeline_inspect(NULL,&sentinel)==EINVAL&&!memcmp(&sentinel,&before,sizeof(sentinel)));
 CHECK(remesa_shader_host_pipeline_inspect(p,(void *)view.links)==EINVAL);
 CHECK(remesa_shader_host_pipeline_inspect(p,(void *)view.data[0].storage.bytes)==EINVAL);
 CHECK(remesa_shader_host_pipeline_inspect(p,(void *)((const unsigned char *)view.image.programs+
  sizeof(*view.image.programs)))==EINVAL);
 /* Lifetime is independent of all source descriptors, NIR and recipe arrays. */
 memset(&pair,0,sizeof(pair));memset(&r,0,sizeof(r));memset(cells,0,sizeof(cells));
 CHECK(!remesa_shader_host_pipeline_validate(p));
 CHECK(!remesa_shader_host_pipeline_inspect(p,&view)&&view.links->launch.semantic.fragment_argb==color);
 remesa_shader_host_pipeline_destroy(p);
}
static void failures(void)
{
 struct remesa_shader_draw_programs pair=programs(UINT32_C(0xff00ff00));
 struct remesa_pds_data_cell cells[2][12];struct remesa_shader_host_pipeline_request r=request(cells);
 for(unsigned mode=0;mode<10;mode++) {
  struct remesa_shader_host_pipeline_request bad=r;struct remesa_shader_host_pipeline *p=NULL;
  if(mode==0)bad.use[1].bytes++;
  if(mode==1)bad.pds[1].generation++;
  if(mode==2)bad.pds[1].allocation.address=bad.pds[0].allocation.address;
  if(mode==3)bad.pds[1].code_alignment=3;
  if(mode==4)bad.data[1].cell_count=0;
  if(mode==5)bad.data[1].cell_count=REMESA_SHADER_STORAGE_MAX_BYTES/4+1;
  if(mode==6)bad.data_byte_offsets[1]=48;
  if(mode==7)bad.banks[1]=bad.banks[0];
  if(mode==8)bad.data[1].cells=(void *)(UINTPTR_MAX-3);
  if(mode==9)bad.vertex_mapping.output[1]=bad.vertex_mapping.input[0];
  CHECK(remesa_shader_host_pipeline_create(&pair,&bad,&p)!=0&&!p);
 }
 CHECK(remesa_shader_host_pipeline_create(&pair,&r,(void *)&pair)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create(&pair,&r,(void *)&r)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create(&pair,&r,(void *)cells)==EINVAL);
 CHECK(remesa_shader_host_pipeline_create(&pair,&r,(void *)(UINTPTR_MAX-1))==EINVAL);
 /* Enumerate every allocator failure through image/storage/recipe ownership. */
 for(unsigned n=1;n<32;n++) {
  allocation=0;fail_at=n;tracking=true;struct remesa_shader_host_pipeline *p=NULL;
  int error=remesa_shader_host_pipeline_create(&pair,&r,&p);
  if(error)CHECK(error==ENOMEM&&!p);else CHECK(p&&allocation<n);
  remesa_shader_host_pipeline_destroy(p);CHECK(!live);tracking=false;
  if(!error){CHECK(n>10);break;}
  CHECK(n!=31);
 }
 remesa_shader_host_pipeline_destroy(NULL);
}
int main(int argc,char **argv)
{
 if(argc==3&&!strcmp(argv[1],"--export"))export_directory=argv[2];
 else CHECK(argc==1);
 positive(UINT32_C(0xff00ff00));positive(UINT32_C(0xffff00ff));positive(UINT32_C(0xffffff00));failures();
 printf("PASS %u original NIR/VS/FS/PDS DATA pipeline checks; CODE and execution unsupported; no GPU\n",checks);
 return 0;
}
