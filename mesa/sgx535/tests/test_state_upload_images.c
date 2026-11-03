/* SPDX-License-Identifier: MIT. Existing CPU producers and complete PDS image
 * checks only; no GPU execution, source ISP semantics or runtime admission. */
#include "remesa/state_upload.h"
#include "remesa_pds_structure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void carrier(const char *path,unsigned parameter)
{
 unsigned char image[60],encoded[60],mask[60];struct remesa_state_upload decoded;
 FILE *f=fopen(path,"rb");assert(f&&fread(image,1,60,f)==60&&fgetc(f)==EOF&&!fclose(f));
 assert(!(parameter?remesa_parameter_upload_pds_image(image,60,&decoded):
  remesa_state_upload_pds_image(image,60,15,&decoded)));
 const struct remesa_pds_state_image_request r={decoded.source_address&~4095u,4096,
  decoded.source_address&4095u,parameter?40u:15u,decoded.use_lower19,
  parameter?REMESA_PDS_STATE_TASK_PARAMETERS:REMESA_PDS_STATE_TASK_ISP};size_t n;
 assert(!remesa_pds_structure_encode_state_image(&r,encoded,mask,sizeof encoded,&n)&&n==60);
 assert(!memcmp(encoded,image,60));
}
int main(int argc,char **argv)
{
 assert(argc==1||argc==3);unsigned cases=0,mutations=0;
 const unsigned fixups[]={0,4,0x804,0x7ffff};
 for(unsigned parameter=0;parameter<2;parameter++)for(unsigned words=1;words<=(parameter?1u:16u);words++)
  for(unsigned f=0;f<4;f++){
   unsigned char image[64],mask[64];size_t n;struct remesa_state_upload out,initial={7,8,9};
   const struct remesa_pds_state_image_request r={0x20004000,4096,256,parameter?40u:words,
    fixups[f],parameter?REMESA_PDS_STATE_TASK_PARAMETERS:REMESA_PDS_STATE_TASK_ISP};
   memset(image,0xa5,sizeof image);
   assert(!remesa_pds_structure_encode_state_image(&r,image,mask,sizeof image,&n)&&n==60);
#define CHECK_IMAGE(p,cap,dst) (parameter?remesa_parameter_upload_pds_image(p,cap,dst):remesa_state_upload_pds_image(p,cap,words,dst))
   assert(!CHECK_IMAGE(image,64,&out));
   assert(out.source_address==0x20004100&&out.source_bytes==r.state_dwords*4&&out.use_lower19==fixups[f]);
   /* Capacity padding is not CODE, committed DATA or GPU input. */
   for(unsigned i=60;i<64;i++)assert(image[i]==0xa5);
   for(unsigned i=parameter?16:12;i<48;i++)for(unsigned bit=0;bit<8;bit++){
    image[i]^=1u<<bit;out=initial;
    assert(CHECK_IMAGE(image,64,&out)==EINVAL&&!memcmp(&out,&initial,sizeof out));
    /* Retain the sparse decoder's exact original scope, including PARAM's
     * independently known zero at byte40. */
    if(!parameter)assert(!remesa_state_upload_pds(image,64,words,&out));
    else if(i<40||i>=44)assert(!remesa_parameter_upload_pds(image,64,&out));
    image[i]^=1u<<bit;mutations++;
   }
   for(unsigned i=48;i<60;i++)for(unsigned bit=0;bit<8;bit++){
    image[i]^=1u<<bit;out=initial;
    assert(CHECK_IMAGE(image,60,&out)==EINVAL&&!memcmp(&out,&initial,sizeof out));
    image[i]^=1u<<bit;mutations++;
   }
   out=initial;assert(CHECK_IMAGE(image,59,&out)==EINVAL&&!memcmp(&out,&initial,sizeof out));
   assert(CHECK_IMAGE(image,64,(void *)image)==EINVAL);
   assert(CHECK_IMAGE(image,64,NULL)==EINVAL);
   assert(CHECK_IMAGE(NULL,64,&out)==EINVAL);
   assert(CHECK_IMAGE((void *)(UINTPTR_MAX-1),64,&out)==EINVAL);
   for(unsigned i=60;i<64;i++)image[i]^=0xff;
   assert(!CHECK_IMAGE(image,60,&out)&&!CHECK_IMAGE(image,64,&out));cases++;
#undef CHECK_IMAGE
  }
 if(argc==3){carrier(argv[1],0);carrier(argv[2],1);}
 printf("PASS %u constructor cases, %u reserved-DATA/CODE bit mutations, atomic rejection/alias/span/capacity; GPU0\n",cases,mutations);
 return 0;
}
