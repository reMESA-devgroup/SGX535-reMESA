/* SPDX-License-Identifier: GPL-2.0-only
 * Strict one-profile LAB admission. Hashes identify complete prepared images;
 * retained auxiliary CODE is an explicit reference dependency, never an encoder.
 * Parent owns live GEM/PTE/power/exclusion/timeout validation.
 */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <crypto/hash.h>
#include "remesa_depth1280_profile.h"
#include "frozen_kernel_contract.h"
int remesa_depth1280_sha256(const void *p,size_t n,u8 digest[32])
{
 struct crypto_shash *tfm;struct shash_desc *d;int error;
 if(!p||!digest)return -EINVAL;
 tfm=crypto_alloc_shash("sha256",0,0);if(IS_ERR(tfm))return PTR_ERR(tfm);
 d=kmalloc(sizeof(*d)+crypto_shash_descsize(tfm),GFP_KERNEL);
 if(!d){crypto_free_shash(tfm);return -ENOMEM;}
 d->tfm=tfm;error=crypto_shash_digest(d,p,n,digest);
 kfree(d);crypto_free_shash(tfm);return error;
}
static const u64 va[11]={0x20000000ULL,0x20080000ULL,0x20028000ULL,0x30000000ULL,0x46000000ULL,0x40001000ULL,0x407d1000ULL,0x407ef000ULL,0x30100000ULL,0x427ef000ULL,0x403e9000ULL};
static const u64 size[11]={131072ULL,32768ULL,4096ULL,4096ULL,4096ULL,4096000ULL,122880ULL,33554432ULL,33554432ULL,4096ULL,4096000ULL};
static const u8 images[1][6][32]={{{82,114,235,134,166,232,22,184,170,179,152,147,141,156,84,112,150,254,177,189,33,39,160,205,192,175,216,70,232,199,146,79},{174,158,216,1,115,62,118,65,216,79,166,57,48,253,176,190,236,214,29,47,3,131,250,226,117,178,237,63,105,240,248,239},{247,221,212,49,47,127,138,59,97,75,25,58,217,191,153,208,76,148,81,164,35,10,235,172,28,17,231,39,235,60,174,37},{208,210,137,43,170,101,189,84,61,155,127,146,60,213,54,67,144,233,137,227,185,193,208,72,129,49,19,146,246,23,197,57},{114,126,69,67,20,15,160,44,57,92,92,209,25,68,6,188,121,85,97,219,1,245,206,37,175,45,17,50,157,0,250,101},{131,4,151,172,220,76,189,235,176,252,90,5,246,92,176,42,24,54,72,50,159,48,103,172,46,210,127,188,176,234,52,25}}};
int remesa_retained_depth1280_images(void *mmu_end,const struct remesa_memory_view *v,
 unsigned n,const void *commands,unsigned bytes)
{
 unsigned i,j,mode;u8 digest[32];int error;
 if(!mmu_end||!v||n!=11||!commands||bytes!=264||*(u64*)mmu_end!=0xe0000000ULL)return -EINVAL;
 for(i=0;i<n;i++){
  if(!v[i].cpu||!v[i].pages||!v[i].binding.id||v[i].binding.va!=va[i]||
     v[i].binding.bytes!=size[i]||v[i].page_count!=size[i]/4096)return -EINVAL;
  for(j=0;j<i;j++)if(v[j].binding.id==v[i].binding.id)return -EINVAL;
 }
 {
  struct sgx535_frozen_command_pairs decoded;
  struct sgx535_frozen_cpu_view control={v[4].cpu,v[4].binding.bytes};
  if(sgx535_frozen_extract_commands(&control,&decoded)||
     memcmp(commands,&decoded,sizeof(decoded)))return -EINVAL;
 }
 error=remesa_depth1280_sha256(v[2].cpu,size[2],digest);if(error)return error;
 for(mode=0;mode<1;mode++)if(!memcmp(digest,images[mode][2],32))break;
 if(mode==1)return -EKEYREJECTED;
 for(i=0;i<5;i++)if(i!=2){
  error=remesa_depth1280_sha256(v[i].cpu,size[i],digest);if(error)return error;
  if(memcmp(digest,images[mode][i],32))return -EKEYREJECTED;
 }
 /* Qualified reference starts COLOR entirely zero, independently checked. */
 if(memchr_inv(v[5].cpu,0,size[5]))return -EKEYREJECTED;
 for(i=6;i<n;i++)if(memchr_inv(v[i].cpu,0,size[i]))return -EKEYREJECTED;
 return 0;
}
