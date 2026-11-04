/* SPDX-License-Identifier: MIT */
#include "remesa/matrix_preserve_ta.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
struct observed {struct remesa_ta_reference refs[9];unsigned count,reject;};
static int collect(void *opaque,const struct remesa_ta_reference *ref)
{
 struct observed *o=opaque;assert(o->count<9);o->refs[o->count++]=*ref;
 return o->reject==o->count?EACCES:0;
}
int main(int argc,char **argv)
{
 unsigned i,k;_Alignas(8) unsigned char full[104],image[76],bad[76],alias[96];
 struct remesa_ta_reference_counts counts,sentinel={97,98};
 struct observed old={0},fresh={0};FILE *f;
 assert(argc==2);f=fopen(argv[1],"rb");assert(f);
 assert(fread(full,1,sizeof full,f)==sizeof full&&fgetc(f)==EOF&&!fclose(f));
 assert(!remesa_ta_gears_references_walk(full,sizeof full,collect,&old,&counts));
 assert(old.count==9&&counts.draws==1&&counts.references==9);
 /* Reuse actual producer bytes. No CODE, instruction or padding is added. */
 memcpy(image,full,28);memcpy(image+28,full+56,48);
 assert(!remesa_matrix_preserve_ta_walk(image,sizeof image,collect,&fresh,&counts));
 assert(counts.draws==1&&counts.references==8&&fresh.count==8);
 {const unsigned expected[8]={0,2,3,4,5,6,7,8};
  for(i=0;i<8;i++)assert(!memcmp(&fresh.refs[i],&old.refs[expected[i]],sizeof(fresh.refs[i])));
 }
 /* This length also has legacy gears syntax. Only the new typed consumer
  * authenticates INITIAL DATA rather than a Z-clear fragment state. Syntax
  * alone supplies neither LAB admission nor preserved-content authority. */
 assert(!remesa_ta_gears_references_walk(image,sizeof image,NULL,NULL,&counts));
 assert(remesa_ta_references_walk(image,sizeof image,NULL,NULL,&counts));
 assert(remesa_matrix_preserve_ta_walk(full,sizeof full,NULL,NULL,&counts));
 for(i=0;i<sizeof image;i++){
  counts=sentinel;fresh=(struct observed){0};
  assert(remesa_matrix_preserve_ta_walk(image,i,collect,&fresh,&counts));
  assert(!fresh.count&&!memcmp(&counts,&sentinel,sizeof counts));
 }
 {const unsigned words[]={4,16,24,32,40,52,60,68,72};
  for(i=0;i<sizeof(words)/sizeof(words[0]);i++)for(k=0;k<32;k++){
   memcpy(bad,image,sizeof bad);bad[words[i]+k/8]^=1u<<(k%8);
   counts=sentinel;fresh=(struct observed){0};
   assert(remesa_matrix_preserve_ta_walk(bad,sizeof bad,collect,&fresh,&counts));
   assert(!fresh.count&&!memcmp(&counts,&sentinel,sizeof counts));
  }
 }
 for(i=1;i<=8;i++){
  counts=sentinel;fresh=(struct observed){.reject=i};
  assert(remesa_matrix_preserve_ta_walk(image,sizeof image,collect,&fresh,&counts)==EACCES);
  assert(fresh.count==i&&!memcmp(&counts,&sentinel,sizeof counts));
 }
 memcpy(alias,image,sizeof image);counts=sentinel;
 assert(remesa_matrix_preserve_ta_walk(alias,sizeof image,NULL,NULL,(void *)(alias+8)));
 assert(remesa_matrix_preserve_ta_walk(NULL,sizeof image,NULL,NULL,&counts));
 assert(remesa_matrix_preserve_ta_walk(image,sizeof image,NULL,NULL,NULL));
 assert(remesa_matrix_preserve_ta_walk((void *)(~(uintptr_t)0-20),sizeof image,NULL,NULL,&counts));
 puts("matrix preserve TA: grammar/closure tests PASS; no GPU admission");return 0;
}
