/* SPDX-License-Identifier: MIT. Numeric CPU contracts, no GPU/ISA admission. */
#include "remesa/fragment_fetch.h"
#include "remesa_pds_structure.h"
#include "remesa_ta_scene.h"
#include <assert.h>
#include <stdio.h>
static void put(unsigned char *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
int main(void)
{
 const uint32_t windows[]={0,0x20000000,0xf0000000},fixups[]={0,3,0x804,0x7ffff};
 unsigned char group[16],image[64],mask[64],secondary[32],saved[64];size_t n;
 struct remesa_fragment_binding refs,refs_initial={1,2,3};struct remesa_fragment_task task,task_initial={4,5};
 for(unsigned color=0;color<2;color++)for(unsigned bank=0;bank<3;bank++)for(unsigned f=0;f<4;f++){
  const uint64_t base=(uint64_t)windows[bank]+0x1000;
  struct remesa_ta_fragment_binding_request binding={.address_window_base=windows[bank],
   .profile=color?REMESA_TA_FRAGMENT_INPUT_COLOR_CPU:REMESA_TA_FRAGMENT_SCENE_ZCLEAR,
   .primary={base,4096,0x160,color?60:56,12,color,0},.secondary={base,4096,0x220,4,0,0,0}};
  assert(!remesa_ta_fragment_binding_encode(&binding,group));
  assert(!remesa_fragment_binding_read(group+4,12,windows[bank],color,&refs)&&refs.primary==base+0x160&&refs.secondary==base+0x220&&refs.primary_bytes==(color?60u:56u));
  struct remesa_pds_structure_request r={.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
  if(color){r.entry_count=1;r.entries[0]=(struct remesa_pds_structure_entry){.kind=REMESA_PDS_STRUCTURE_R,
   .id=10,.components=4,.mode=3,.format_present=1,.explicit_format=0x0c000000};}
  struct remesa_pds_structure shape;assert(!remesa_pds_structure_encode(&r,&shape));
  memset(image,0xa5,64);assert(!remesa_pds_structure_serialize(&shape,image,mask,64,&n)&&n==(color?60u:56u));
  put(image,shape.use_background|fixups[f]);memset(mask,255,4);
  for(unsigned i=0;i<n;i++)if(!mask[i])image[i]=(unsigned char)(i*17);
  assert(!remesa_fragment_primary(image,n,color,&task)&&task.use_lower19==fixups[f]&&task.committed_bytes==n);
  assert(!remesa_fragment_primary(image,64,color,&task));memcpy(saved,image,64);
  const unsigned bad[]={3,4,32,48,52};
  for(unsigned i=0;i<5;i++){
   memcpy(image,saved,64);image[bad[i]]^=0x80;task=task_initial;
   assert(remesa_fragment_primary(image,n,color,&task)==EINVAL&&!memcmp(&task,&task_initial,sizeof task));
  }
  if(color){memcpy(image,saved,64);image[36]^=1;assert(remesa_fragment_primary(image,60,1,&task)==EINVAL);}
  memset(secondary,0xa5,32);const uint32_t none=REMESA_PDS_STRUCTURE_NO_CELL;
  const struct remesa_pds_code_instruction empty={REMESA_PDS_STRUCTURE_CPU_EMPTY_SECONDARY_AF,none,none};
  assert(!remesa_pds_structure_encode_code(&empty,1,secondary,32,&n)&&n==4);
  assert(!remesa_fragment_secondary(secondary,4)&&!remesa_fragment_secondary(secondary,32)&&secondary[4]==0xa5);
  secondary[3]^=1;assert(remesa_fragment_secondary(secondary,4)==EINVAL);
  refs=refs_initial;group[4]^=1;assert(remesa_fragment_binding_read(group+4,12,windows[bank],color,&refs)==ERANGE&&!memcmp(&refs,&refs_initial,sizeof refs));group[4]^=1;
  group[8]^=2;assert(remesa_fragment_binding_read(group+4,12,windows[bank],color,&refs)==EINVAL&&!memcmp(&refs,&refs_initial,sizeof refs));group[8]^=2;
 }
 unsigned char oracle[12];assert(fread(oracle,1,12,stdin)==12&&fgetc(stdin)==EOF);
 assert(!remesa_fragment_binding_read(oracle,12,0x20000000,0,&refs)&&refs.primary==0x20000160&&refs.secondary==0x20004200&&refs.primary_bytes==56);
 refs=refs_initial;assert(remesa_fragment_binding_read(group+4,11,0x20000000,0,&refs)==EINVAL&&!memcmp(&refs,&refs_initial,sizeof refs));
 assert(remesa_fragment_binding_read(group+4,12,0x20001000,0,&refs)==EINVAL);
 assert(remesa_fragment_binding_read(group+4,12,0x20000000,0,(void *)(group+4))==EINVAL);
 assert(remesa_fragment_secondary(secondary,3)==EINVAL);
 puts("PASS fragment contracts: original group40/no-R/COLOR0/empty-secondary encoders,3 windows/four fixups,known-mask CODE/reference/task checks,legacy carrier triple,capacity and atomic errors; GPU0");return 0;
}
