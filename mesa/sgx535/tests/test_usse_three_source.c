/* SPDX-License-Identifier: MIT */
/* Original CPU field codec. Independent records arrive only on stdin;
 * no historical instruction bytes are embedded or admitted to GPU execution. */
#include "remesa_usse.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { assert(c); ++checks; } while (0)
struct field { unsigned word, shift, width; size_t offset; };
#define MEMBER(m) offsetof(struct remesa_usse_cpu_three_source,m)
/* Independent field table from the complete selected CPU writer and helpers.
 * These positions name numerical arguments, not hardware banks or semantics. */
static const struct field fields[] = {
 {0,14,7,MEMBER(source[0].number)}, {0,7,7,MEMBER(source[1].number)},
 {0,0,7,MEMBER(source[2].number)}, {1,2,1,MEMBER(source[0].class_id)},
 {0,30,2,MEMBER(source[1].class_id)}, {0,28,2,MEMBER(source[2].class_id)},
 {1,19,1,MEMBER(source_extension[0])}, {1,17,1,MEMBER(source_extension[1])},
 {1,16,1,MEMBER(source_extension[2])}, {0,21,1,MEMBER(word0_control21)},
 {0,22,6,MEMBER(word0_control22_27)}, {1,0,2,MEMBER(word1_control0_1)},
 {1,12,2,MEMBER(word1_control12_13)}, {1,18,1,MEMBER(word1_control18)}
};
static void little_endian(unsigned char *p,uint32_t word)
{for(unsigned n=0;n<4;n++)p[n]=(unsigned char)(word>>(8*n));}
static void synthetic(unsigned char bytes[8],const uint32_t words[2])
{little_endian(bytes,words[0]);little_endian(bytes+4,words[1]);}
static void unchanged_rejection(const void *bytes,size_t length,int error)
{
 struct remesa_usse_cpu_three_source actual,saved;
 memset(&actual,0xa5,sizeof(actual));saved=actual;
 CHECK(remesa_usse_cpu_three_source_decode(bytes,length,&actual)==error);
 CHECK(!memcmp(&actual,&saved,sizeof(actual)));
}
static void captured(const unsigned char *bytes,unsigned source1,unsigned mode)
{
 struct remesa_usse_cpu_three_source value,expected={
  .source={{0,0},{2,source1},{2,0}},.source_extension={0,1,1},
  .word0_control21=1,.word1_control12_13=mode,.word1_control18=1
 };
 CHECK(!remesa_usse_cpu_three_source_decode(bytes,8,&value));
 CHECK(!memcmp(&value,&expected,sizeof(value)));
 unsigned char encoded[12],old[12];memset(encoded,0xa5,sizeof(encoded));memcpy(old,encoded,sizeof(old));
 size_t written=99;
 CHECK(!remesa_usse_cpu_three_source_encode(&expected,encoded,sizeof(encoded),&written));
 CHECK(written==8&&!memcmp(encoded,bytes,8)&&!memcmp(encoded+8,old+8,4));
 CHECK(remesa_usse_cpu_three_source_encode(&expected,encoded,7,&written)==ENOSPC&&written==8);

 struct remesa_usse_cpu_move move,saved;
 memset(&move,0x7a,sizeof(move));saved=move;
 CHECK(remesa_usse_cpu_move_decode(bytes,8,&move)==ENOTSUP);
 CHECK(!memcmp(&move,&saved,sizeof(move)));
}
int main(void)
{
 unsigned char observed[16];
 CHECK(fread(observed,1,sizeof(observed),stdin)==sizeof(observed));
 CHECK(getchar()==EOF);
 captured(observed,0,1);captured(observed+8,11,0);
 /* This artificial record is assembled solely from the documented field
  * partition. It is not uploaded, emitted by the driver, or hardware-qualified. */
 const uint32_t fixed=(UINT32_C(31)<<27)|(UINT32_C(3)<<24)|
  (UINT32_C(1)<<21)|(UINT32_C(1)<<14);
 unsigned char bytes[8];uint32_t words[2]={0,fixed};
 struct remesa_usse_cpu_three_source zero={0},actual;
 synthetic(bytes,words);
 CHECK(!remesa_usse_cpu_three_source_decode(bytes,8,&actual));
 CHECK(!memcmp(&actual,&zero,sizeof(actual)));
 uint32_t mutable[2]={0,0};
 for(unsigned n=0;n<sizeof(fields)/sizeof(fields[0]);n++) {
  const struct field *f=&fields[n];uint32_t maximum=(UINT32_C(1)<<f->width)-1;
  mutable[f->word]|=maximum<<f->shift;
  const uint32_t boundaries[2]={1,maximum};
  for(unsigned k=0;k<2;k++) {
   struct remesa_usse_cpu_three_source expected={0};
   memcpy((unsigned char *)&expected+f->offset,&boundaries[k],sizeof(uint32_t));
   words[0]=0;words[1]=fixed;words[f->word]|=boundaries[k]<<f->shift;
   synthetic(bytes,words);
   CHECK(!remesa_usse_cpu_three_source_decode(bytes,8,&actual));
   CHECK(!memcmp(&actual,&expected,sizeof(actual)));
   unsigned char emitted[8];size_t emitted_bytes=0;
   CHECK(!remesa_usse_cpu_three_source_encode(&expected,emitted,sizeof(emitted),&emitted_bytes));
   CHECK(emitted_bytes==8&&!memcmp(emitted,bytes,8));
   struct remesa_usse_cpu_three_source invalid=expected;uint32_t excess=maximum+1;
   memcpy((unsigned char *)&invalid+f->offset,&excess,sizeof(excess));
   memset(emitted,0xa5,sizeof(emitted));unsigned char saved_bytes[8];memcpy(saved_bytes,emitted,8);emitted_bytes=71;
   CHECK(remesa_usse_cpu_three_source_encode(&invalid,emitted,sizeof(emitted),&emitted_bytes)==ENOTSUP);
   CHECK(emitted_bytes==71&&!memcmp(emitted,saved_bytes,8));

  }
 }
 CHECK(mutable[0]==UINT32_MAX);
 /* Exercise all field maxima together to expose overlapping extraction. */
 words[0]=mutable[0];words[1]=fixed|mutable[1];synthetic(bytes,words);
 CHECK(!remesa_usse_cpu_three_source_decode(bytes,8,&actual));
 for(unsigned n=0;n<sizeof(fields)/sizeof(fields[0]);n++) {
  uint32_t value;memcpy(&value,(unsigned char *)&actual+fields[n].offset,sizeof(value));
  CHECK(value==((UINT32_C(1)<<fields[n].width)-1));
 }
 /* Every unknown or fixed bit has a falsifiable rejection, not ignored data. */
 unsigned rejected_bits=0;
 for(unsigned bit=0;bit<32;bit++)if(!(mutable[1]&(UINT32_C(1)<<bit))) {
  words[0]=0;words[1]=fixed^(UINT32_C(1)<<bit);synthetic(bytes,words);
  unchanged_rejection(bytes,8,ENOTSUP);++rejected_bits;
 }
 const size_t bad_lengths[]={0,1,7,9,SIZE_MAX};
 for(unsigned n=0;n<sizeof(bad_lengths)/sizeof(bad_lengths[0]);n++)
  unchanged_rejection(observed,bad_lengths[n],ENOTSUP);
 unchanged_rejection(NULL,8,EINVAL);
 CHECK(remesa_usse_cpu_three_source_decode(observed,8,NULL)==EINVAL);
 unchanged_rejection((const void *)(UINTPTR_MAX-3),8,EINVAL);
 CHECK(remesa_usse_cpu_three_source_decode(observed,8,
  (void *)(UINTPTR_MAX-3))==EINVAL);
 /* Full and partial input/output overlap, with accessible aligned storage. */
 union { max_align_t alignment;unsigned char bytes[192]; } area,saved;
 const size_t input_offsets[]={0,8,sizeof(actual)-4};
 const size_t output_offsets[]={0,12,0};
 for(unsigned n=0;n<3;n++) {
  memset(&area,0x6b,sizeof(area));memcpy(area.bytes+input_offsets[n],observed,8);saved=area;
  CHECK(remesa_usse_cpu_three_source_decode(area.bytes+input_offsets[n],8,
   (void *)(area.bytes+output_offsets[n]))==EINVAL);
  CHECK(!memcmp(&area,&saved,sizeof(area)));
 }
 /* Encoder rejection must preserve the original typed input and output. */
 struct remesa_usse_cpu_three_source input={0},copy=input;
 unsigned char result[8],saved_result[8];memset(result,0xa5,8);memcpy(saved_result,result,8);size_t size=99;
 CHECK(remesa_usse_cpu_three_source_encode(NULL,result,8,&size)==EINVAL);
 CHECK(remesa_usse_cpu_three_source_encode(&input,NULL,8,&size)==EINVAL);
 CHECK(remesa_usse_cpu_three_source_encode(&input,result,8,NULL)==EINVAL);
 CHECK(remesa_usse_cpu_three_source_encode(&input,result,7,&size)==ENOSPC);
 CHECK(size==99&&!memcmp(result,saved_result,8));
 CHECK(remesa_usse_cpu_three_source_encode(&input,&input,sizeof(input),&size)==EINVAL);
 CHECK(remesa_usse_cpu_three_source_encode(&input,result,8,(void *)&input)==EINVAL);
 CHECK(!memcmp(&input,&copy,sizeof(copy)));
 CHECK(remesa_usse_cpu_three_source_encode((void *)(UINTPTR_MAX-3),result,8,&size)==EINVAL);
 CHECK(remesa_usse_cpu_three_source_encode(&input,(void *)(UINTPTR_MAX-3),8,&size)==EINVAL);
 CHECK(remesa_usse_cpu_three_source_encode(&input,result,8,(void *)(UINTPTR_MAX-3))==EINVAL);
 CHECK(size==99&&!memcmp(result,saved_result,8));
 printf("{\"status\":\"PASS_CPU_ONLY\",\"checks\":%u,\"captured_records\":2,"
  "\"independent_fields\":14,\"rejected_word1_bits\":%u,"
  "\"physical_semantics_verified\":false,\"GPU_submissions\":0}\n",checks,rejected_bits);
 return 0;
}
