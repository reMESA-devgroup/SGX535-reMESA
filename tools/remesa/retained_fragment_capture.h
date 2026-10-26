/* SPDX-License-Identifier: MIT */
#ifndef REMESA_RETAINED_FRAGMENT_CAPTURE_H
#define REMESA_RETAINED_FRAGMENT_CAPTURE_H
#include "remesa_linux.h"
#include "remesa_linux_memory.h"
/* Serialized, single-attempt laboratory caller adapter. No default auth and
 * no endpoint access from authenticate. The external controller must protect
 * the directory, helper, bindings, process and observer capture interval. */
struct remesa_fragment_recorder {
 const char *directory,*authenticator,*binding;
 int memory_fd,retained_fd;
 uint64_t records,submit_begin,submit_end;
 unsigned retained_records;
 bool failed,sealed;
};
int remesa_fragment_record_memory(void *,const struct remesa_memory_request *,
 const struct remesa_memory_request *,int,int,bool);
int remesa_fragment_record_retained(void *,const struct remesa_retained_request *,
 const struct remesa_retained_request *,int,int);
int remesa_fragment_authenticate(void *,const struct remesa_retained_request *,
 const struct remesa_linux_fragment_capture *,struct remesa_linux_fragment_evidence *);
int remesa_fragment_auth_preflight(struct remesa_fragment_recorder *);
int remesa_fragment_save(const char *,const char *,const void *,size_t);
int remesa_fragment_copy(const char *,const char *,const char *);
int remesa_fragment_load(const char *,const char *,void *,size_t);
int remesa_fragment_evidence_decode(const unsigned char *,size_t,
 struct remesa_linux_fragment_evidence *);
#endif
