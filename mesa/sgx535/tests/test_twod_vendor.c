/* SPDX-License-Identifier: MIT; captured vendor CPU oracle regression. */
#include "remesa_twod.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned word(const unsigned char *p){return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
int main(void){
 {struct remesa_twod_copy r={.source={1,0x40001000,4120576,1282,802,5128,1},.destination={2,0x41000000,4120576,1282,802,5128,1},.request_base=0x40000000,.source_x=1,.source_y=1,.destination_x=1,.destination_y=1,.width=1280,.height=800};
 const unsigned expected[8]={0xa0061408u,0x1000000u,0x90061408u,0x1000u,0x30001001u,0x8001ccccu,0x1001u,0x500320u};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 {struct remesa_twod_copy r={.source={1,0x40001000,1654784,642,642,2568,1},.destination={2,0x41000000,1654784,642,642,2568,1},.request_base=0x40000000,.source_x=1,.source_y=1,.destination_x=1,.destination_y=1,.width=640,.height=640};
 const unsigned expected[8]={0xa0060a08u,0x1000000u,0x90060a08u,0x1000u,0x30001001u,0x8001ccccu,0x1001u,0x280280u};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 {struct remesa_twod_copy r={.source={1,0x40001000,1245184,643,481,2572,1},.destination={2,0x41000000,1245184,643,481,2572,1},.request_base=0x40000000,.source_x=1,.source_y=1,.destination_x=1,.destination_y=1,.width=641,.height=479};
 const unsigned expected[8]={0xa0060a0cu,0x1000000u,0x90060a0cu,0x1000u,0x30001001u,0x8001ccccu,0x1001u,0x2811dfu};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 {struct remesa_twod_copy r={.source={1,0x40001000,12288,34,34,136,1},.destination={1,0x40001000,12288,34,34,136,1},.request_base=0x40000000,.source_x=2,.source_y=2,.destination_x=1,.destination_y=1,.width=32,.height=32};
 const unsigned expected[8]={0xa0060088u,0x1000u,0x90060088u,0x1000u,0x30002002u,0x8001ccccu,0x1001u,0x20020u};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 {struct remesa_twod_copy r={.source={1,0x40001000,12288,34,34,136,1},.destination={1,0x40001000,12288,34,34,136,1},.request_base=0x40000000,.source_x=1,.source_y=1,.destination_x=2,.destination_y=2,.width=32,.height=32};
 const unsigned expected[8]={0xa0060088u,0x1000u,0x90060088u,0x1000u,0x30020020u,0x8081ccccu,0x21021u,0x20020u};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 {struct remesa_twod_copy r={.source={1,0x40001000,12288,34,34,136,1},.destination={1,0x40001000,12288,34,34,136,1},.request_base=0x40000000,.source_x=1,.source_y=2,.destination_x=2,.destination_y=1,.width=32,.height=32};
 const unsigned expected[8]={0xa0060088u,0x1000u,0x90060088u,0x1000u,0x30020002u,0x8101ccccu,0x21001u,0x20020u};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 {struct remesa_twod_copy r={.source={1,0x40001000,12288,34,34,136,1},.destination={1,0x40001000,12288,34,34,136,1},.request_base=0x40000000,.source_x=2,.source_y=1,.destination_x=1,.destination_y=2,.width=32,.height=32};
 const unsigned expected[8]={0xa0060088u,0x1000u,0x90060088u,0x1000u,0x30002020u,0x8181ccccu,0x1021u,0x20020u};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 {struct remesa_twod_copy r={.source={1,0x40001000,12288,34,34,136,1},.destination={2,0x41000000,12288,34,34,136,1},.request_base=0x40000000,.source_x=1,.source_y=1,.destination_x=1,.destination_y=1,.width=32,.height=32};
 const unsigned expected[8]={0xa0060088u,0x1000000u,0x90060088u,0x1000u,0x30001001u,0x8001ccccu,0x1001u,0x20020u};
 struct {unsigned pre;struct remesa_twod_copy_plan p;unsigned post;} g={.pre=0x12345678,.post=0x87654321};
 struct remesa_twod_copy saved=r;
 assert(!remesa_twod_copy_prepare_vendor(&r,&g.p));assert(g.p.dwords==9);
 for(unsigned i=0;i<8;i++)assert(word(g.p.command+4*i)==expected[i]);
 assert(word(g.p.command+32)==0xf0000000u&&word(g.p.command+36)==0);
 assert(g.pre==0x12345678&&g.post==0x87654321&&!memcmp(&r,&saved,sizeof r));
 struct remesa_twod_copy_plan old=g.p;r.reserved=1;
 assert(remesa_twod_copy_prepare_vendor(&r,&g.p)==EINVAL&&!memcmp(&g.p,&old,sizeof old));
 assert(remesa_twod_copy_prepare_vendor(&saved,(void*)&saved)==EINVAL);
 assert(remesa_twod_copy_prepare_vendor(NULL,&g.p)==EINVAL);
 }
 puts("PASS eight captured vendor cases, effective size, guards, immutable inputs and transactional rejection; CPU only");return 0;}
