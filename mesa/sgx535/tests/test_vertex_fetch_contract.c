/* SPDX-License-Identifier: MIT. Original encoders and preserved PDS inputs. */
#include "remesa/vertex_fetch.h"
#include "remesa_pds_structure.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 unsigned char image[192],mask[192],saved[192];size_t n;
 struct remesa_vertex_fetch out,initial={1,2,3,4};const unsigned fixups[]={0,4,0x804,0x7ffff};
 for(unsigned compiled=0;compiled<2;compiled++)for(unsigned stride=1;stride<=16;stride++)for(unsigned f=0;f<4;f++){
  struct remesa_pds_vertex_image_request r={0x20004000,4096,256,stride,fixups[f]};
  memset(image,0xa5,sizeof image);
  int e=compiled?remesa_pds_structure_encode_compiled_vertex_image(&r,image,mask,sizeof image,&n):
   remesa_pds_structure_encode_vertex_image(&r,image,mask,sizeof image,&n);
  assert(!e&&n==64&&image[64]==0xa5);
  assert(!remesa_vertex_fetch_pds_image(image,sizeof image,stride,compiled,&out));
  for(unsigned i=0;i<64;i++)if(!mask[i])for(unsigned bit=0;bit<8;bit++){
   image[i]^=1u<<bit;out=initial;
   assert(remesa_vertex_fetch_pds_image(image,sizeof image,stride,compiled,&out)==EINVAL&&!memcmp(&out,&initial,sizeof out));
   assert(!remesa_vertex_fetch_pds(image,64,stride,compiled,&out));image[i]^=1u<<bit;
  }
  for(unsigned i=0;i<64;i++)if(!mask[i])image[i]=(unsigned char)(i*17);
  assert(!remesa_vertex_fetch_pds(image,64,stride,compiled,&out)&&out.source_address==0x20004100&&
   out.stride_bytes==stride*4&&out.use_lower19==fixups[f]&&out.compiled==compiled);
  assert(!remesa_vertex_fetch_pds(image,192,stride,compiled,&out));memcpy(saved,image,sizeof image);
  const unsigned offsets[]={0,4,19,32,48,52,56,60};
  for(unsigned i=0;i<8;i++){
   memcpy(image,saved,sizeof image);image[offsets[i]]^=i==0?1:0x80;out=initial;
   assert(remesa_vertex_fetch_pds(image,64,stride,compiled,&out)&&!memcmp(&out,&initial,sizeof out));
  }
  if(compiled)for(unsigned i=0;i<2;i++){
   memcpy(image,saved,sizeof image);image[i?36:20]^=1;out=initial;
   assert(remesa_vertex_fetch_pds(image,64,stride,1,&out)==EINVAL&&!memcmp(&out,&initial,sizeof out));
  }
 }
 unsigned char oracle[128];assert(fread(oracle,1,sizeof oracle,stdin)==sizeof oracle&&fgetc(stdin)==EOF);
 assert(!remesa_vertex_fetch_pds(oracle,64,8,0,&out)&&out.source_address==0x40000000&&out.use_lower19==0x404);
 assert(!remesa_vertex_fetch_pds(oracle+64,64,4,0,&out)&&out.source_address==0x200042c4&&out.use_lower19==0x604);
 _Alignas(8) unsigned char indices[8]={0,0,2,0,1,0,0xa5,0xa5};
 struct remesa_vertex_indices range,range_initial={7,8};
 assert(!remesa_vertex_indices_check(indices,6,3,3,&range)&&range.minimum==0&&range.maximum==2);
 indices[0]=3;range=range_initial;
 assert(remesa_vertex_indices_check(indices,8,3,3,&range)==ERANGE&&!memcmp(&range,&range_initial,sizeof range));indices[0]=0;
 indices[2]=indices[3]=255;
 assert(!remesa_vertex_indices_check(indices,6,3,65536,&range)&&range.maximum==65535);
 range=range_initial;assert(remesa_vertex_indices_check(indices,6,3,65535,&range)==ERANGE&&!memcmp(&range,&range_initial,sizeof range));
 static unsigned char maximum[65535*2];
 assert(!remesa_vertex_indices_check(maximum,sizeof maximum,65535,1,&range)&&range.maximum==0);
 maximum[sizeof maximum-2]=1;range=range_initial;
 assert(remesa_vertex_indices_check(maximum,sizeof maximum,65535,1,&range)==ERANGE&&!memcmp(&range,&range_initial,sizeof range));
 assert(remesa_vertex_indices_check(indices,5,3,3,&range)==EINVAL);
 assert(remesa_vertex_indices_check(indices,6,2,3,&range)==EINVAL);
 assert(remesa_vertex_indices_check(indices,6,3,0,&range)==EINVAL);
 assert(remesa_vertex_indices_check(indices,6,3,3,(void *)indices)==EINVAL);
 out=initial;assert(remesa_vertex_fetch_pds(image,63,4,0,&out)==EINVAL&&!memcmp(&out,&initial,sizeof out));
 assert(remesa_vertex_fetch_pds(image,64,4,0,(void *)image)==EINVAL);
 puts("PASS vertex fetch: original copy/compiled encoders1..16/four fixups,2 independent carriers, known-mask/tamper/capacity64vs192, actual u16 indices/committed bounds/max65535 and atomic rejection; GPU0");return 0;
}
