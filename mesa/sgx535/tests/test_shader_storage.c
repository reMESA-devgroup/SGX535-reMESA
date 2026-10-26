/* SPDX-License-Identifier: MIT
 * Original candidate bytes below exercise storage only; no GPU instruction.
 */
#include "remesa_shader_storage.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void __real_free(void *);
void *__wrap_malloc(size_t);
void *__wrap_calloc(size_t,size_t);
void __wrap_free(void *);
static unsigned checks,live_allocations;
static int tracking,fail_after=-1;
void *__wrap_malloc(size_t size)
{
 void *p;
 if(tracking&&fail_after==0)return NULL;
 if(tracking&&fail_after>0)fail_after--;
 p=__real_malloc(size);
 if(tracking&&p)live_allocations++;
 return p;
}
void *__wrap_calloc(size_t count,size_t size)
{
 void *p;
 if(tracking&&fail_after==0)return NULL;
 if(tracking&&fail_after>0)fail_after--;
 p=__real_calloc(count,size);
 if(tracking&&p)live_allocations++;
 return p;
}
void __wrap_free(void *p)
{
 if(tracking&&p){if(!live_allocations)abort();live_allocations--;}
 __real_free(p);
}
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);abort();}} while(0)

int main(void)
{
 struct remesa_shader_cookie_pool cookies={0};
 void *cookie=NULL;
 CHECK(remesa_shader_cookie_create(NULL,&cookie)==EINVAL&&!cookie);
 CHECK(remesa_shader_cookie_create(&cookies,NULL)==EINVAL);
 CHECK(remesa_shader_cookie_create(&cookies,(void *)&cookies)==EINVAL&&!cookies.count);
 tracking=1;fail_after=0;
 CHECK(remesa_shader_cookie_create(&cookies,&cookie)==ENOMEM&&!cookie&&!cookies.count);
 fail_after=-1;
 CHECK(!remesa_shader_cookie_create(&cookies,&cookie)&&cookies.count==1&&live_allocations==1);
 void *first=cookie;
 CHECK(remesa_shader_cookie_create(&cookies,&cookie)==EBUSY&&cookie==first&&cookies.count==1);
 CHECK(remesa_shader_cookie_create(&cookies,(void *)first)==EINVAL);
 CHECK(remesa_shader_cookie_output_check(&cookies,(void *)(UINTPTR_MAX-3),8)==EINVAL);
 /* Retired identity nodes remain unique even after payload destruction. */
 for(unsigned n=1;n<REMESA_SHADER_COOKIE_LIMIT;n++) {
  cookie=NULL;
  CHECK(!remesa_shader_cookie_create(&cookies,&cookie)&&cookie!=first);
 }
 cookie=NULL;
 CHECK(remesa_shader_cookie_create(&cookies,&cookie)==ENOSPC&&!cookie&&
       cookies.count==REMESA_SHADER_COOKIE_LIMIT&&live_allocations==REMESA_SHADER_COOKIE_LIMIT);
 remesa_shader_cookie_pool_finish(&cookies);
 CHECK(!cookies.count&&!cookies.head&&!live_allocations);
 remesa_shader_cookie_pool_finish(&cookies);
 tracking=0;
 unsigned char bytes[REMESA_SHADER_STORAGE_MAX_BYTES];
 struct remesa_shader_storage_request r;
 struct remesa_shader_storage *s=NULL,*alias=NULL,*replacement=NULL;
 struct remesa_shader_storage_view v,saved;
 const void *part=(const void *)(uintptr_t)1;
 size_t i,alignment;
 int err;
 for(i=0;i<sizeof(bytes);i++)bytes[i]=(unsigned char)(i*17u+3u);
 memset(&r,0,sizeof(r));r.stage=REMESA_IR_FRAGMENT;
 r.format=REMESA_SHADER_STORAGE_HOST_CPU_RECORDS;r.bytes=bytes;
 r.byte_length=56;r.byte_alignment=8;
 tracking=1;
 CHECK(remesa_shader_storage_create(&r,&s)==0);
 CHECK(live_allocations==2);
 CHECK(remesa_shader_storage_validate(s)==0);
 CHECK(remesa_shader_storage_inspect(s,&v)==0);
 CHECK(v.stage==REMESA_IR_FRAGMENT&&v.format==REMESA_SHADER_STORAGE_HOST_CPU_RECORDS);
 CHECK(v.status==REMESA_SHADER_STORAGE_CANDIDATE&&v.byte_length==56&&v.byte_alignment==8);
 CHECK(v.bytes!=bytes&&!memcmp(v.bytes,bytes,56));
 CHECK(!((uintptr_t)v.bytes&7));
 saved=v;
 memset(bytes,0,56);
 CHECK(memcmp(v.bytes,bytes,56)!=0);
 CHECK(remesa_shader_storage_validate_execution(s)==ENOTSUP);
 CHECK(remesa_shader_storage_create(&r,&s)==EBUSY&&live_allocations==2);
 CHECK(remesa_shader_storage_reference(&alias,s)==0&&alias==s);
 CHECK(remesa_shader_storage_reference(&alias,alias)==0);
 CHECK(remesa_shader_storage_reference(&s,NULL)==0&&s==NULL&&live_allocations==2);
 CHECK(remesa_shader_storage_inspect(alias,&v)==0&&!memcmp(&v,&saved,sizeof(v)));
 {
  unsigned char retained[56];
  memcpy(retained,v.bytes,sizeof(retained));
  CHECK(remesa_shader_storage_inspect(alias,(struct remesa_shader_storage_view *)(void *)alias)==EINVAL);
  CHECK(remesa_shader_storage_inspect(alias,(struct remesa_shader_storage_view *)(void *)v.bytes)==EINVAL);
  CHECK(remesa_shader_storage_range(alias,0,8,(const void **)(void *)alias)==EINVAL);
  CHECK(remesa_shader_storage_range(alias,0,8,(const void **)(void *)v.bytes)==EINVAL);
  /* The alias rejection must precede reading an alleged reference pointer:
   * the record's leading bytes deliberately do not contain a valid pointer. */
  CHECK(remesa_shader_storage_reference((struct remesa_shader_storage **)(void *)alias,alias)==EINVAL);
  CHECK(remesa_shader_storage_reference((struct remesa_shader_storage **)(void *)v.bytes,alias)==EINVAL);
  CHECK(remesa_shader_storage_validate(alias)==0&&live_allocations==2);
  CHECK(!memcmp(retained,v.bytes,sizeof(retained)));
 }
 CHECK(remesa_shader_storage_range(alias,8,16,&part)==0&&part==(const unsigned char *)v.bytes+8);
 CHECK(remesa_shader_storage_range(alias,56,0,&part)==0&&part==(const unsigned char *)v.bytes+56);
 part=(const void *)(uintptr_t)1;
 CHECK(remesa_shader_storage_range(alias,57,0,&part)==ERANGE&&part==(const void *)(uintptr_t)1);
 CHECK(remesa_shader_storage_range(alias,48,9,&part)==ERANGE&&part==(const void *)(uintptr_t)1);
 CHECK(remesa_shader_storage_range(alias,SIZE_MAX,SIZE_MAX,&part)==ERANGE&&part==(const void *)(uintptr_t)1);
 CHECK(remesa_shader_storage_range(alias,0,1,NULL)==EINVAL);
 CHECK(remesa_shader_storage_create(&r,&replacement)==0&&live_allocations==4);
 CHECK(remesa_shader_storage_reference(&alias,replacement)==0&&live_allocations==2);
 CHECK(remesa_shader_storage_reference(&replacement,NULL)==0&&live_allocations==2);
 CHECK(remesa_shader_storage_reference(&alias,NULL)==0&&live_allocations==0);
 CHECK(remesa_shader_storage_reference(&alias,NULL)==0&&live_allocations==0);
 CHECK(remesa_shader_storage_reference(NULL,NULL)==EINVAL);
 CHECK(remesa_shader_storage_validate(NULL)==EINVAL);
 CHECK(remesa_shader_storage_validate_execution(NULL)==EINVAL);
 memset(&v,0xa5,sizeof(v));saved=v;
 CHECK(remesa_shader_storage_inspect(NULL,&v)==EINVAL&&!memcmp(&v,&saved,sizeof(v)));
 CHECK(remesa_shader_storage_create(NULL,&s)==EINVAL);
 CHECK(remesa_shader_storage_create(&r,NULL)==EINVAL);
 r.bytes=NULL;CHECK(remesa_shader_storage_create(&r,&s)==EINVAL&&s==NULL);r.bytes=bytes;
 r.stage=(enum remesa_ir_stage)0;CHECK(remesa_shader_storage_create(&r,&s)==EINVAL&&s==NULL);r.stage=REMESA_IR_VERTEX;
 r.format=(enum remesa_shader_storage_format)4;CHECK(remesa_shader_storage_create(&r,&s)==ENOTSUP&&s==NULL);r.format=REMESA_SHADER_STORAGE_HOST_CPU_RECORDS;
 r.byte_length=0;CHECK(remesa_shader_storage_create(&r,&s)==EINVAL&&s==NULL);
 r.byte_length=55;CHECK(remesa_shader_storage_create(&r,&s)==EINVAL&&s==NULL);
 r.byte_length=REMESA_SHADER_STORAGE_MAX_BYTES+8u;CHECK(remesa_shader_storage_create(&r,&s)==E2BIG&&s==NULL);
 r.byte_length=SIZE_MAX-7u;CHECK(remesa_shader_storage_create(&r,&s)==E2BIG&&s==NULL);
 r.byte_length=56;r.byte_alignment=0;CHECK(remesa_shader_storage_create(&r,&s)==EINVAL&&s==NULL);
 r.byte_alignment=3;CHECK(remesa_shader_storage_create(&r,&s)==EINVAL&&s==NULL);
 r.byte_alignment=REMESA_SHADER_STORAGE_MAX_ALIGNMENT*2u;CHECK(remesa_shader_storage_create(&r,&s)==E2BIG&&s==NULL);
 r.byte_alignment=8;
 /* Reject publication into input storage before allocation or pointer reads. */
 {
  union { unsigned char bytes[64]; struct remesa_shader_storage *out; } u;
  unsigned char original[64];struct remesa_shader_storage_request bad=r;
  memset(&u,0,sizeof(u));memcpy(original,u.bytes,sizeof(original));
  bad.bytes=u.bytes;bad.byte_length=sizeof(u.bytes);
  CHECK(remesa_shader_storage_create(&bad,&u.out)==EINVAL&&!live_allocations);
  CHECK(!memcmp(u.bytes,original,sizeof(original)));
  struct remesa_shader_storage_request saved_request=bad;
  CHECK(remesa_shader_storage_create(&bad,(void *)&bad)==EINVAL&&!live_allocations);
  CHECK(!memcmp(&bad,&saved_request,sizeof(bad)));
  CHECK(remesa_shader_storage_create((void *)(UINTPTR_MAX-3),&s)==EINVAL&&!s);
  CHECK(remesa_shader_storage_create(&r,(void *)(UINTPTR_MAX-3))==EINVAL&&!s);
  bad=r;bad.bytes=(void *)(UINTPTR_MAX-3);
  CHECK(remesa_shader_storage_create(&bad,&s)==EINVAL&&!s&&!live_allocations);
 }
 for(i=0;i<2;i++){
  fail_after=(int)i;err=remesa_shader_storage_create(&r,&s);fail_after=-1;
  CHECK(err==ENOMEM&&s==NULL&&live_allocations==0);
 }
 /* HOST PDS DATA has its own scalar serialization unit, no ISA admission. */
 r.format=REMESA_SHADER_STORAGE_HOST_PDS_DATA;r.byte_length=4;
 CHECK(!remesa_shader_storage_create(&r,&s)&&live_allocations==2);
 CHECK(!remesa_shader_storage_inspect(s,&v)&&v.byte_length==4&&v.format==REMESA_SHADER_STORAGE_HOST_PDS_DATA);
 CHECK(remesa_shader_storage_validate_execution(s)==ENOTSUP);
 CHECK(!remesa_shader_storage_reference(&s,NULL)&&!s&&!live_allocations);
 r.format=REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE;
 CHECK(!remesa_shader_storage_create(&r,&s)&&live_allocations==2);
 CHECK(!remesa_shader_storage_inspect(s,&v)&&v.format==REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE);
 CHECK(remesa_shader_storage_validate_execution(s)==ENOTSUP);
 CHECK(!remesa_shader_storage_reference(&s,NULL)&&!live_allocations);
 r.format=REMESA_SHADER_STORAGE_HOST_PDS_DATA;
 r.byte_length=6;CHECK(remesa_shader_storage_create(&r,&s)==EINVAL&&!s&&!live_allocations);
 r.byte_length=16;
 for(i=0;i<2;i++){
  fail_after=(int)i;err=remesa_shader_storage_create(&r,&s);fail_after=-1;
  CHECK(err==ENOMEM&&!s&&!live_allocations);
 }
 r.format=REMESA_SHADER_STORAGE_HOST_CPU_RECORDS;
 r.byte_length=sizeof(bytes);
 for(alignment=1;alignment<=REMESA_SHADER_STORAGE_MAX_ALIGNMENT;alignment*=2){
  r.byte_alignment=alignment;
  CHECK(remesa_shader_storage_create(&r,&s)==0&&live_allocations==2);
  CHECK(remesa_shader_storage_inspect(s,&v)==0&&v.stage==REMESA_IR_VERTEX);
  CHECK(!((uintptr_t)v.bytes&(alignment-1))&&v.byte_length==sizeof(bytes));
  CHECK(!memcmp(v.bytes,bytes,sizeof(bytes))&&remesa_shader_storage_validate_execution(s)==ENOTSUP);
  CHECK(remesa_shader_storage_reference(&s,NULL)==0&&s==NULL&&live_allocations==0);
 }
 tracking=0;
 printf("PASS %u immutable shader-storage cases; candidate host bytes only, no GPU upload\n",checks);
 return 0;
}
