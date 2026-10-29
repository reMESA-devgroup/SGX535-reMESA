/* SPDX-License-Identifier: MIT */
/* CPU-only experimental allocation/locale dependency; no device API. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stddef.h>
struct allocation {struct allocation *next;size_t size;uint32_t magic;_Alignas(max_align_t) unsigned char guard[16];};
static struct allocation *head;
static size_t bytes,peak,live,count;
static void fail(const char *s){fprintf(stderr,"CPU_STUB_FAILURE %s\n",s);_Exit(90);}
static void verify(struct allocation *a)
{
 unsigned char *p=(unsigned char *)(a+1);
 if(a->magic!=0x4350554d)fail("allocation header");
 for(unsigned i=0;i<16;i++)if(a->guard[i]!=0xa5||p[a->size+i]!=0xa5)fail("allocation guard");
}
void *PVRSRVAllocUserModeMem(uint32_t size)
{
 if(size>32u*1024*1024||bytes>128u*1024*1024-size||live>=65536)fail("allocation bound");
 struct allocation *a=malloc(sizeof(*a)+(size_t)size+16);if(!a)return NULL;
 a->next=head;a->size=size;a->magic=0x4350554d;memset(a->guard,0xa5,16);
 unsigned char *p=(unsigned char *)(a+1);memset(p,0xcc,size);memset(p+size,0xa5,16);
 head=a;bytes+=size;live++;count++;if(bytes>peak)peak=bytes;return p;
}
void *PVRSRVCallocUserModeMem(uint32_t size)
{void *p=PVRSRVAllocUserModeMem(size);if(p)memset(p,0,size);return p;}
void PVRSRVFreeUserModeMem(void *p)
{
 if(!p)return;
 struct allocation **link=&head;
 while(*link&&(void *)(*link+1)!=p)link=&(*link)->next;
 if(!*link)fail("foreign or duplicate free");
 struct allocation *a=*link;verify(a);*link=a->next;
 bytes-=a->size;live--;a->magic=0;free(a);
}
void *PVRSRVReallocUserModeMem(void *p,uint32_t size)
{
 if(!p)return PVRSRVAllocUserModeMem(size);
 struct allocation *old=head;
 while(old&&(void *)(old+1)!=p)old=old->next;
 if(!old)fail("foreign realloc");
 verify(old);
 if(!size){PVRSRVFreeUserModeMem(p);return NULL;}
 void *next=PVRSRVAllocUserModeMem(size);if(!next)return NULL;
 memcpy(next,p,old->size<size?old->size:size);PVRSRVFreeUserModeMem(p);return next;
}
char *PVRSRVSetLocale(const char *s){return setlocale(LC_ALL,s);}
size_t cpu_owned_size(void *p)
{
 for(struct allocation *a=head;a;a=a->next)if((void *)(a+1)==p){verify(a);return a->size;}
 return 0;
}
void cpu_allocation_check(void)
{
 for(struct allocation *a=head;a;a=a->next)verify(a);
 printf("ALLOCATIONS total=%zu live=%zu bytes=%zu peak=%zu\n",count,live,bytes,peak);
}

/* Checked interior view used by the separate offline USP finalizer oracle. */
int cpu_range_owned(const void *p,size_t n)
{
 uintptr_t v=(uintptr_t)p;
 for(struct allocation *a=head;a;a=a->next){
  verify(a);uintptr_t b=(uintptr_t)(a+1);
  if(v>=b&&v-b<=a->size&&n<=a->size-(v-b))return 1;
 }
 return 0;
}
