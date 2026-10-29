/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
/* Controlled compiler oracle, no GL/EGL/device calls. ABI from REA.
 * Static native GLES2 defaults with overrides disabled; explicit caller-chosen
 * register budget and unwritten-word probe are NOT measured hardware capacity.
 * USP allocations are precompiled objects, not executable USSE images. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
size_t cpu_owned_size(void *);
void cpu_allocation_check(void);
struct guarded {uint32_t pre[4],words[1024],post[4];};
static void init_guard(struct guarded *g)
{memset(g,0,sizeof(*g));for(unsigned i=0;i<4;i++)g->pre[i]=g->post[i]=0x5a7ecafe;}
static void check_guard(struct guarded *g)
{for(unsigned i=0;i<4;i++)if(g->pre[i]!=0x5a7ecafe||g->post[i]!=0x5a7ecafe)_Exit(91);}
static uint32_t pointer(void *p){return (uint32_t)(uintptr_t)p;}
static void dump(const char *name,void *p)
{
 size_t size=cpu_owned_size(p);printf("OWNED %s bytes=%zu\n",name,size);
 if(!size||size>32u*1024*1024)return;
 FILE *f=fopen(name,"wb");if(!f)_Exit(92);
 if(fwrite(p,1,size,f)!=size||fclose(f))_Exit(93);
}
int main(int argc,char **argv)
{
 if(sizeof(void *)!=4||argc!=6)return 10;
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!lib){fprintf(stderr,"DLOPEN %s\n",dlerror());return 11;}
 int (*initialize)(void *)=(int(*)(void *))dlsym(lib,"GLSLInitCompiler");
 uint32_t *(*compile)(uint32_t *)=(uint32_t *(*)(uint32_t *))dlsym(lib,"GLSLCompileToUniflex");
 void (*release)(void *,void *)=(void(*)(void *,void *))dlsym(lib,"GLSLFreeCompiledUniflexProgram");
 int (*shutdown)(void *)=(int(*)(void *))dlsym(lib,"GLSLShutDownCompiler");
 if(!initialize||!compile||!release||!shutdown)return 12;
 struct guarded init,params,source,request,hardware;
 init_guard(&init);init_guard(&params);init_guard(&source);init_guard(&request);init_guard(&hardware);
 /* Observed native precision slots (init+8 block) and resource constants
  * (init+0x64), with application hint overrides disabled. */
 const uint32_t precision_slots[23]={3,3,1,0,2,1,3,3,0,3,0,0,2,3,2,2,3,0,0,0,0,0,0};
 memcpy(init.words+2,precision_slots,sizeof(precision_slots));
 const uint32_t limits[8]={8,8,1,8,8,128,8,64};
 memcpy(init.words+0x64/4,limits,sizeof(limits));
 init.words[0x88/4]=1;init.words[0x8c/4]=1;init.words[0x90/4]=10;
 init.words[0x94/4]=32;init.words[0x98/4]=1;init.words[0x9c/4]=1;init.words[0xa0/4]=50;
 printf("INIT %d\n",initialize(init.words));fflush(stdout);
 if(!init.words[1])return 13;
 const char *constant="#version 100\nprecision mediump float;\nvoid main(){gl_FragColor=vec4(1.0,0.0,0.0,1.0);}\n";
 const char *texture="#version 100\nprecision mediump float;\nvarying vec2 uv;\nuniform sampler2D image;\nvoid main(){gl_FragColor=texture2D(image,uv);}\n";
 const char *text=!strcmp(argv[2],"texture")?texture:constant;
 const char *texts[1]={text};
 source.words[0]=pointer(init.words);source.words[1]=pointer((void *)texts);
 source.words[2]=1;source.words[3]=1; /* source count1, original fragment selector1 */
 source.words[4]=1;source.words[7]=0x7fffffff;
 request.words[0]=atoi(argv[3])?2:1;
 request.words[1]=1; /* native backend flags2; bit0 -> IR only */
 /* Original GLES2 glCompileShader native fragment profile, REA glCompileShader.
  * Offset0x40 is dynamic in the driver; the argument is a CPU test budget. */
 params.words[0x08/4]=5;params.words[0x18/4]=9;params.words[0x1c/4]=0x77;
 params.words[0x20/4]=1;params.words[0x30/4]=2;params.words[0x34/4]=5;
 unsigned budget=(unsigned)strtoul(argv[4],NULL,0);if(!budget||budget>128)return 17;
 params.words[0x40/4]=budget;
 const size_t format_bytes=8u*40u;
 unsigned char *format_map=mmap(NULL,12288,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if(format_map==MAP_FAILED||mprotect(format_map+4096,4096,PROT_READ|PROT_WRITE))return 15;
 uint32_t *formats=(void *)(format_map+8192-format_bytes);
 memset(formats,0,format_bytes);
 for(unsigned i=0;i<8;i++){
  for(unsigned c=0;c<4;c++)formats[i*10+c]=1;
  formats[i*10+4]=0x60a;formats[i*10+6]=4;formats[i*10+7]=0xffffffff;
  formats[i*10+9]=(uint32_t)strtoul(argv[5],NULL,0); /* native writer leaves this word unwritten */
 }
 if(mprotect(format_map+4096,4096,PROT_READ))return 16;
 hardware.words[0]=pointer(params.words);hardware.words[1]=pointer(formats);
 request.words[2]=pointer(hardware.words);request.words[3]=pointer(source.words);
 uint32_t *compiled=compile(request.words);fflush(stdout);
 cpu_allocation_check();check_guard(&init);check_guard(&params);
 check_guard(&source);check_guard(&request);check_guard(&hardware);
 printf("TARGET core=%u rev=%u\n",params.words[0x58/4],params.words[0x5c/4]);
 size_t compiled_bytes=cpu_owned_size(compiled);
 printf("COMPILED owned=%zu\n",compiled_bytes);
 int success=0;
 if(compiled_bytes>=32){
  printf("RESULT stage=%u flags=%u success=%u\n",compiled[0],compiled[1],compiled[2]);
  success=compiled[2]!=0;
  char *log=(void *)(uintptr_t)compiled[3];size_t log_bytes=cpu_owned_size(log);
  if(log_bytes)printf("LOG %.*s\n",(int)(log_bytes<8192?log_bytes:8192),log);
  dump("compiled-allocation.bin",compiled);
  uint32_t *program=(void *)(uintptr_t)compiled[5];size_t program_bytes=cpu_owned_size(program);
  dump("uniflex-allocation.bin",program);
  if(program_bytes>=0xc0){
   dump("primary-output-allocation.bin",(void *)(uintptr_t)program[0x2e]);
   dump("alternate-output-allocation.bin",(void *)(uintptr_t)program[0x2f]);
  }
  release(init.words,compiled);
 }
 shutdown(init.words);cpu_allocation_check();check_guard(&init);check_guard(&params);
 munmap(format_map,12288);dlclose(lib);return success?0:20;
}
