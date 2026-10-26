/* SPDX-License-Identifier: MIT */
/* Expected contributions are independently checked against the original x86
 * producer. These are CPU routing rules, not GPU feature qualification. */
#include "remesa_render_target.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static const struct {
 struct remesa_isp_color_control_request r;
 uint32_t expected;
} cases[]={
 {{15,0,0,0,0,0,0},0},
 {{0,0,0,0,0,0,0},0x02200000},
 {{1,0,0,0,0,0,0},0x02000000},
 {{15,1,0,0,0,0,0},0x02000000},
 {{15,0,1,0,0,0,0},0x02000000},
 {{15,0,0,1,0,0,0},0x08000000},
 {{0,0,0,1,0,0,0},0x04000000},
 {{15,0,0,0,0,1,0},0x08000000},
 {{0,0,0,0,0,1,0},0x04000000},
 {{15,0,0,0,1,0,0},0x0a000000},
 {{0,0,0,0,1,0,0},0x0a000000},
 {{15,1,0,1,0,0,0},0x04000000},
 {{15,0,1,0,0,1,0},0x04000000},
 {{5,0,0,0,1,1,0},0x0a000000},
 {{0,0,0,0,0,0,1},0x02000000},
 {{0,0,0,1,0,0,1},0x04000000},
 {{15,0,0,0,0,0,1},0},
 {{1,0,1,0,0,0,0},0x02000000},
 {{0,1,1,1,1,1,1},0x0a000000},
 {{8,0,0,1,0,0,0},0x04000000}
};
int main(int argc,char **argv)
{
 bool dump=argc==2&&!strcmp(argv[1],"--dump");
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  uint32_t value=0xdeadbeef;
  struct remesa_isp_color_control_request input,saved;
  memcpy(&input,&cases[i].r,sizeof(input));memcpy(&saved,&input,sizeof(saved));
  assert(!remesa_isp_color_control_encode(&input,&value));
  assert(value==cases[i].expected&&!(value&~UINT32_C(0x0e200000)));
  assert(!memcmp(&input,&saved,sizeof(input)));
  if(dump)printf("%u %08x\n",i,value);
 }
 /* Existing restricted API must retain all sixteen established cases. */
 for(unsigned mask=0;mask<16;mask++) {
  struct remesa_isp_color_control_request r={.rgba_mask=mask};uint32_t a,b;
  assert(!remesa_isp_color_control_encode(&r,&a));
  assert(!remesa_isp_opaque_color_mask_encode(mask,&b)&&a==b);
 }
 uint32_t value=UINT32_C(0xdeadbeef);
 struct remesa_isp_color_control_request bad={.rgba_mask=16};
 assert(remesa_isp_color_control_encode(&bad,&value)==EINVAL&&value==UINT32_C(0xdeadbeef));
 assert(remesa_isp_color_control_encode(NULL,&value)==EINVAL&&value==UINT32_C(0xdeadbeef));
 assert(remesa_isp_color_control_encode(&cases[0].r,NULL)==EINVAL);
 if(!dump)puts("PASS: 20 independently checked ISP routing cases, branch priority, mask compatibility and transactional rejection; CPU ONLY");
 return 0;
}
