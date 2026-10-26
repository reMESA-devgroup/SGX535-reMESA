/* SPDX-License-Identifier: MIT */
/* Authored public input-color FS original CPU compiler/link/PDS oracles.
 * This tests existing codecs; it does NOT promote IR, input format or execution. */
#include "remesa_usse.h"
#include "remesa_pds_structure.h"
#include "remesa_ta_scene.h"
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static void unhex(const char *s,unsigned char *out,unsigned n)
{CHECK(strlen(s)==2*n);for(unsigned i=0;i<n;i++){unsigned v;CHECK(sscanf(s+2*i,"%2x",&v)==1);out[i]=(unsigned char)v;}}
int main(void)
{
 unsigned input,count=0,known=0;char body[17],pds[121],group[33];
 while(scanf("%u %16s %120s %32s",&input,body,pds,group)==4) {
  CHECK(input==1||input==2);
  unsigned char original_body[8],original_pds[60],bytes[60],mask[60];
  unhex(body,original_body,8);unhex(pds,original_pds,60);size_t n=999;
  /* Original identity FS body: CPU class2[0] -> class1[0], control12=1.
   * No additional finalizer/END or physical interpolation is asserted. */
  const struct remesa_usse_cpu_move move={.source={2,0},.destination={1,0},.control12=1,.control23=1};
  CHECK(!remesa_usse_cpu_move_encode(&move,bytes,60,&n));
  CHECK(n==8&&!memcmp(bytes,original_body,8));
  struct remesa_pds_structure_request r={.entry_count=1,.code_policy=REMESA_PDS_STRUCTURE_CODE_CPU_COMPLETE};
  r.entries[0]=(struct remesa_pds_structure_entry){.kind=REMESA_PDS_STRUCTURE_R,.id=input+9,
   .components=4,.mode=3,.flag_r=0,.format_present=1,.explicit_format=0x0c000000};
  struct remesa_pds_structure s;
  CHECK(!remesa_pds_structure_encode(&r,&s));
  CHECK(s.data_dwords==12&&s.code_slot_count==3&&s.use_background==0x80000);
  CHECK(!remesa_pds_structure_serialize(&s,bytes,mask,60,&n));CHECK(n==60);
  for(unsigned i=0;i<60;i++) {
   CHECK(mask[i]==0||mask[i]==255);
   if(mask[i]){if(bytes[i]!=original_pds[i])fprintf(stderr,"mismatch input%u byte%u generated%02x original%02x\n",input,i,bytes[i],original_pds[i]);CHECK(bytes[i]==original_pds[i]);known++;}
   else CHECK(bytes[i]==0); /* Policy zero, not an oracle byte match. */
  }
  for(unsigned i=48;i<60;i++)CHECK(mask[i]==255);
  CHECK(!mask[0]); /* symbolic USE reference and untouched DATA excluded */
  unsigned char original_group[16],group_bytes[16];unhex(group,original_group,16);
  struct remesa_ta_fragment_binding_request binding={.address_window_base=0x20000000,
   .primary={0x20000000,0x20000,0x160,60,12,1,0},
   .secondary={0x20000000,0x20000,0x220,4,0,0,0},.profile=REMESA_TA_FRAGMENT_INPUT_COLOR_CPU};
  CHECK(!remesa_ta_fragment_binding_encode(&binding,group_bytes));
  CHECK(!memcmp(group_bytes,original_group,16));
  for(unsigned mode=0;mode<4;mode++) {
   struct remesa_ta_fragment_binding_request bad=binding;
   if(mode==0)bad.primary.field10=0;if(mode==1)bad.primary.field14=1;
   if(mode==2)bad.primary.object_bytes=56;if(mode==3)bad.profile=REMESA_TA_FRAGMENT_AUTHORED_CONSTANT;
   memset(group_bytes,0xa5,16);unsigned char saved[16];memcpy(saved,group_bytes,16);
   CHECK(remesa_ta_fragment_binding_encode(&bad,group_bytes)==ENOTSUP);
   CHECK(!memcmp(group_bytes,saved,16));
  }
  struct remesa_ta_constant_draw_state_request forbidden={.fragment=binding};unsigned char draw[56];
  CHECK(remesa_ta_constant_draw_state_encode(&forbidden,draw)==ENOTSUP);
  count++;
 }
 CHECK(count==2&&known>24);
 printf("PASS2 authored identity-color FS body/PDS original compiler+link oracles; numeric input1/2 -> R10/11, same compact MOVE and A2/CODE, known-mask-only DATA comparison, symbolic USE background and original group40 binding; CPU ONLY (%u checks,%u known bytes)\n",checks,known);
 return 0;
}
