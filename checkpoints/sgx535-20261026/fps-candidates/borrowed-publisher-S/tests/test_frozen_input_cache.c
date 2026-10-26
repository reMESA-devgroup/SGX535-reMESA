#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
typedef uint8_t u8;typedef uint32_t u32;typedef uint64_t u64;
#include "../module/remesa_stream_trusted.h"
#include "../module/remesa_gem_memory.h"
#include "../module/remesa_stream_grammar.h"
#include "../module/stream_profile_asset.h"
#include "../sha256.h"
#include "../module/remesa_canonical_inputs.h"
#include "../module/remesa_pds_write_guard.h"
#ifndef EKEYREJECTED
#define EKEYREJECTED 129
#endif
#define SGX535_BO_PDS 0u
#define SGX535_BO_VERTEX_TA 2u
static struct remesa_stream_profile profile;
static unsigned provider_checks,hash_calls;static bool provider_fault;
struct trusted_operation {struct remesa_memory_ticket ticket;struct remesa_memory_snapshot snapshot;struct remesa_memory_view views[11];u8 image_digest[32];bool started,inputs_authenticated;unsigned input_check_calls,input_hash_passes,input_byte_passes;};
struct trusted_stream {struct remesa_gem_memory*memory;bool fast_blake3,fast_nohash,inputs_sealed;};
static void*vmalloc(size_t n){return malloc(n);}static void vfree(void*p){free(p);}
int remesa_depth1280_sha256(const void*p,size_t n,u8 out[32]){hash_calls++;sha256(p,n,out);return 0;}
int remesa_gem_job_check_snapshot(struct remesa_gem_memory*s,const struct remesa_memory_snapshot*t,const struct remesa_memory_view*v,unsigned n){(void)s;assert(t&&v&&n==11);provider_checks++;return provider_fault?-EIO:0;}
int remesa_gem_job_check_snapshot_locked(struct remesa_gem_memory*s,const struct remesa_memory_snapshot*t,const struct remesa_memory_view*v,unsigned n){return remesa_gem_job_check_snapshot(s,t,v,n);}
static int guarded_pds_write(u64 off,const void*p,unsigned n){return remesa_canonical_pds_page(profile.batches,profile.count,remesa_canonical_inputs[0],off,p,n);}
static unsigned seal_calls;
int remesa_gem_stream_seal_inputs(struct remesa_gem_memory*s,struct remesa_memory_ticket t,int(*g)(u64,const void*,unsigned)){(void)s;(void)t;assert(g);seal_calls++;return 0;}
#include "check_images_actual.inc"
static void initialize(struct trusted_operation*o,unsigned char*images[5]){memset(o,0,sizeof(*o));o->snapshot.mmu_end=0xe0000000ULL;o->snapshot.commands=&profile.commands;o->snapshot.command_bytes=264;for(unsigned i=0;i<11;i++){o->views[i].binding.va=profile.bindings[i].va;o->views[i].binding.bytes=profile.bindings[i].bytes;if(i<5)o->views[i].cpu=images[i];}}
int main(int argc,char**argv){assert(argc==2);profile=gears_stream_profile_asset;unsigned char*images[5];char path[4096];for(unsigned i=0;i<5;i++){size_t n=profile.bindings[i].bytes;images[i]=malloc(n);assert(images[i]);snprintf(path,sizeof(path),"%s/role%u.bin",argv[1],i);FILE*f=fopen(path,"rb");assert(f&&fread(images[i],1,n,f)==n&&!fclose(f));}
 struct trusted_stream s={.fast_blake3=true};struct trusted_operation o;initialize(&o,images);for(unsigned i=0;i<4;i++)assert(!check_images(&s,&o,i==3));assert(provider_checks==4&&hash_calls==7&&o.input_check_calls==4&&o.input_hash_passes==1&&o.inputs_authenticated);
 unsigned oldhash=hash_calls;provider_fault=true;assert(check_images(&s,&o,false)==-EIO&&hash_calls==oldhash);provider_fault=false;
 o.snapshot.command_bytes=260;assert(check_images(&s,&o,false)==-EKEYREJECTED&&hash_calls==oldhash);o.snapshot.command_bytes=264;
 o.views[0].binding.va+=4096;assert(check_images(&s,&o,false)==-EKEYREJECTED&&hash_calls==oldhash);o.views[0].binding.va-=4096;
 o.started=true;assert(check_images(&s,&o,false)==-EBUSY&&hash_calls==oldhash);
 initialize(&o,images);images[1][0]^=1;assert(check_images(&s,&o,false)==-EKEYREJECTED&&!o.inputs_authenticated);images[1][0]^=1;
 initialize(&o,images);s.fast_blake3=false;oldhash=hash_calls;unsigned oldprovider=provider_checks;for(unsigned i=0;i<4;i++)assert(!check_images(&s,&o,i==3));assert(hash_calls-oldhash==28&&provider_checks-oldprovider==4&&o.input_hash_passes==4);
 for(unsigned i=0;i<5;i++)free(images[i]);puts("PASS: FAST4providerchecks/1full7hashpass, LAB4full28hashes; cachedPTE/command/addressfaultsreject; post-startreuseforbidden; newopCODEtamperrejected; actualSHA+grammar, fakeprovider, GPU0");return 0;}
