/* SPDX-License-Identifier: MIT */
/* Compile-only userspace compatibility evidence. No device access or ioctl. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
/* headers_install strips this kernel address-space annotation for userspace.
 * Keep original raw DRM files unchanged and erase only that annotation here. */
#ifndef __user
#define __user
#endif
#include "remesa/memory_uapi.h"
#include "remesa/native_uapi.h"
#include "remesa/retained_uapi.h"
#include "remesa/twod_lab_uapi.h"
#include "remesa/twod_copy_uapi.h"
#include "remesa/twod_fill_uapi.h"
#include "remesa/private_probe_uapi.h"
#include "remesa/visible_readback_uapi.h"
#include "remesa/shader_uapi.h"
#include "remesa/lab_uapi.h"
_Static_assert(CHAR_BIT==8,"wire bytes must have 8 bits");
_Static_assert(sizeof(__u64)==8,"wire u64 width");
_Static_assert(sizeof(struct remesa_memory_request)==4152,"existing memory wire bytes");
_Static_assert(sizeof(struct remesa_private_probe_request)==112,"existing private probe wire bytes");
_Static_assert(sizeof(struct remesa_shader_request)==REMESA_SHADER_REQUEST_BYTES,"shader wire bytes");
static void field(const char *name,size_t offset,bool *first)
{printf("%s\"%s\":%zu",*first?"":",",name,offset);*first=false;}
#define BEGIN(T,C,V) do { \
 printf("%s\"%s\":{\"size\":%zu,\"alignment\":%zu,\"ioctl\":%llu,\"version\":%u,\"fields\":{", \
 first_request?"":",",#T,sizeof(struct T),_Alignof(struct T),(unsigned long long)(C),(unsigned)(V)); \
 first_request=false;bool first_field=true;
#define F(T,M) field(#M,offsetof(struct T,M),&first_field)
#define END printf("}}"); }while(0)
int main(void)
{
 bool first_request=true;printf("{\"word_bits\":%zu,\"wire\":{",sizeof(void *)*CHAR_BIT);
 BEGIN(remesa_memory_request,DRM_IOCTL_REMESA_MEMORY,REMESA_MEMORY_VERSION);
 F(remesa_memory_request,version);F(remesa_memory_request,action);F(remesa_memory_request,id);
 F(remesa_memory_request,flags);F(remesa_memory_request,context);F(remesa_memory_request,bytes);
 F(remesa_memory_request,va);F(remesa_memory_request,offset);F(remesa_memory_request,length);
 F(remesa_memory_request,reserved);F(remesa_memory_request,data);END;
 BEGIN(remesa_native_draw,DRM_IOCTL_REMESA_NATIVE_DRAW,REMESA_NATIVE_ABI_VERSION);
 F(remesa_native_draw,version);F(remesa_native_draw,flags);F(remesa_native_draw,width);F(remesa_native_draw,height);
 F(remesa_native_draw,vertex_count);F(remesa_native_draw,index_count);F(remesa_native_draw,argb);F(remesa_native_draw,reserved);
 F(remesa_native_draw,positions);F(remesa_native_draw,indices);F(remesa_native_draw,reserved2);F(remesa_native_draw,operation);
 F(remesa_native_draw,operation_errno);F(remesa_native_draw,status);F(remesa_native_draw,phase);F(remesa_native_draw,events);
 F(remesa_native_draw,issued);F(remesa_native_draw,published);F(remesa_native_draw,color);END;
 BEGIN(remesa_retained_request,DRM_IOCTL_REMESA_RETAINED,REMESA_RETAINED_VERSION);
 F(remesa_retained_request,version);F(remesa_retained_request,flags);F(remesa_retained_request,context);
 F(remesa_retained_request,bindings);F(remesa_retained_request,commands);F(remesa_retained_request,reserved);
 F(remesa_retained_request,operation);F(remesa_retained_request,serial);F(remesa_retained_request,slot);
 F(remesa_retained_request,generation);F(remesa_retained_request,state);F(remesa_retained_request,error);
 F(remesa_retained_request,admitted);F(remesa_retained_request,attached);F(remesa_retained_request,observer_request_bytes);
 F(remesa_retained_request,observer_response_bytes);F(remesa_retained_request,observer_request);F(remesa_retained_request,observer_response);END;
 BEGIN(remesa_twod_lab_request,DRM_IOCTL_REMESA_TWOD_LAB,REMESA_TWOD_LAB_VERSION);
 F(remesa_twod_lab_request,version);F(remesa_twod_lab_request,flags);F(remesa_twod_lab_request,context);
 F(remesa_twod_lab_request,source);F(remesa_twod_lab_request,destination);F(remesa_twod_lab_request,width);
 F(remesa_twod_lab_request,operation);F(remesa_twod_lab_request,cookie);F(remesa_twod_lab_request,error);
 F(remesa_twod_lab_request,phase_ns);END;
 BEGIN(remesa_twod_copy_request,DRM_IOCTL_REMESA_TWOD_COPY,REMESA_TWOD_COPY_VERSION);
 F(remesa_twod_copy_request,version);F(remesa_twod_copy_request,flags);F(remesa_twod_copy_request,context);
 F(remesa_twod_copy_request,source);F(remesa_twod_copy_request,destination);F(remesa_twod_copy_request,source_x);
 F(remesa_twod_copy_request,source_y);F(remesa_twod_copy_request,destination_x);F(remesa_twod_copy_request,destination_y);
 F(remesa_twod_copy_request,width);F(remesa_twod_copy_request,height);F(remesa_twod_copy_request,budget_ms);
 F(remesa_twod_copy_request,reserved);F(remesa_twod_copy_request,operation);F(remesa_twod_copy_request,cookie);
 F(remesa_twod_copy_request,stages);F(remesa_twod_copy_request,retained);F(remesa_twod_copy_request,core);
 F(remesa_twod_copy_request,revision);F(remesa_twod_copy_request,error);F(remesa_twod_copy_request,response_reserved);END;
 BEGIN(remesa_twod_fill_request,DRM_IOCTL_REMESA_TWOD_FILL,REMESA_TWOD_FILL_VERSION);
 F(remesa_twod_fill_request,version);F(remesa_twod_fill_request,flags);F(remesa_twod_fill_request,context);
 F(remesa_twod_fill_request,destination);F(remesa_twod_fill_request,destination_x);F(remesa_twod_fill_request,destination_y);
 F(remesa_twod_fill_request,width);F(remesa_twod_fill_request,height);F(remesa_twod_fill_request,color);
 F(remesa_twod_fill_request,budget_ms);F(remesa_twod_fill_request,reserved);F(remesa_twod_fill_request,operation);
 F(remesa_twod_fill_request,cookie);F(remesa_twod_fill_request,stages);F(remesa_twod_fill_request,retained);
 F(remesa_twod_fill_request,core);F(remesa_twod_fill_request,revision);F(remesa_twod_fill_request,error);
 F(remesa_twod_fill_request,response_reserved);END;
 BEGIN(remesa_private_probe_request,DRM_IOCTL_REMESA_PRIVATE_PROBE,REMESA_PRIVATE_PROBE_VERSION);
 F(remesa_private_probe_request,version);F(remesa_private_probe_request,flags);F(remesa_private_probe_request,context);
 F(remesa_private_probe_request,ids);F(remesa_private_probe_request,budget_ms);F(remesa_private_probe_request,operation);
 F(remesa_private_probe_request,cookie);F(remesa_private_probe_request,core);F(remesa_private_probe_request,revision);
 F(remesa_private_probe_request,phase);F(remesa_private_probe_request,ledger);F(remesa_private_probe_request,error);
 F(remesa_private_probe_request,retained);F(remesa_private_probe_request,stages);F(remesa_private_probe_request,reserved);END;
 /* Zero means no ioctl is assigned by these envelope headers. */
 BEGIN(remesa_visible_readback_request,0,REMESA_VISIBLE_READBACK_VERSION);
 F(remesa_visible_readback_request,version);F(remesa_visible_readback_request,id);F(remesa_visible_readback_request,context);
 F(remesa_visible_readback_request,operation);F(remesa_visible_readback_request,offset);F(remesa_visible_readback_request,slot);
 F(remesa_visible_readback_request,generation);F(remesa_visible_readback_request,length);F(remesa_visible_readback_request,reserved);
 F(remesa_visible_readback_request,data);END;
 BEGIN(remesa_shader_request,0,REMESA_SHADER_ABI_VERSION);
 F(remesa_shader_request,version);F(remesa_shader_request,struct_bytes);F(remesa_shader_request,action);F(remesa_shader_request,flags);
 F(remesa_shader_request,owner);F(remesa_shader_request,generation);F(remesa_shader_request,byte_count);F(remesa_shader_request,resource_mask);
 F(remesa_shader_request,reserved);F(remesa_shader_request,resources);F(remesa_shader_request,data);END;
 BEGIN(remesa_lab_info,REMESA_LAB_INFO,REMESA_LAB_VERSION);
 F(remesa_lab_info,version);F(remesa_lab_info,features);F(remesa_lab_info,max_buffer_bytes);END;
 BEGIN(remesa_lab_alloc,REMESA_LAB_ALLOC,REMESA_LAB_VERSION);
 F(remesa_lab_alloc,bytes);F(remesa_lab_alloc,flags);F(remesa_lab_alloc,fd);END;
 puts("}}");return 0;
}
