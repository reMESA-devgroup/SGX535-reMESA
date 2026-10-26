/* SPDX-License-Identifier: MIT */
#include "remesa_shader_storage.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct remesa_shader_cookie { struct remesa_shader_cookie *next; };

struct remesa_shader_storage {
 uint32_t references;
 enum remesa_ir_stage stage;
 enum remesa_shader_storage_format format;
 enum remesa_shader_storage_status status;
 size_t byte_length,byte_alignment,allocation_length;
 void *allocation;
 const unsigned char *bytes;
};

static int overlaps(const void *a,size_t an,const void *b,size_t bn)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(!an||!bn)return 0;
 if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;
 return x<=y?y-x<an:x-y<bn;
}

int remesa_shader_cookie_output_check(const struct remesa_shader_cookie_pool *pool,
                                     const void *out,size_t bytes)
{
 if(!pool||!out||!bytes||bytes>UINTPTR_MAX-(uintptr_t)out||
    overlaps(out,bytes,pool,sizeof(*pool)))return EINVAL;
 for(const struct remesa_shader_cookie *c=pool->head;c;c=c->next)
  if(overlaps(out,bytes,c,sizeof(*c)))return EINVAL;
 return 0;
}
int remesa_shader_cookie_create(struct remesa_shader_cookie_pool *pool,void **out)
{
 int error=remesa_shader_cookie_output_check(pool,out,sizeof(*out));
 if(error)return error;
 if(*out)return EBUSY;
 if(pool->count>=REMESA_SHADER_COOKIE_LIMIT)return ENOSPC;
 struct remesa_shader_cookie *cookie=calloc(1,sizeof(*cookie));
 if(!cookie)return ENOMEM;
 cookie->next=pool->head;pool->head=cookie;++pool->count;
 *out=cookie;return 0;
}
void remesa_shader_cookie_pool_finish(struct remesa_shader_cookie_pool *pool)
{
 if(!pool)return;
 while(pool->head) {
  struct remesa_shader_cookie *cookie=pool->head;pool->head=cookie->next;free(cookie);
 }
 pool->count=0;
}

static int aliases_storage(const struct remesa_shader_storage *s,
                           const void *out,size_t length)
{
 return overlaps(out,length,s,sizeof(*s))||
        overlaps(out,length,s->allocation,s->allocation_length);
}

static int metadata(enum remesa_ir_stage stage,
                    enum remesa_shader_storage_format format,
                    size_t length,size_t alignment)
{
 if(stage!=REMESA_IR_VERTEX&&stage!=REMESA_IR_FRAGMENT)return EINVAL;
 size_t unit;
 if(format==REMESA_SHADER_STORAGE_HOST_CPU_RECORDS)unit=REMESA_SHADER_STORAGE_CPU_RECORD_BYTES;
 else if(format==REMESA_SHADER_STORAGE_HOST_PDS_DATA||
         format==REMESA_SHADER_STORAGE_HOST_PDS_STRUCTURE)unit=4;
 else return ENOTSUP;
 if(!length||length%unit)return EINVAL;
 if(length>REMESA_SHADER_STORAGE_MAX_BYTES)return E2BIG;
 if(!alignment||(alignment&(alignment-1)))return EINVAL;
 if(alignment>REMESA_SHADER_STORAGE_MAX_ALIGNMENT)return E2BIG;
 if(length>SIZE_MAX-(alignment-1))return EOVERFLOW;
 return 0;
}

int remesa_shader_storage_validate(const struct remesa_shader_storage *s)
{
 int err;
 uintptr_t base,bytes;
 if(!s)return EINVAL;
 err=metadata(s->stage,s->format,s->byte_length,s->byte_alignment);
 if(err)return err;
 if(!s->references||s->status!=REMESA_SHADER_STORAGE_CANDIDATE||
    !s->allocation||!s->bytes)return EPROTO;
 if(s->allocation_length!=s->byte_length+s->byte_alignment-1)return EPROTO;
 base=(uintptr_t)s->allocation;bytes=(uintptr_t)s->bytes;
 if(base>UINTPTR_MAX-s->allocation_length)return EOVERFLOW;
 if(bytes<base||bytes>base+s->allocation_length)return EPROTO;
 if(s->byte_length>base+s->allocation_length-bytes)return EPROTO;
 if(bytes&(s->byte_alignment-1))return EPROTO;
 return 0;
}

int remesa_shader_storage_create(const struct remesa_shader_storage_request *r,
                                struct remesa_shader_storage **out)
{
 struct remesa_shader_storage *s;
 uintptr_t base,aligned;
 int err;
 if(!r||!out||sizeof(*r)>UINTPTR_MAX-(uintptr_t)r||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out||
    overlaps(r,sizeof(*r),out,sizeof(*out)))return EINVAL;
 if(!r->bytes)return EINVAL;
 err=metadata(r->stage,r->format,r->byte_length,r->byte_alignment);
 if(err)return err;
 if(r->byte_length>UINTPTR_MAX-(uintptr_t)r->bytes||
    overlaps(r->bytes,r->byte_length,out,sizeof(*out)))return EINVAL;
 if(*out)return EBUSY;
 s=calloc(1,sizeof(*s));
 if(!s)return ENOMEM;
 s->allocation_length=r->byte_length+r->byte_alignment-1;
 s->allocation=malloc(s->allocation_length);
 if(!s->allocation){free(s);return ENOMEM;}
 base=(uintptr_t)s->allocation;
 if(base>UINTPTR_MAX-s->allocation_length){
  free(s->allocation);free(s);return EOVERFLOW;
 }
 aligned=(base+r->byte_alignment-1)&~(uintptr_t)(r->byte_alignment-1);
 s->references=1;s->stage=r->stage;s->format=r->format;
 s->status=REMESA_SHADER_STORAGE_CANDIDATE;
 s->byte_length=r->byte_length;s->byte_alignment=r->byte_alignment;
 s->bytes=(const unsigned char *)aligned;
 memcpy((void *)s->bytes,r->bytes,s->byte_length);
 *out=s;
 return 0;
}

int remesa_shader_storage_reference(struct remesa_shader_storage **dst,
                                   struct remesa_shader_storage *src)
{
 struct remesa_shader_storage *old;
 int err;
 if(!dst)return EINVAL;
 if(src){
  err=remesa_shader_storage_validate(src);if(err)return err;
  if(aliases_storage(src,dst,sizeof(*dst)))return EINVAL;
 }
 old=*dst;
 if(old){
  /* Check the known object range before any object fields are accessed. The
   * destination must hold a valid owned reference; an arbitrary byte buffer
   * is not a reference slot, even when it happens to contain pointer bits. */
  if(overlaps(dst,sizeof(*dst),old,sizeof(*old)))return EINVAL;
  err=remesa_shader_storage_validate(old);if(err)return err;
  if(aliases_storage(old,dst,sizeof(*dst)))return EINVAL;
 }
 if(src==old)return 0;
 if(src&&src->references==UINT32_MAX)return EOVERFLOW;
 if(src)src->references++;
 *dst=src;
 if(old&&!--old->references){free(old->allocation);free(old);}
 return 0;
}

int remesa_shader_storage_inspect(const struct remesa_shader_storage *s,
                                 struct remesa_shader_storage_view *out)
{
 struct remesa_shader_storage_view result;
 int err;
 if(!out)return EINVAL;
 err=remesa_shader_storage_validate(s);if(err)return err;
 if(aliases_storage(s,out,sizeof(*out)))return EINVAL;
 memset(&result,0,sizeof(result));
 result.stage=s->stage;result.format=s->format;result.status=s->status;
 result.bytes=s->bytes;result.byte_length=s->byte_length;
 result.byte_alignment=s->byte_alignment;
 *out=result;
 return 0;
}

int remesa_shader_storage_range(const struct remesa_shader_storage *s,
                               size_t offset,size_t length,const void **out)
{
 int err;
 if(!out)return EINVAL;
 err=remesa_shader_storage_validate(s);if(err)return err;
 if(aliases_storage(s,out,sizeof(*out)))return EINVAL;
 if(offset>s->byte_length||length>s->byte_length-offset)return ERANGE;
 *out=s->bytes+offset;
 return 0;
}

int remesa_shader_storage_validate_execution(const struct remesa_shader_storage *s)
{
 int err=remesa_shader_storage_validate(s);
 return err?err:ENOTSUP;
}
