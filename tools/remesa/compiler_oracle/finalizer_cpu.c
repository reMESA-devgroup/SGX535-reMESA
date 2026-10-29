/* SPDX-License-Identifier: MIT */
/* CPU-only replay of pinned original USP API; never a production dependency. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
extern void *PVRSRVAllocUserModeMem(uint32_t);
extern void PVRSRVFreeUserModeMem(void *);
extern size_t cpu_owned_size(void *);
extern int cpu_range_owned(const void *,size_t);
extern void cpu_allocation_check(void);
static void fail(const char *s){fprintf(stderr,"FAIL %s\n",s);exit(92);}
static void logmsg(const char *fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);}
static uint32_t u32(const void *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint16_t u16(const void *p){uint16_t v;memcpy(&v,p,2);return v;}
static void dump(const char *path,const void *p,size_t n){FILE *f=fopen(path,"wb");if(!f||fwrite(p,1,n,f)!=n||fclose(f))fail("dump");}
int main(int argc,char **argv)
{
 if(argc!=4)fail("args: library USP numeric-format");
 alarm(15);setvbuf(stdout,NULL,_IONBF,0);
 FILE *in=fopen(argv[2],"rb");if(!in||fseek(in,0,SEEK_END))fail("input");
 long len=ftell(in);if(len<40||len>1048576||fseek(in,0,SEEK_SET))fail("input bound");
 size_t page=(size_t)sysconf(_SC_PAGESIZE),cap=((size_t)len+page-1)/page*page;
 unsigned char *map=mmap(NULL,cap+page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if(map==MAP_FAILED)fail("mmap");
 unsigned char *blob=map+cap-len;
 if(fread(blob,1,(size_t)len,in)!=(size_t)len||fclose(in))fail("input read");
 if(u32(blob)!=0x55535020||u32(blob+4)!=31||u32(blob+8)!=(uint32_t)len-12||u32(blob+32)!=4||u32(blob+36)!=121)fail("USP prefix");
 if(mprotect(map,cap,PROT_READ)||mprotect(map+cap,page,PROT_NONE))fail("input protection");
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 11;}
 Dl_info info;if(!dladdr(dlsym(lib,"glCompileShader"),&info))fail("base");
 uintptr_t base=(uintptr_t)info.dli_fbase;
 #define FN(type,off) ((type)(base+(off)))
 void *(*create)(void *(*)(uint32_t),void (*)(void *),void (*)(const char *,...))=FN(__typeof__(create),0x3be80);
 void *(*parse)(void *,const void *)=FN(__typeof__(parse),0x3bef0);
 void (*setfmt)(void *,uint16_t,const void *,uint32_t,uint32_t)=FN(__typeof__(setfmt),0x3bf50);
 void (*setout)(void *,uint32_t)=FN(__typeof__(setout),0x3bfe0);
 int (*setmode)(void *,uint32_t)=FN(__typeof__(setmode),0x3c000);
 void *(*finish)(void *,void *)=FN(__typeof__(finish),0x3c020);
 void (*freehw)(void *,void *)=FN(__typeof__(freehw),0x3c1f0);
 void (*freeshader)(void *,void *)=FN(__typeof__(freeshader),0x3c210);
 void (*freectx)(void *)=FN(__typeof__(freectx),0x3c260);
 void *ctx=create(PVRSRVAllocUserModeMem,PVRSRVFreeUserModeMem,logmsg);if(!ctx)fail("context");
 void *shader=parse(ctx,blob);if(!shader)fail("parse");
 puts("PARSED core=4 rev=121");
 unsigned format=(unsigned)strtoul(argv[3],NULL,0);uintptr_t formatva;
 if(format==21)formatva=0x50e20;else if(format==20)formatva=0x50e60;else fail("format");
 for(unsigned i=0;i<16;i++)setfmt(ctx,(uint16_t)i,(void *)(base+formatva+12),0,0);
 setout(ctx,1);if(!setmode(ctx,0))fail("mode");
 void *hw=finish(ctx,shader);if(!hw)fail("finalize");
 size_t sz=cpu_owned_size(hw);if(sz<0xa0)fail("HW metadata bound");
 unsigned char *h=hw;uint16_t count=u16(h+12);size_t bytes=(size_t)count*8;
 void *code=(void *)(uintptr_t)u32(h+16);
 if(u32(h+4)!=4||u32(h+8)!=121)fail("HW target");
 if(!count||count>512||!cpu_range_owned(code,bytes)||
    (uintptr_t)code!=(uintptr_t)hw+0xa0||bytes>sz-0xa0)fail("CODE ownership");
 printf("FINALIZED format=%u allocation=%zu flags=%08x instructions=%u bytes=%zu temp24=%u input40=%u\n",format,sz,u32(h),count,bytes,u16(h+0x24),u16(h+0x40));
 dump("hw-allocation.bin",hw,sz);dump("raw-use-code.bin",code,bytes);
 uint16_t inputs=u16(h+0x40);void *inputptr=(void *)(uintptr_t)u32(h+0x44);
 if(inputs){if(inputs>128||!cpu_range_owned(inputptr,(size_t)inputs*28))fail("input metadata bounds");dump("input-records.bin",inputptr,(size_t)inputs*28);}
 uint32_t samples=u32(h+0x90);void *sampleptr=(void *)(uintptr_t)u32(h+0x94);
 if(samples){if(samples>16||!cpu_range_owned(sampleptr,(size_t)samples*28))fail("sample metadata bounds");dump("sample-records.bin",sampleptr,(size_t)samples*28);}
 /* Native full-mask/no-alpha/flags0 path appends NOP only if HW flag2.
  * These qualified cases must not require that branch. */
 if(u32(h)&2)fail("unqualified termination branch");
 unsigned char terminated[4096];memcpy(terminated,code,bytes);
 uint32_t last=u32(terminated+bytes-4)|(1u<<18);
 memcpy(terminated+bytes-4,&last,4);
 dump("native-terminated-use-code.bin",terminated,bytes);
 cpu_allocation_check();freehw(ctx,hw);freeshader(ctx,shader);freectx(ctx);cpu_allocation_check();
 if(munmap(map,cap+page)||dlclose(lib))fail("cleanup");
 return 0;
}
